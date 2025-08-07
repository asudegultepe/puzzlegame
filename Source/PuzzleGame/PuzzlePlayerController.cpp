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
    DragSmoothness = 10.0f;

    // Trace settings
    TraceDistance = 10000.0f;
    TraceChannel = ECC_WorldStatic;

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

    // Show main widget if available
    if (MainWidgetClass)
    {
        ShowMainWidget();
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

    if (CurrentInteractionState == EMouseInteractionState::None)
    {
        // Check if we clicked a puzzle piece
        APuzzlePiece* ClickedPiece = GetPuzzlePieceUnderMouse();

        if (ClickedPiece)
        {
            // Start dragging the piece
            StartDragPiece(ClickedPiece);
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
    PieceSpawnLocation.Z += DragHeight; // Spawn above ground level

    // Spawn new puzzle piece at mouse location
    if (CachedGameMode)
    {
        APuzzlePiece* NewPiece = CachedGameMode->SpawnPuzzlePiece(PieceID, PieceSpawnLocation);
        if (NewPiece)
        {
            StartDragPiece(NewPiece);

            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
                    FString::Printf(TEXT("Spawned piece %d from UI"), PieceID));
            }
        }
    }
}

void APuzzlePlayerController::StartDragPiece(APuzzlePiece* Piece)
{
    if (!Piece || CurrentInteractionState != EMouseInteractionState::None)
    {
        return;
    }

    SelectedPiece = Piece;
    CurrentInteractionState = EMouseInteractionState::DraggingPiece;
    bIsDragging = true;

    // Set piece as selected
    SelectedPiece->SetSelected(true);

    // Calculate drag offset
    DragStartLocation = SelectedPiece->GetActorLocation();
    FVector MouseWorld = GetMouseWorldLocation();
    DragOffset = DragStartLocation - MouseWorld;

    // Fire events
    OnPieceSelected(SelectedPiece);
    OnDragStarted(SelectedPiece);

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
    
    // Find if we're dropping near another piece
    APuzzlePiece* TargetPiece = nullptr;
    float MinDistance = 1000.0f; // Detection radius
    
    if (CachedGameMode)
    {
        TArray<APuzzlePiece*> AllPieces = CachedGameMode->GetPuzzlePieces();
        FVector MyLocation = SelectedPiece->GetActorLocation();
        
        // Set selected piece location to drop location temporarily to check distances
        MyLocation.X = DropLocation.X;
        MyLocation.Y = DropLocation.Y;
        MyLocation.Z = 0.0f;
        
        for (APuzzlePiece* Piece : AllPieces)
        {
            if (Piece && Piece != SelectedPiece)
            {
                FVector PieceLocation = Piece->GetActorLocation();
                float Distance = FVector::Dist2D(MyLocation, PieceLocation);
                
                if (Distance < MinDistance)
                {
                    MinDistance = Distance;
                    TargetPiece = Piece;
                }
            }
        }
    }
    
    if (TargetPiece)
    {
        // Swap pieces
        UE_LOG(LogTemp, Warning, TEXT("Swapping pieces: %d with %d"), 
            SelectedPiece->GetPieceID(), TargetPiece->GetPieceID());
        
        FVector MyCurrentLocation = SelectedPiece->GetActorLocation();
        FVector TargetCurrentLocation = TargetPiece->GetActorLocation();
        
        if (CachedGameMode)
        {
            // Snap both positions to grid
            FVector MySnapLocation = CachedGameMode->GetNearestGridPosition(TargetCurrentLocation);
            FVector TargetSnapLocation = CachedGameMode->GetNearestGridPosition(MyCurrentLocation);
            
            // Swap to snapped positions
            SelectedPiece->MovePieceToLocation(MySnapLocation, true);
            TargetPiece->MovePieceToLocation(TargetSnapLocation, true);
        }
        else
        {
            // Fallback: direct swap
            SelectedPiece->MovePieceToLocation(TargetCurrentLocation, true);
            TargetPiece->MovePieceToLocation(MyCurrentLocation, true);
        }

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, 
                FString::Printf(TEXT("Swapped pieces: %d <-> %d"), 
                    SelectedPiece->GetPieceID(), TargetPiece->GetPieceID()));
        }
    }
    else
    {
        // Drop at current location with grid snapping
        if (CachedGameMode)
        {
            // Find the nearest grid position
            FVector SnappedLocation = CachedGameMode->GetNearestGridPosition(DropLocation);
            
            // Check if there's already a piece at this grid position
            APuzzlePiece* OccupyingPiece = CachedGameMode->GetPieceAtGridPosition(SnappedLocation);
            
            if (OccupyingPiece && OccupyingPiece != SelectedPiece)
            {
                // Swap with the piece occupying this grid position
                FVector MyOriginalLocation = SelectedPiece->GetActorLocation();
                FVector MySnapLocation = CachedGameMode->GetNearestGridPosition(MyOriginalLocation);
                
                SelectedPiece->MovePieceToLocation(SnappedLocation, true);
                OccupyingPiece->MovePieceToLocation(MySnapLocation, true);
                
                if (GEngine)
                {
                    GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan,
                        FString::Printf(TEXT("Swapped with piece at grid position!")));
                }
            }
            else
            {
                // No piece at this position, just move there
                SelectedPiece->MovePieceToLocation(SnappedLocation, true);
            }
        }
        else
        {
            SelectedPiece->MovePieceToLocation(DropLocation, true);
        }
    }

    // Clean up drag state
    SelectedPiece->SetSelected(false);
    OnPieceDeselected(SelectedPiece);
    OnDragEnded(SelectedPiece);

    SelectedPiece = nullptr;
    CurrentInteractionState = EMouseInteractionState::None;
    bIsDragging = false;

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange, TEXT("Drag ended"));
    }
}

void APuzzlePlayerController::UpdateDragPosition()
{
    if (!bIsDragging || !SelectedPiece)
    {
        return;
    }

    // Get target position (mouse world location + offset)
    FVector TargetLocation = GetMouseWorldLocation() + DragOffset;
    TargetLocation.Z = DragStartLocation.Z + DragHeight; // Keep at drag height

    // Smoothly interpolate to target position
    FVector CurrentLocation = SelectedPiece->GetActorLocation();
    FVector NewLocation = FMath::VInterpTo(CurrentLocation, TargetLocation, GetWorld()->GetDeltaSeconds(), DragSmoothness);

    SelectedPiece->SetActorLocation(NewLocation);
    
    // Draw snap preview
    if (CachedGameMode)
    {
        FVector SnapPosition = CachedGameMode->GetNearestGridPosition(TargetLocation);
        DrawDebugBox(GetWorld(), SnapPosition + FVector(0, 0, 5), FVector(40, 40, 2), FColor::Green, false, 0.1f, 0, 3.0f);
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


    return bHit;
}

FVector APuzzlePlayerController::GetMouseWorldLocation()
{
    FHitResult HitResult;
    if (TraceUnderMouse(HitResult))
    {
        return HitResult.Location;
    }

    // Fallback: project to a plane at Z=0
    FVector WorldLocation, WorldDirection;
    if (DeprojectMousePositionToWorld(WorldLocation, WorldDirection))
    {
        // Find intersection with Z=0 plane
        float t = -WorldLocation.Z / WorldDirection.Z;
        return WorldLocation + (WorldDirection * t);
    }

    return FVector::ZeroVector;
}

APuzzlePiece* APuzzlePlayerController::GetPuzzlePieceUnderMouse()
{
    FHitResult HitResult;
    if (TraceUnderMouse(HitResult))
    {
        return Cast<APuzzlePiece>(HitResult.GetActor());
    }

    return nullptr;
}

void APuzzlePlayerController::ShowMainWidget()
{
    if (MainWidgetClass && !MainWidget)
    {
        MainWidget = CreateWidget<UUserWidget>(this, MainWidgetClass);
    }

    if (MainWidget)
    {
        MainWidget->AddToViewport();
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
}

APuzzleGameMode* APuzzlePlayerController::GetPuzzleGameMode()
{
    if (!CachedGameMode)
    {
        CachedGameMode = Cast<APuzzleGameMode>(UGameplayStatics::GetGameMode(this));
    }

    return CachedGameMode;
}