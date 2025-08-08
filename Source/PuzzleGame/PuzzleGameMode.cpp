// Fill out your copyright notice in the Description page of Project Settings.


#include "PuzzleGameMode.h"
#include "PuzzlePiece.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "Kismet/KismetMathLibrary.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "DrawDebugHelpers.h"

APuzzleGameMode::APuzzleGameMode()
{
    PrimaryActorTick.bCanEverTick = false;

    // Varsayılan değerler
    GameTime = 0.0f;
    TotalMoves = 0;
    CurrentGameState = EPuzzleGameState::NotStarted;

    // Puzzle konfigürasyonu
    PuzzleWidth = 3;
    PuzzleHeight = 3;
    PieceSpacing = 260.0f; // Match Blueprint value
    PuzzleStartLocation = FVector(-260.0f, -260.0f, 0.0f); // Match Blueprint value

    // Default puzzle piece class - Blueprint'te set edilecek
    PuzzlePieceClass = APuzzlePiece::StaticClass();
    
    // Boundary constraint settings
    bEnableBoundaryConstraint = true;
    BoundaryPadding = 200.0f; // Increased padding for more movement freedom
    
    // Grid visualization
    bShowGridMarkers = false;
    GridMarkerScale = 0.8f;
    GridMarkerColor = FLinearColor(0.0f, 1.0f, 0.0f, 0.3f);
    
    // Load grid marker mesh in constructor
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube"));
    if (CubeMeshFinder.Succeeded())
    {
        GridMarkerMesh = CubeMeshFinder.Object;
    }
}

APuzzleGameMode::~APuzzleGameMode()
{
    // Destructor is empty - UPROPERTY arrays are managed by Unreal
}

void APuzzleGameMode::BeginPlay()
{
    Super::BeginPlay();

    // Check if any puzzle pieces already exist in the scene
    TArray<AActor*> FoundPieces;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), APuzzlePiece::StaticClass(), FoundPieces);
    if (FoundPieces.Num() > 0)
    {
        for (AActor* Actor : FoundPieces)
        {
            if (APuzzlePiece* Piece = Cast<APuzzlePiece>(Actor))
            {
                    
                // Destroy any pre-existing pieces
                Piece->Destroy();
            }
        }
    }

    // Puzzle'ı başlat
    InitializePuzzle();
}

void APuzzleGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Clear timer first
    GetWorldTimerManager().ClearTimer(GameTimerHandle);
    
    // Don't manually clear UPROPERTY arrays in EndPlay
    // Unreal Engine will handle the cleanup automatically
    
    Super::EndPlay(EndPlayReason);
}

void APuzzleGameMode::StartGame()
{
    if (CurrentGameState == EPuzzleGameState::NotStarted ||
        CurrentGameState == EPuzzleGameState::Completed)
    {
        // Oyunu sıfırla
        GameTime = 0.0f;
        TotalMoves = 0;
        CurrentGameState = EPuzzleGameState::InProgress;

        // Puzzle parçalarını karıştır
        ShufflePuzzlePieces();

        // Timer'ı başlat
        GetWorldTimerManager().SetTimer(GameTimerHandle, this, &APuzzleGameMode::OnTimerTick, 1.0f, true);

        // İstatistikleri güncelle
        OnStatsUpdated.Broadcast(GameTime, TotalMoves);

    }
    else if (CurrentGameState == EPuzzleGameState::Paused)
    {
        ResumeGame();
    }
}

void APuzzleGameMode::PauseGame()
{
    if (CurrentGameState == EPuzzleGameState::InProgress)
    {
        CurrentGameState = EPuzzleGameState::Paused;
        GetWorldTimerManager().ClearTimer(GameTimerHandle);

    }
}

void APuzzleGameMode::ResumeGame()
{
    if (CurrentGameState == EPuzzleGameState::Paused)
    {
        CurrentGameState = EPuzzleGameState::InProgress;
        GetWorldTimerManager().SetTimer(GameTimerHandle, this, &APuzzleGameMode::OnTimerTick, 1.0f, true);

    }
}

void APuzzleGameMode::RestartGame()
{
    // Mevcut puzzle parçalarını temizle
    for (APuzzlePiece* Piece : PuzzlePieces)
    {
        if (IsValid(Piece))
        {
            Piece->Destroy();
        }
    }
    PuzzlePieces.Empty();

    // Timer'ı durdur
    GetWorldTimerManager().ClearTimer(GameTimerHandle);

    // Puzzle'ı yeniden başlat
    InitializePuzzle();
    StartGame();
}

void APuzzleGameMode::IncrementMoveCount()
{
    if (CurrentGameState == EPuzzleGameState::InProgress)
    {
        TotalMoves++;
        OnStatsUpdated.Broadcast(GameTime, TotalMoves);

        // Her hamle sonrası oyunun bitip bitmediğini kontrol et
        if (CheckGameCompletion())
        {
            OnGameComplete();
        }
    }
}

APuzzlePiece* APuzzleGameMode::SpawnPuzzlePiece(int32 PieceID, FVector SpawnLocation)
{
    
    // Validate piece ID
    if (PieceID < 0 || PieceID >= PuzzlePieces.Num())
    {
        return nullptr;
    }
    
    // Debug: Check how many pieces already exist
    int32 ExistingCount = 0;
    for (int32 i = 0; i < PuzzlePieces.Num(); i++)
    {
        if (PuzzlePieces[i] != nullptr)
        {
            ExistingCount++;
        }
    }
    
    if (!PuzzlePieceClass)
    {
        return nullptr;
    }
    
    // Check if this piece already exists
    if (PieceID >= 0 && PieceID < PuzzlePieces.Num() && PuzzlePieces[PieceID])
    {
        
        // Check if it's in the available list
        if (AvailablePieceIDs.Contains(PieceID))
        {
        }
        
        // Still remove from available list if it somehow exists there
        RemovePieceFromAvailable(PieceID);
        
        // This is a bug - we shouldn't be trying to spawn an already existing piece
        // Return null instead of the existing piece to prevent moving it
        return nullptr;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    APuzzlePiece* NewPiece = GetWorld()->SpawnActor<APuzzlePiece>(PuzzlePieceClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);

    if (NewPiece)
    {
        NewPiece->SetPieceID(PieceID);
        
        // Calculate correct position for this piece
        int32 Row = PieceID / PuzzleWidth;
        int32 Col = PieceID % PuzzleWidth;
        FVector CorrectPosition = PuzzleStartLocation + FVector(
            Col * PieceSpacing,
            Row * PieceSpacing,
            0.0f
        );
        NewPiece->SetCorrectPosition(CorrectPosition);
        
        // Set material based on PieceID if materials are configured
        if (PieceMaterials.IsValidIndex(PieceID))
        {
            NewPiece->SetPieceMaterial(PieceMaterials[PieceID]);
        }
        else
        {
        }
        
        // Store in array at correct index
        if (PieceID >= 0 && PieceID < PuzzlePieces.Num())
        {
            PuzzlePieces[PieceID] = NewPiece;
        }

        // Remove from available list
        RemovePieceFromAvailable(PieceID);
        
        // Register in grid occupancy
        int32 SpawnGridID = GetGridIDFromPosition(SpawnLocation);
        if (SpawnGridID >= 0)
        {
            UpdateGridOccupancy(SpawnGridID, NewPiece);
        }
        
        // Hamle sayısını artır
        IncrementMoveCount();
        
        // Start the game timer if this is the first piece
        if (CurrentGameState == EPuzzleGameState::NotStarted)
        {
            StartGame();
        }
        
    }

    return NewPiece;
}

bool APuzzleGameMode::CheckGameCompletion()
{
    if (PuzzlePieces.Num() == 0)
        return false;
    
    // Check if all pieces are spawned and in correct positions
    int32 SpawnedPieces = 0;
    int32 CorrectPieces = 0;
    
    for (APuzzlePiece* Piece : PuzzlePieces)
    {
        if (IsValid(Piece))
        {
            SpawnedPieces++;
            if (Piece->IsInCorrectPosition())
            {
                CorrectPieces++;
            }
        }
    }
    
    // Game is complete when all pieces are spawned and in correct positions
    if (SpawnedPieces == PuzzlePieces.Num() && CorrectPieces == PuzzlePieces.Num())
    {
        return true;
    }

    return false;
}

void APuzzleGameMode::ShufflePuzzlePieces()
{
    // Since pieces are spawned from UI, we don't shuffle existing pieces
    // This function is kept for compatibility but does nothing in the new system
    
    // Comment out the shuffling logic as it's not compatible with UI-based spawning
    /*
    // Array'i karıştır (Fisher-Yates shuffle algoritması)
    for (int32 i = PuzzlePieces.Num() - 1; i > 0; i--)
    {
        int32 RandomIndex = UKismetMathLibrary::RandomIntegerInRange(0, i);
        PuzzlePieces.Swap(i, RandomIndex);
    }

    // Karıştırıldıktan sonra pozisyonları güncelle
    for (int32 i = 0; i < PuzzlePieces.Num(); i++)
    {
        if (IsValid(PuzzlePieces[i]))
        {
            // Yeni pozisyonu hesapla (grid layout)
            int32 Row = i / PuzzleWidth;
            int32 Col = i % PuzzleWidth;

            FVector NewPosition = PuzzleStartLocation + FVector(
                Col * PieceSpacing,
                Row * PieceSpacing,
                0.0f
            );

            PuzzlePieces[i]->MovePieceToLocation(NewPosition, false);
        }
    }
    */
}

int32 APuzzleGameMode::GetCompletedPiecesCount() const
{
    int32 Count = 0;
    for (const APuzzlePiece* Piece : PuzzlePieces)
    {
        if (IsValid(Piece) && Piece->IsInCorrectPosition())
        {
            Count++;
        }
    }
    return Count;
}

float APuzzleGameMode::GetCompletionPercentage() const
{
    if (PuzzlePieces.Num() == 0)
        return 0.0f;

    return (float)GetCompletedPiecesCount() / (float)PuzzlePieces.Num() * 100.0f;
}

void APuzzleGameMode::OnGameComplete()
{
    CurrentGameState = EPuzzleGameState::Completed;
    GetWorldTimerManager().ClearTimer(GameTimerHandle);

    // Completion event'ini broadcast et
    OnGameCompleted.Broadcast(GameTime, TotalMoves);

}

void APuzzleGameMode::OnTimerTick()
{
    if (CurrentGameState == EPuzzleGameState::InProgress)
    {
        GameTime += 1.0f;
        OnStatsUpdated.Broadcast(GameTime, TotalMoves);
    }
}

void APuzzleGameMode::InitializePuzzle()
{
    CurrentGameState = EPuzzleGameState::NotStarted;

    
    // Clear any existing pieces first
    for (int32 i = 0; i < PuzzlePieces.Num(); i++)
    {
        if (PuzzlePieces[i])
        {
            PuzzlePieces[i]->Destroy();
            PuzzlePieces[i] = nullptr;
        }
    }

    // Calculate boundary first
    CalculateBoundary();

    // Create grid visualization
    if (bShowGridMarkers)
    {
        CreateGridVisualization();
    }

    // Initialize the puzzle pieces array but don't spawn them
    // They will be spawned when dragged from UI
    int32 TotalPieces = PuzzleWidth * PuzzleHeight;
    PuzzlePieces.Empty();
    PuzzlePieces.SetNum(TotalPieces);
    
    // Initialize grid occupancy array
    GridOccupancy.Empty();
    GridOccupancy.SetNum(TotalPieces);
    for (int32 i = 0; i < TotalPieces; i++)
    {
        GridOccupancy[i] = -1; // -1 means empty
    }
    
    // Ensure all entries are null initially
    for (int32 i = 0; i < TotalPieces; i++)
    {
        PuzzlePieces[i] = nullptr;
    }
    
    // Create a shuffled list of piece IDs for the UI
    AvailablePieceIDs.Empty();
    for (int32 i = 0; i < TotalPieces; i++)
    {
        AvailablePieceIDs.Add(i);
    }
    
    // Check for duplicates before shuffling
    TSet<int32> UniqueCheck;
    for (int32 ID : AvailablePieceIDs)
    {
        bool bAlreadyInSet = false;
        UniqueCheck.Add(ID, &bAlreadyInSet);
        if (bAlreadyInSet)
        {
        }
    }
    
    // Shuffle the available pieces for random order
    for (int32 i = AvailablePieceIDs.Num() - 1; i > 0; i--)
    {
        int32 j = FMath::RandRange(0, i);
        AvailablePieceIDs.Swap(i, j);
    }
    
    // Debug: Log initial available pieces
    FString InitialPieces = TEXT("Initial available pieces: ");
    for (int32 ID : AvailablePieceIDs)
    {
        InitialPieces += FString::Printf(TEXT("%d "), ID);
    }

}

void APuzzleGameMode::CreateGridVisualization()
{

    // Clear existing markers
    ClearGridVisualization();

    // Create grid markers for each correct position
    int32 TotalPieces = PuzzleWidth * PuzzleHeight;

    for (int32 i = 0; i < TotalPieces; i++)
    {
        // Calculate grid position
        int32 Row = i / PuzzleWidth;
        int32 Col = i % PuzzleWidth;

        FVector GridPosition = PuzzleStartLocation + FVector(
            Col * PieceSpacing,
            Row * PieceSpacing,
            -10.0f // Slightly below ground
        );

        CreateGridMarker(GridPosition, i);

        // Debug sphere for visualization
#if WITH_EDITOR
#endif
    }

}

void APuzzleGameMode::CreateGridMarker(const FVector& Position, int32 GridIndex)
{
    // Spawn static mesh actor for grid marker
    AStaticMeshActor* GridMarker = GetWorld()->SpawnActor<AStaticMeshActor>();

    if (GridMarker)
    {
        // Get mesh component
        UStaticMeshComponent* MeshComp = GridMarker->GetStaticMeshComponent();

        if (MeshComp && GridMarkerMesh)
        {
            // Set the pre-loaded cube mesh
            MeshComp->SetStaticMesh(GridMarkerMesh);

            // Set position and scale
            GridMarker->SetActorLocation(Position);
            GridMarker->SetActorScale3D(FVector(GridMarkerScale, GridMarkerScale, 0.02f)); // Flat marker

            // Create dynamic material for grid marker
            UMaterialInterface* DefaultMaterial = MeshComp->GetMaterial(0);
            if (DefaultMaterial)
            {
                UMaterialInstanceDynamic* DynamicMat = UMaterialInstanceDynamic::Create(DefaultMaterial, this);
                if (DynamicMat)
                {
                    // Set grid marker color from property
                    DynamicMat->SetVectorParameterValue(TEXT("BaseColor"), GridMarkerColor);
                    MeshComp->SetMaterial(0, DynamicMat);
                }
            }

            // Set collision to no collision (visual only)
            MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

            // Set render settings
            MeshComp->SetCastShadow(false);
            MeshComp->SetReceivesDecals(false);

            GridMarkers.Add(GridMarker);

        }
    }
}

void APuzzleGameMode::ClearGridVisualization()
{

    for (AStaticMeshActor* Marker : GridMarkers)
    {
        if (IsValid(Marker))
        {
            Marker->Destroy();
        }
    }

    GridMarkers.Empty();
}

void APuzzleGameMode::ToggleGridVisualization()
{
    bShowGridMarkers = !bShowGridMarkers;

    if (bShowGridMarkers)
    {
        CreateGridVisualization();
    }
    else
    {
        ClearGridVisualization();
    }
}

void APuzzleGameMode::CalculateBoundary()
{
    // Calculate the bounding box for the puzzle grid
    FVector MinCorner = PuzzleStartLocation;
    FVector MaxCorner = PuzzleStartLocation + FVector(
        (PuzzleWidth - 1) * PieceSpacing,
        (PuzzleHeight - 1) * PieceSpacing,
        0.0f
    );

    // Add padding to boundary
    BoundaryMin = MinCorner - FVector(BoundaryPadding, BoundaryPadding, 0.0f);
    BoundaryMax = MaxCorner + FVector(BoundaryPadding, BoundaryPadding, 0.0f);


    // Debug draw boundary box
    DrawBoundaryDebug();
}

bool APuzzleGameMode::IsLocationWithinBoundary(const FVector& Location)
{
    return (Location.X >= BoundaryMin.X && Location.X <= BoundaryMax.X &&
        Location.Y >= BoundaryMin.Y && Location.Y <= BoundaryMax.Y);
}

FVector APuzzleGameMode::ClampLocationToBoundary(const FVector& Location)
{
    FVector ClampedLocation = Location;

    // Clamp X coordinate
    ClampedLocation.X = FMath::Clamp(ClampedLocation.X, BoundaryMin.X, BoundaryMax.X);

    // Clamp Y coordinate
    ClampedLocation.Y = FMath::Clamp(ClampedLocation.Y, BoundaryMin.Y, BoundaryMax.Y);

    // Keep Z at ground level
    ClampedLocation.Z = 0.0f;

    return ClampedLocation;
}

void APuzzleGameMode::EnforceAllPieceBoundaries()
{
    int32 ConstrainedPieces = 0;

    for (APuzzlePiece* Piece : PuzzlePieces)
    {
        if (IsValid(Piece))
        {
            FVector CurrentLocation = Piece->GetActorLocation();

            if (!IsLocationWithinBoundary(CurrentLocation))
            {
                FVector ConstrainedLocation = ClampLocationToBoundary(CurrentLocation);
                Piece->MovePieceToLocation(ConstrainedLocation, true);

                ConstrainedPieces++;


            }
        }
    }

    if (ConstrainedPieces > 0)
    {
    }
}

void APuzzleGameMode::DrawBoundaryDebug()
{
#if WITH_EDITOR
    // Draw boundary box outline
    FVector BoxCenter = (BoundaryMin + BoundaryMax) * 0.5f;
    FVector BoxExtent = (BoundaryMax - BoundaryMin) * 0.5f;

#endif
}

// Add to GameMode for testing
void APuzzleGameMode::DebugGridSystem()
{

}

void APuzzleGameMode::RefreshGridVisualization()
{
    ClearGridVisualization();
    if (bShowGridMarkers)
    {
        CreateGridVisualization();
    }
}

void APuzzleGameMode::SetBoundaryPadding(float NewPadding)
{
    BoundaryPadding = NewPadding;
    CalculateBoundary();
    EnforceAllPieceBoundaries();
    
}

void APuzzleGameMode::PrintAllPiecePositions()
{
    
    int32 TotalPieces = 0;
    int32 SpawnedPieces = 0;
    int32 CorrectPieces = 0;
    
    for (int32 i = 0; i < PuzzlePieces.Num(); i++)
    {
        TotalPieces++;
        APuzzlePiece* Piece = PuzzlePieces[i];
        
        if (Piece)
        {
            SpawnedPieces++;
            FVector Location = Piece->GetActorLocation();
            FVector CorrectPos = Piece->GetCorrectPosition();
            bool bIsCorrect = Piece->IsInCorrectPosition();
            
            if (bIsCorrect)
            {
                CorrectPieces++;
            }
            
            // Find which grid this piece is on
            int32 CurrentGridID = GetGridIDFromPosition(Location);
            
        }
        else
        {
        }
    }
    
    
    // Check completion
    bool bShouldBeComplete = SpawnedPieces == TotalPieces && CorrectPieces == TotalPieces;
    
    if (bShouldBeComplete && CurrentGameState != EPuzzleGameState::Completed)
    {
        if (CheckGameCompletion())
        {
            OnGameComplete();
        }
    }
}

FVector APuzzleGameMode::GetNearestGridPosition(const FVector& WorldPosition)
{
    FVector NearestPosition = WorldPosition;
    float MinDistance = FLT_MAX;
    
    // Check all grid positions
    for (int32 Row = 0; Row < PuzzleHeight; Row++)
    {
        for (int32 Col = 0; Col < PuzzleWidth; Col++)
        {
            FVector GridPosition = PuzzleStartLocation + FVector(
                Col * PieceSpacing,
                Row * PieceSpacing,
                0.0f
            );
            
            float Distance = FVector::Dist2D(WorldPosition, GridPosition);
            if (Distance < MinDistance)
            {
                MinDistance = Distance;
                NearestPosition = GridPosition;
            }
        }
    }
    
    // Keep the Z coordinate at ground level
    NearestPosition.Z = 0.0f;
    
    
    return NearestPosition;
}

APuzzlePiece* APuzzleGameMode::GetPieceAtGridPosition(const FVector& GridPosition)
{
    // Check all pieces to find one at the given grid position
    for (APuzzlePiece* Piece : PuzzlePieces)
    {
        if (Piece)
        {
            FVector PieceLocation = Piece->GetActorLocation();
            // Check if piece is at this grid position (within a small tolerance)
            if (FVector::Dist2D(PieceLocation, GridPosition) < 10.0f)
            {
                return Piece;
            }
        }
    }
    
    return nullptr;
}

void APuzzleGameMode::RemovePieceFromAvailable(int32 PieceID)
{
    int32 RemovedCount = AvailablePieceIDs.Remove(PieceID);
    
    // Debug: Print all remaining available pieces
    if (AvailablePieceIDs.Num() <= 3)
    {
        FString RemainingPieces = TEXT("Remaining available pieces: ");
        for (int32 ID : AvailablePieceIDs)
        {
            RemainingPieces += FString::Printf(TEXT("%d "), ID);
        }
    }
}

int32 APuzzleGameMode::GetGridIDFromPosition(const FVector& WorldPosition)
{
    // Find the nearest grid position first
    FVector NearestGridPos = GetNearestGridPosition(WorldPosition);
    
    // Calculate which grid cell this corresponds to
    FVector RelativePos = NearestGridPos - PuzzleStartLocation;
    
    int32 Col = FMath::RoundToInt(RelativePos.X / PieceSpacing);
    int32 Row = FMath::RoundToInt(RelativePos.Y / PieceSpacing);
    
    // Validate bounds
    if (Col < 0 || Col >= PuzzleWidth || Row < 0 || Row >= PuzzleHeight)
    {
        return -1; // Invalid grid position
    }
    
    // Convert to grid ID (row * width + col)
    int32 GridID = Row * PuzzleWidth + Col;
    
    
    return GridID;
}

FVector APuzzleGameMode::GetGridPositionFromID(int32 GridID)
{
    if (GridID < 0 || GridID >= PuzzleWidth * PuzzleHeight)
    {
        return FVector::ZeroVector;
    }
    
    // Calculate row and column from ID
    int32 Row = GridID / PuzzleWidth;
    int32 Col = GridID % PuzzleWidth;
    
    // Calculate world position
    FVector GridPosition = PuzzleStartLocation + FVector(
        Col * PieceSpacing,
        Row * PieceSpacing,
        0.0f
    );
    
    return GridPosition;
}

APuzzlePiece* APuzzleGameMode::GetPieceAtGridID(int32 GridID)
{
    if (GridID < 0 || GridID >= PuzzleWidth * PuzzleHeight)
    {
        return nullptr;
    }
    
    // Check the grid occupancy array
    if (GridOccupancy.IsValidIndex(GridID))
    {
        int32 PieceID = GridOccupancy[GridID];
        if (PieceID >= 0 && PuzzlePieces.IsValidIndex(PieceID))
        {
            return PuzzlePieces[PieceID];
        }
    }
    
    return nullptr;
}

void APuzzleGameMode::UpdateGridOccupancy(int32 GridID, APuzzlePiece* Piece)
{
    if (GridID < 0 || GridID >= PuzzleWidth * PuzzleHeight || !GridOccupancy.IsValidIndex(GridID))
    {
        return;
    }
    
    if (Piece)
    {
        int32 PieceID = Piece->GetPieceID();
        
        // First remove this piece from any other grid position
        for (int32 i = 0; i < GridOccupancy.Num(); i++)
        {
            if (GridOccupancy[i] == PieceID && i != GridID)
            {
                GridOccupancy[i] = -1;
            }
        }
        
        // Now update at new position
        GridOccupancy[GridID] = PieceID;
    }
    else
    {
        // Clear the grid position
        GridOccupancy[GridID] = -1;
    }
}

void APuzzleGameMode::SwapPiecesAtGridIDs(int32 GridID1, int32 GridID2)
{
    if (!GridOccupancy.IsValidIndex(GridID1) || !GridOccupancy.IsValidIndex(GridID2))
    {
        return;
    }
    
    // Get pieces at both grid positions
    APuzzlePiece* Piece1 = GetPieceAtGridID(GridID1);
    APuzzlePiece* Piece2 = GetPieceAtGridID(GridID2);
    
    // Get the grid positions
    FVector GridPos1 = GetGridPositionFromID(GridID1);
    FVector GridPos2 = GetGridPositionFromID(GridID2);
    
    if (Piece1 && Piece2)
    {
        // Both positions occupied - swap them
        
        // Move pieces
        Piece1->MovePieceToLocation(GridPos2, false);
        Piece2->MovePieceToLocation(GridPos1, false);
        
        // Update occupancy
        GridOccupancy[GridID1] = Piece2->GetPieceID();
        GridOccupancy[GridID2] = Piece1->GetPieceID();
    }
    else if (Piece1 && !Piece2)
    {
        // Only position 1 occupied - move piece1 to position 2
        
        Piece1->MovePieceToLocation(GridPos2, false);
        
        // Update occupancy
        GridOccupancy[GridID1] = -1;
        GridOccupancy[GridID2] = Piece1->GetPieceID();
    }
    else if (!Piece1 && Piece2)
    {
        // Only position 2 occupied - move piece2 to position 1
        
        Piece2->MovePieceToLocation(GridPos1, false);
        
        // Update occupancy
        GridOccupancy[GridID2] = -1;
        GridOccupancy[GridID1] = Piece2->GetPieceID();
    }
    // If neither position has a piece, nothing to do
}

void APuzzleGameMode::ForceCheckGameCompletion()
{
    
    // First print all positions
    PrintAllPiecePositions();
    
    // Force check
    if (CheckGameCompletion())
    {
        OnGameComplete();
    }
    else
    {
    }
}

void APuzzleGameMode::DebugPuzzleState()
{
    
    PrintAllPiecePositions();
    
    for (int32 i = 0; i < GridOccupancy.Num(); i++)
    {
        int32 PieceID = GridOccupancy[i];
        FVector GridPos = GetGridPositionFromID(i);
        if (PieceID >= 0)
        {
        }
        else
        {
        }
    }
    
    
    // Also show on screen
    
    // Check which piece is at wrong position
    for (int32 i = 0; i < PuzzlePieces.Num(); i++)
    {
        if (PuzzlePieces[i] && !PuzzlePieces[i]->IsInCorrectPosition())
        {
            FVector CurrentPos = PuzzlePieces[i]->GetActorLocation();
            FVector CorrectPos = PuzzlePieces[i]->GetCorrectPosition();
            float Distance = FVector::Dist2D(CurrentPos, CorrectPos);
            
        }
    }
    
    // Check if any grid position is empty
    for (int32 i = 0; i < GridOccupancy.Num(); i++)
    {
        if (GridOccupancy[i] < 0)
        {
            FVector GridPos = GetGridPositionFromID(i);
        }
    }
    
    // Special check for Piece 8
    if (PuzzlePieces.IsValidIndex(8) && PuzzlePieces[8])
    {
        FVector Piece8Pos = PuzzlePieces[8]->GetActorLocation();
        int32 Piece8GridID = GetGridIDFromPosition(Piece8Pos);
        
        // Check if it's registered in grid occupancy
        bool bFoundInOccupancy = false;
        for (int32 i = 0; i < GridOccupancy.Num(); i++)
        {
            if (GridOccupancy[i] == 8)
            {
                bFoundInOccupancy = true;
                break;
            }
        }
        if (!bFoundInOccupancy)
        {
        }
    }
    else
    {
    }
}