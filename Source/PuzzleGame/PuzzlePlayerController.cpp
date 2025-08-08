// Fill out your copyright notice in the Description page of Project Settings.


#include "PuzzlePlayerController.h"
#include "PuzzlePiece.h"
#include "PuzzleGameMode.h"
#include "PuzzleMainWidget.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/EngineTypes.h"
#include "CollisionQueryParams.h"
#include "Framework/Application/NavigationConfig.h"
#include "GameFramework/InputSettings.h"

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

    
    // First, clean up any existing widgets that might have been created by Blueprint
    if (MainWidgetClass)
    {
        TArray<UUserWidget*> ExistingWidgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), ExistingWidgets, MainWidgetClass, false);
        
        
        for (UUserWidget* Widget : ExistingWidgets)
        {
            if (Widget)
            {
                Widget->RemoveFromParent();
            }
        }
    }
    
    // Also check for any UserWidget that might be our MainWidget
    TArray<UUserWidget*> AllWidgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), AllWidgets, UUserWidget::StaticClass(), false);
    
    
    for (UUserWidget* Widget : AllWidgets)
    {
        if (Widget)
        {
        }
    }
    
    // Show main widget if available
    if (MainWidgetClass)
    {
        ShowMainWidget();
    }
    else
    {
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
    

    if (CurrentInteractionState == EMouseInteractionState::None)
    {
        // Check if we clicked a puzzle piece
        APuzzlePiece* ClickedPiece = GetPuzzlePieceUnderMouse();

        if (ClickedPiece)
        {
            // Start dragging the piece
            StartDragPiece(ClickedPiece);
        }
        else
        {
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
    
    if (CurrentInteractionState != EMouseInteractionState::None)
    {
        return; // Already in an interaction
    }

    // Get world location under mouse
    FVector PieceSpawnLocation = GetMouseWorldLocation();
    PieceSpawnLocation.Z = 0.0f; // Keep at ground level
    

    // Spawn new puzzle piece at mouse location
    if (CachedGameMode)
    {
        
        APuzzlePiece* NewPiece = CachedGameMode->SpawnPuzzlePiece(PieceID, PieceSpawnLocation);
        if (NewPiece)
        {
            
            // Set state to DraggingFromUI before starting drag
            CurrentInteractionState = EMouseInteractionState::DraggingFromUI;
            
            // Set drag offset to zero for UI spawned pieces
            DragOffset = FVector::ZeroVector;
            
            StartDragPiece(NewPiece);

            
            // Refresh UI to remove the placed piece
            if (MainWidget)
            {
                if (UPuzzleMainWidget* PuzzleWidget = Cast<UPuzzleMainWidget>(MainWidget))
                {
                    PuzzleWidget->RefreshPieceList();
                }
                else
                {
                }
            }
            else
            {
            }
        }
        else
        {
        }
    }
    else
    {
    }
}

void APuzzlePlayerController::StartDragPiece(APuzzlePiece* Piece)
{
    if (!Piece)
    {
        return;
    }
    
    if (CurrentInteractionState != EMouseInteractionState::None && CurrentInteractionState != EMouseInteractionState::DraggingFromUI)
    {
        return;
    }

    SelectedPiece = Piece;
    bIsDragging = true;

    // Set piece as selected
    SelectedPiece->SetSelected(true);

    // For UI spawned pieces, always use zero offset
    if (CurrentInteractionState == EMouseInteractionState::DraggingFromUI)
    {
        DragOffset = FVector::ZeroVector;
        // For UI spawned pieces, set an invalid drag start location to indicate it's new
        DragStartLocation = FVector(-9999, -9999, -9999); // Invalid location
        CurrentInteractionState = EMouseInteractionState::DraggingPiece; // Change to regular dragging
    }
    else
    {
        // For pieces already in scene, store their current grid position
        DragStartLocation = SelectedPiece->GetActorLocation();
        
        // Calculate proper offset
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
            
            // Check if this is a new piece from UI (StartGridID would be -1)
            bool bIsNewPieceFromUI = (StartGridID < 0);
            
            if (TargetGridID >= 0 && TargetGridID != StartGridID)
            {
                // Check if target position is occupied
                APuzzlePiece* OccupyingPiece = CachedGameMode->GetPieceAtGridID(TargetGridID);
                
                if (OccupyingPiece)
                {
                    // Target is occupied - swap with it
                    CachedGameMode->SwapPiecesAtGridIDs(StartGridID, TargetGridID);
                    // Only increment move count for actual swaps (not initial placement)
                    if (!bIsNewPieceFromUI)
                    {
                        CachedGameMode->IncrementMoveCount();
                    }
                }
                else
                {
                    // Target is empty - just move there
                    FVector GridPosition = CachedGameMode->GetGridPositionFromID(TargetGridID);
                    SelectedPiece->MovePieceToLocation(GridPosition, false);
                    
                    // Update grid occupancy
                    CachedGameMode->UpdateGridOccupancy(TargetGridID, SelectedPiece);
                    
                    // Only increment move count if this is not the initial placement from UI
                    if (!bIsNewPieceFromUI)
                    {
                        CachedGameMode->IncrementMoveCount();
                    }
                }
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
    static UUserWidget* GlobalMainWidget = nullptr;
    
    // First, find and remove ALL existing MainWidgets
    TArray<UUserWidget*> FoundWidgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), FoundWidgets, MainWidgetClass, false);
    
    
    // Remove all existing widgets first
    for (UUserWidget* Widget : FoundWidgets)
    {
        if (Widget && Widget->IsInViewport())
        {
            Widget->RemoveFromParent();
        }
    }
    
    // Clear our reference if it was removed
    if (MainWidget && (!IsValid(MainWidget) || !MainWidget->IsInViewport()))
    {
        MainWidget = nullptr;
    }
    
    // Check if another instance already created the widget
    if (GlobalMainWidget && GlobalMainWidget != MainWidget)
    {
        // Validate the pointer is still valid
        if (IsValid(GlobalMainWidget))
        {
            // Remove the duplicate
            if (GlobalMainWidget->IsInViewport())
            {
                GlobalMainWidget->RemoveFromParent();
            }
            
            if (!MainWidget)
            {
                MainWidget = GlobalMainWidget;
            }
        }
        else
        {
            // Invalid pointer, clear it
            GlobalMainWidget = nullptr;
        }
    }
    
    if (MainWidgetClass && !MainWidget)
    {
        MainWidget = CreateWidget<UUserWidget>(this, MainWidgetClass);
        
        if (MainWidget)
        {
            GlobalMainWidget = MainWidget;
        }
    }

    if (MainWidget && IsValid(MainWidget))
    {
        if (!MainWidget->IsInViewport())
        {
            MainWidget->AddToViewport();
        }
        
        // Defer input mode setting to next frame to ensure everything is initialized
        GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
        {
            if (IsValid(this) && IsValid(MainWidget))
            {
                // Set input mode to allow both game and UI
                FInputModeGameAndUI InputMode;
                InputMode.SetWidgetToFocus(nullptr); // Don't focus on widget
                InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
                InputMode.SetHideCursorDuringCapture(false);
                SetInputMode(InputMode);
            }
        });
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
        CachedGameMode->DebugPuzzleState();
    }
    else
    {
    }
}

void APuzzlePlayerController::CheckPuzzleComplete()
{
    if (CachedGameMode)
    {
        CachedGameMode->ForceCheckGameCompletion();
    }
    else
    {
    }
}