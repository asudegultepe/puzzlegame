// Fill out your copyright notice in the Description page of Project Settings.


#include "PuzzlePlayerController.h"
#include "PuzzlePiece.h"
#include "PuzzleGameMode.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/EngineTypes.h"
#include "CollisionQueryParams.h"

APuzzlePlayerController::APuzzlePlayerController()
{
    PrimaryActorTick.bCanEverTick = true;

    // Input settings
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;

    // Default values
    CurrentInteractionState = EMouseInteractionState::None;
    SelectedPiece = nullptr;
    MainWidget = nullptr;

    // Drag settings
    DragHeight = 50.0f;
    DragSmoothness = 20.0f; // Increased for more responsive dragging

    // Trace settings
    TraceDistance = 10000.0f;
    TraceChannel = ECC_Visibility; // Changed from WorldStatic to Visibility for better detection

    // Internal state
    bIsDragging = false;
    bMousePressed = false;
    CachedGameMode = nullptr;

    CurrentMousePosition = FVector2D::ZeroVector;
    MouseWorldPosition = FVector::ZeroVector;
    MouseWorldDirection = FVector::ZeroVector;
    DragStartLocation = FVector::ZeroVector;
    DragOffset = FVector::ZeroVector;
}

void APuzzlePlayerController::BeginPlay()
{
    Super::BeginPlay();

    // Setup Enhanced Input
    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        if (DefaultMappingContext)
        {
            Subsystem->AddMappingContext(DefaultMappingContext, 0);
        }
    }

    // Cache game mode reference
    CachedGameMode = GetPuzzleGameMode();

    // Debug log to verify this is the correct controller
    UE_LOG(LogTemp, Warning, TEXT("APuzzlePlayerController::BeginPlay - Controller Class: %s"), *GetClass()->GetName());
    
    // Show main widget if available
    if (MainWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("MainWidgetClass is set, calling ShowMainWidget"));
        ShowMainWidget();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("MainWidgetClass is NULL! Set it in BP_PuzzlePlayerController"));
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Blue, TEXT("PuzzlePlayerController initialized"));
    }
}

void APuzzlePlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    // Enhanced Input System bindings
    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
    {
        // Left Click - Press and Release
        if (LeftClickAction)
        {
            EnhancedInputComponent->BindAction(LeftClickAction, ETriggerEvent::Started, this, &APuzzlePlayerController::OnLeftClickPressed);
            EnhancedInputComponent->BindAction(LeftClickAction, ETriggerEvent::Completed, this, &APuzzlePlayerController::OnLeftClickReleased);
        }

        // Right Click
        if (RightClickAction)
        {
            EnhancedInputComponent->BindAction(RightClickAction, ETriggerEvent::Started, this, &APuzzlePlayerController::OnRightClickPressed);
        }

        // Toggle UI
        if (ToggleUIAction)
        {
            EnhancedInputComponent->BindAction(ToggleUIAction, ETriggerEvent::Started, this, &APuzzlePlayerController::OnToggleUI);
        }
    }
}

void APuzzlePlayerController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Update mouse position every frame
    UpdateMousePosition();

    // Handle drag updates if dragging
    if (bIsDragging)
    {
        HandleDragUpdate();
    }
}

void APuzzlePlayerController::OnLeftClickPressed(const FInputActionValue& Value)
{
    bMousePressed = true;
    
    UE_LOG(LogTemp, Warning, TEXT("Left click pressed. Current state: %d"), (int32)CurrentInteractionState);

    if (CurrentInteractionState == EMouseInteractionState::None)
    {
        // Check if we clicked a puzzle piece
        APuzzlePiece* ClickedPiece = GetPuzzlePieceUnderMouse();

        if (ClickedPiece)
        {
            UE_LOG(LogTemp, Warning, TEXT("Clicked on piece %d"), ClickedPiece->GetPieceID());
            // Start dragging the piece
            StartDragPiece(ClickedPiece);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("No piece under mouse"));
        }
    }
}

void APuzzlePlayerController::OnLeftClickReleased(const FInputActionValue& Value)
{
    bMousePressed = false;

    if (bIsDragging)
    {
        EndDrag();
    }
}

void APuzzlePlayerController::OnRightClickPressed(const FInputActionValue& Value)
{
    // Right click to deselect or cancel drag
    if (CurrentInteractionState != EMouseInteractionState::None)
    {
        EndDrag();
    }
}

void APuzzlePlayerController::OnToggleUI(const FInputActionValue& Value)
{
    ToggleMainWidget();
}

void APuzzlePlayerController::StartDragFromUI(int32 PieceID)
{
    UE_LOG(LogTemp, Warning, TEXT("StartDragFromUI called with PieceID: %d"), PieceID);
    
    if (CurrentInteractionState != EMouseInteractionState::None)
    {
        UE_LOG(LogTemp, Warning, TEXT("Already in interaction state: %d"), (int32)CurrentInteractionState);
        return; // Already in an interaction
    }

    // Get world location under mouse
    FVector PieceSpawnLocation = GetMouseWorldLocation();
    PieceSpawnLocation.Z = 0.0f; // Keep at ground level
    
    UE_LOG(LogTemp, Warning, TEXT("Spawn location: %s"), *PieceSpawnLocation.ToString());

    // Spawn new puzzle piece at mouse location
    if (CachedGameMode)
    {
        UE_LOG(LogTemp, Warning, TEXT("CachedGameMode exists, calling SpawnPuzzlePiece"));
        
        APuzzlePiece* NewPiece = CachedGameMode->SpawnPuzzlePiece(PieceID, PieceSpawnLocation);
        if (NewPiece)
        {
            UE_LOG(LogTemp, Warning, TEXT("Piece spawned successfully"));
            
            // Set state to DraggingFromUI before starting drag
            CurrentInteractionState = EMouseInteractionState::DraggingFromUI;
            
            // Set drag offset to zero for UI spawned pieces
            DragOffset = FVector::ZeroVector;
            
            StartDragPiece(NewPiece);

            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
                    FString::Printf(TEXT("Spawned piece %d from UI"), PieceID));
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("SpawnPuzzlePiece returned null for PieceID %d!"), PieceID);
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red,
                    FString::Printf(TEXT("Failed to spawn piece %d - may already exist"), PieceID));
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("CachedGameMode is null!"));
    }
}

void APuzzlePlayerController::StartDragPiece(APuzzlePiece* Piece)
{
    if (!Piece)
    {
        UE_LOG(LogTemp, Warning, TEXT("StartDragPiece: Piece is null"));
        return;
    }
    
    if (CurrentInteractionState != EMouseInteractionState::None && CurrentInteractionState != EMouseInteractionState::DraggingFromUI)
    {
        UE_LOG(LogTemp, Warning, TEXT("StartDragPiece: Already in state %d"), (int32)CurrentInteractionState);
        return;
    }

    SelectedPiece = Piece;
    bIsDragging = true;

    // Set piece as selected
    SelectedPiece->SetSelected(true);

    // Calculate drag offset
    DragStartLocation = SelectedPiece->GetActorLocation();
    
    // For UI spawned pieces, always use zero offset
    if (CurrentInteractionState == EMouseInteractionState::DraggingFromUI)
    {
        DragOffset = FVector::ZeroVector;
        CurrentInteractionState = EMouseInteractionState::DraggingPiece; // Change to regular dragging
    }
    else
    {
        // For pieces already in scene, calculate proper offset
        FVector MouseWorld = GetMouseWorldLocation();
        DragOffset = DragStartLocation - MouseWorld;
        CurrentInteractionState = EMouseInteractionState::DraggingPiece;
    }

    // Fire events
    OnPieceSelected(SelectedPiece);
    OnDragStarted(SelectedPiece);
    
    // Switch to game-only input mode during drag for free mouse movement
    FInputModeGameOnly GameOnlyMode;
    SetInputMode(GameOnlyMode);
    bShowMouseCursor = true; // Keep cursor visible

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow,
            FString::Printf(TEXT("Started dragging piece %d"), SelectedPiece->GetPieceID()));
    }
}

void APuzzlePlayerController::EndDrag()
{
    if (!bIsDragging || !SelectedPiece)
    {
        return;
    }

    // Get the drop location
    FVector DropLocation = GetMouseWorldLocation();
    
    // Check what's under the mouse using trace
    FHitResult HitResult;
    APuzzlePiece* TargetPiece = nullptr;
    
    if (TraceUnderMouse(HitResult))
    {
        TargetPiece = Cast<APuzzlePiece>(HitResult.GetActor());
    }
    
    if (TargetPiece && TargetPiece != SelectedPiece)
    {
        // Swap pieces using the new grid occupancy system
        UE_LOG(LogTemp, Warning, TEXT("Swapping pieces: %d with %d"), SelectedPiece->GetPieceID(), TargetPiece->GetPieceID());
        
        if (CachedGameMode)
        {
            // Get current grid positions of both pieces
            int32 SelectedGridID = CachedGameMode->GetGridIDFromPosition(SelectedPiece->GetActorLocation());
            int32 TargetGridID = CachedGameMode->GetGridIDFromPosition(TargetPiece->GetActorLocation());
            
            if (SelectedGridID >= 0 && TargetGridID >= 0)
            {
                // Use the GameMode's swap function
                CachedGameMode->SwapPiecesAtGridIDs(SelectedGridID, TargetGridID);
                
                // Increment move count
                CachedGameMode->IncrementMoveCount();
            }
        }

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, TEXT("Swapped pieces!"));
        }
    }
    else
    {
        // Drop at current location with grid snapping
        if (CachedGameMode)
        {
            // Get the starting grid ID
            int32 StartGridID = CachedGameMode->GetGridIDFromPosition(DragStartLocation);
            
            // Get the target grid ID for the drop location
            FVector CurrentPieceLocation = SelectedPiece->GetActorLocation();
            int32 TargetGridID = CachedGameMode->GetGridIDFromPosition(CurrentPieceLocation);
            
            if (TargetGridID >= 0 && TargetGridID != StartGridID)
            {
                // Check if target position is occupied
                APuzzlePiece* OccupyingPiece = CachedGameMode->GetPieceAtGridID(TargetGridID);
                
                if (OccupyingPiece)
                {
                    // Target is occupied - swap with it
                    CachedGameMode->SwapPiecesAtGridIDs(StartGridID, TargetGridID);
                }
                else
                {
                    // Target is empty - just move there
                    FVector GridPosition = CachedGameMode->GetGridPositionFromID(TargetGridID);
                    SelectedPiece->MovePieceToLocation(GridPosition, false);
                    
                    // Update grid occupancy
                    CachedGameMode->UpdateGridOccupancy(TargetGridID, SelectedPiece);
                }
                
                // Increment move count since we moved
                CachedGameMode->IncrementMoveCount();
            }
            else if (TargetGridID == StartGridID)
            {
                // Dropped at same position - just snap back
                FVector GridPosition = CachedGameMode->GetGridPositionFromID(TargetGridID);
                SelectedPiece->MovePieceToLocation(GridPosition, false);
            }
        }
    }

    // Clean up drag state
    SelectedPiece->SetSelected(false);
    OnPieceDeselected(SelectedPiece);
    OnDragEnded(SelectedPiece);

    SelectedPiece = nullptr;
    CurrentInteractionState = EMouseInteractionState::None;
    bIsDragging = false;
    DragOffset = FVector::ZeroVector; // Reset drag offset
    
    // Restore input mode to game and UI
    if (MainWidget && MainWidget->IsInViewport())
    {
        FInputModeGameAndUI InputMode;
        InputMode.SetWidgetToFocus(nullptr);
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        InputMode.SetHideCursorDuringCapture(false);
        SetInputMode(InputMode);
    }

    UE_LOG(LogTemp, Warning, TEXT("Drag ended. State reset to None"));
    
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange, TEXT("Drag ended - State reset"));
    }
}

void APuzzlePlayerController::UpdateDragPosition()
{
    if (!bIsDragging || !SelectedPiece)
    {
        return;
    }

    // Get target position 
    FVector MouseLocation = GetMouseWorldLocation();
    FVector TargetLocation = MouseLocation + DragOffset;
    TargetLocation.Z = DragHeight; // Keep at drag height

    // Direct set location during drag - no interpolation for immediate response
    SelectedPiece->SetActorLocation(TargetLocation);
    
    // Draw snap preview
    if (CachedGameMode)
    {
        int32 GridID = CachedGameMode->GetGridIDFromPosition(TargetLocation);
        if (GridID >= 0)
        {
            FVector SnapPosition = CachedGameMode->GetGridPositionFromID(GridID);
            DrawDebugBox(GetWorld(), SnapPosition + FVector(0, 0, 5), FVector(40, 40, 2), FColor::Green, false, 0.1f, 0, 3.0f);
        }
    }
}

bool APuzzlePlayerController::TraceUnderMouse(FHitResult& HitResult)
{
    // Get mouse position in world space
    FVector WorldLocation, WorldDirection;
    if (!DeprojectMousePositionToWorld(WorldLocation, WorldDirection))
    {
        return false;
    }

    // Perform line trace
    FVector Start = WorldLocation;
    FVector End = Start + (WorldDirection * TraceDistance);

    FCollisionQueryParams QueryParams;
    QueryParams.bTraceComplex = false;
    QueryParams.bReturnPhysicalMaterial = false;
    QueryParams.AddIgnoredActor(GetPawn());
    
    // If we're dragging a piece, ignore it in the trace
    if (bIsDragging && SelectedPiece)
    {
        QueryParams.AddIgnoredActor(SelectedPiece);
    }

    bool bHit = GetWorld()->LineTraceSingleByChannel(
        HitResult,
        Start,
        End,
        TraceChannel,
        QueryParams
    );

    // Debug draw the trace line
#if WITH_EDITOR
    if (bHit)
    {
        DrawDebugLine(GetWorld(), Start, HitResult.Location, FColor::Green, false, 0.1f, 0, 1.0f);
        DrawDebugSphere(GetWorld(), HitResult.Location, 5.0f, 8, FColor::Red, false, 0.1f);
    }
    else
    {
        DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, 0.1f, 0, 1.0f);
    }
#endif

    return bHit;
}

FVector APuzzlePlayerController::GetMouseWorldLocation()
{
    // Always project to a plane at Z=0 for consistent behavior
    FVector WorldLocation, WorldDirection;
    if (DeprojectMousePositionToWorld(WorldLocation, WorldDirection))
    {
        // Find intersection with Z=0 plane
        if (FMath::Abs(WorldDirection.Z) > 0.0001f) // Avoid division by zero
        {
            float t = -WorldLocation.Z / WorldDirection.Z;
            FVector PlaneLocation = WorldLocation + (WorldDirection * t);
            return PlaneLocation;
        }
    }

    // Fallback: try trace
    FHitResult HitResult;
    if (TraceUnderMouse(HitResult))
    {
        FVector TraceLocation = HitResult.Location;
        TraceLocation.Z = 0.0f;
        return TraceLocation;
    }

    return FVector::ZeroVector;
}

APuzzlePiece* APuzzlePlayerController::GetPuzzlePieceUnderMouse()
{
    FHitResult HitResult;
    if (TraceUnderMouse(HitResult))
    {
        AActor* HitActor = HitResult.GetActor();
        if (HitActor)
        {
            UE_LOG(LogTemp, Verbose, TEXT("Hit actor: %s"), *HitActor->GetClass()->GetName());
            APuzzlePiece* Piece = Cast<APuzzlePiece>(HitActor);
            if (Piece)
            {
                return Piece;
            }
        }
    }

    return nullptr;
}

void APuzzlePlayerController::ShowMainWidget()
{
    if (MainWidgetClass && !MainWidget)
    {
        MainWidget = CreateWidget<UUserWidget>(this, MainWidgetClass);
        
        if (MainWidget)
        {
            UE_LOG(LogTemp, Warning, TEXT("Created MainWidget with owner: %s"), *GetClass()->GetName());
        }
    }

    if (MainWidget)
    {
        MainWidget->AddToViewport();
        
        // Set input mode to allow both game and UI
        FInputModeGameAndUI InputMode;
        InputMode.SetWidgetToFocus(nullptr); // Don't focus on widget
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        InputMode.SetHideCursorDuringCapture(false);
        SetInputMode(InputMode);
    }
}

void APuzzlePlayerController::HideMainWidget()
{
    if (MainWidget)
    {
        MainWidget->RemoveFromParent();
    }
}

void APuzzlePlayerController::ToggleMainWidget()
{
    if (MainWidget && MainWidget->IsInViewport())
    {
        HideMainWidget();
    }
    else
    {
        ShowMainWidget();
    }
}

void APuzzlePlayerController::UpdateMousePosition()
{
    // Get mouse position on viewport
    GetMousePosition(CurrentMousePosition.X, CurrentMousePosition.Y);

    // Update world position and direction
    FVector WorldLocation, WorldDirection;
    if (DeprojectMousePositionToWorld(WorldLocation, WorldDirection))
    {
        MouseWorldPosition = WorldLocation;
        MouseWorldDirection = WorldDirection;
    }
}

void APuzzlePlayerController::HandlePieceSelection()
{
    // This function can be expanded for more complex selection logic
    if (CurrentInteractionState == EMouseInteractionState::None)
    {
        APuzzlePiece* HoveredPiece = GetPuzzlePieceUnderMouse();

        // Handle hover states, highlighting, etc.
        if (HoveredPiece)
        {
            CurrentInteractionState = EMouseInteractionState::Hovering;
        }
    }
}

void APuzzlePlayerController::HandleDragUpdate()
{
    if (CurrentInteractionState == EMouseInteractionState::DraggingPiece)
    {
        UpdateDragPosition();
    }
    else if (bIsDragging)
    {
        UE_LOG(LogTemp, Warning, TEXT("HandleDragUpdate: bIsDragging is true but state is %d"), (int32)CurrentInteractionState);
    }
}

APuzzleGameMode* APuzzlePlayerController::GetPuzzleGameMode()
{
    if (!CachedGameMode)
    {
        CachedGameMode = Cast<APuzzleGameMode>(UGameplayStatics::GetGameMode(this));
    }

    return CachedGameMode;
}

void APuzzlePlayerController::DebugPuzzle()
{
    if (CachedGameMode)
    {
        UE_LOG(LogTemp, Warning, TEXT("DebugPuzzle command executed"));
        CachedGameMode->DebugPuzzleState();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("DebugPuzzle: No GameMode found!"));
    }
}

void APuzzlePlayerController::CheckPuzzleComplete()
{
    if (CachedGameMode)
    {
        UE_LOG(LogTemp, Warning, TEXT("CheckPuzzleComplete command executed"));
        CachedGameMode->ForceCheckGameCompletion();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("CheckPuzzleComplete: No GameMode found!"));
    }
}