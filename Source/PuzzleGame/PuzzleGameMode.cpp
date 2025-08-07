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
        UE_LOG(LogTemp, Error, TEXT("WARNING: Found %d puzzle pieces already in scene at BeginPlay!"), FoundPieces.Num());
        for (AActor* Actor : FoundPieces)
        {
            if (APuzzlePiece* Piece = Cast<APuzzlePiece>(Actor))
            {
                UE_LOG(LogTemp, Error, TEXT("  - Existing Piece ID: %d at location %s"), 
                    Piece->GetPieceID(), *Piece->GetActorLocation().ToString());
                    
                // Destroy any pre-existing pieces
                Piece->Destroy();
            }
        }
        UE_LOG(LogTemp, Warning, TEXT("Destroyed all pre-existing pieces"));
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

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, TEXT("Game Started!"));
        }
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

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("Game Paused"));
        }
    }
}

void APuzzleGameMode::ResumeGame()
{
    if (CurrentGameState == EPuzzleGameState::Paused)
    {
        CurrentGameState = EPuzzleGameState::InProgress;
        GetWorldTimerManager().SetTimer(GameTimerHandle, this, &APuzzleGameMode::OnTimerTick, 1.0f, true);

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("Game Resumed"));
        }
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
    UE_LOG(LogTemp, Warning, TEXT("SpawnPuzzlePiece called: PieceID=%d, Location=%s, TotalPieces=%d"), 
        PieceID, *SpawnLocation.ToString(), PuzzlePieces.Num());
    
    // Validate piece ID
    if (PieceID < 0 || PieceID >= PuzzlePieces.Num())
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid PieceID %d! Valid range is 0-%d"), PieceID, PuzzlePieces.Num()-1);
        return nullptr;
    }
    
    // Debug: Check how many pieces already exist
    int32 ExistingCount = 0;
    for (int32 i = 0; i < PuzzlePieces.Num(); i++)
    {
        if (PuzzlePieces[i] != nullptr)
        {
            ExistingCount++;
            UE_LOG(LogTemp, Verbose, TEXT("  Slot %d: Piece exists"), i);
        }
    }
    UE_LOG(LogTemp, Warning, TEXT("Existing pieces in array: %d/%d"), ExistingCount, PuzzlePieces.Num());
    
    if (!PuzzlePieceClass)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("PuzzlePieceClass not set!"));
        }
        return nullptr;
    }
    
    // Check if this piece already exists
    if (PieceID >= 0 && PieceID < PuzzlePieces.Num() && PuzzlePieces[PieceID])
    {
        UE_LOG(LogTemp, Error, TEXT("SpawnPuzzlePiece: Piece %d already exists! This should not happen."), PieceID);
        UE_LOG(LogTemp, Error, TEXT("Existing piece location: %s"), *PuzzlePieces[PieceID]->GetActorLocation().ToString());
        
        // Check if it's in the available list
        if (AvailablePieceIDs.Contains(PieceID))
        {
            UE_LOG(LogTemp, Error, TEXT("ERROR: Piece %d exists but is still in available list!"), PieceID);
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
            UE_LOG(LogTemp, Warning, TEXT("Applied material %d to Piece %d"), PieceID, PieceID);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("No material for PieceID %d (Materials array size: %d)"), 
                PieceID, PieceMaterials.Num());
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
            UE_LOG(LogTemp, Warning, TEXT("Starting game after first piece spawn"));
            StartGame();
        }
        
        UE_LOG(LogTemp, Warning, TEXT("Successfully spawned piece %d"), PieceID);
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
    UE_LOG(LogTemp, Warning, TEXT("ShufflePuzzlePieces called but not needed in drag-from-UI mode"));
    
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

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
            FString::Printf(TEXT("Puzzle Completed! Time: %.1f seconds, Moves: %d"), GameTime, TotalMoves));
    }
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

    UE_LOG(LogTemp, Warning, TEXT("=== INITIALIZING PUZZLE GRID ==="));
    
    // Clear any existing pieces first
    for (int32 i = 0; i < PuzzlePieces.Num(); i++)
    {
        if (PuzzlePieces[i])
        {
            UE_LOG(LogTemp, Warning, TEXT("Destroying existing piece %d during init"), i);
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
            UE_LOG(LogTemp, Error, TEXT("DUPLICATE ID %d in available pieces!"), ID);
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
    UE_LOG(LogTemp, Warning, TEXT("%s (Total: %d)"), *InitialPieces, AvailablePieceIDs.Num());

    UE_LOG(LogTemp, Warning, TEXT("Initialized puzzle grid for %d pieces"), TotalPieces);
    UE_LOG(LogTemp, Warning, TEXT("Grid boundary: Min %s, Max %s"), *BoundaryMin.ToString(), *BoundaryMax.ToString());
}

void APuzzleGameMode::CreateGridVisualization()
{
    UE_LOG(LogTemp, Warning, TEXT("Creating grid visualization..."));

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
        DrawDebugSphere(GetWorld(), GridPosition + FVector(0, 0, 25), 30.0f, 12, FColor::Green, true, -1.0f, 0, 2.0f);
        DrawDebugString(GetWorld(), GridPosition + FVector(0, 0, 60), FString::Printf(TEXT("%d"), i), nullptr, FColor::White, -1.0f);
#endif
    }

    UE_LOG(LogTemp, Warning, TEXT("Created %d grid markers"), GridMarkers.Num());
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

            UE_LOG(LogTemp, Log, TEXT("Grid marker %d created at position %s"), GridIndex, *Position.ToString());
        }
    }
}

void APuzzleGameMode::ClearGridVisualization()
{
    UE_LOG(LogTemp, Log, TEXT("Clearing %d existing grid markers"), GridMarkers.Num());

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
        UE_LOG(LogTemp, Warning, TEXT("Grid visualization enabled"));
    }
    else
    {
        ClearGridVisualization();
        UE_LOG(LogTemp, Warning, TEXT("Grid visualization disabled"));
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

    UE_LOG(LogTemp, Warning, TEXT("Calculated boundary: Min(%s) Max(%s)"), *BoundaryMin.ToString(), *BoundaryMax.ToString());

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

                UE_LOG(LogTemp, Warning, TEXT("Piece %d moved from %s to %s (boundary constraint)"),
                    Piece->GetPieceID(), *CurrentLocation.ToString(), *ConstrainedLocation.ToString());

                if (GEngine)
                {
                    GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange,
                        FString::Printf(TEXT("Piece %d returned to boundary"), Piece->GetPieceID()));
                }
            }
        }
    }

    if (ConstrainedPieces > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Boundary enforcement: %d pieces constrained"), ConstrainedPieces);
    }
}

void APuzzleGameMode::DrawBoundaryDebug()
{
#if WITH_EDITOR
    // Draw boundary box outline
    FVector BoxCenter = (BoundaryMin + BoundaryMax) * 0.5f;
    FVector BoxExtent = (BoundaryMax - BoundaryMin) * 0.5f;

    DrawDebugBox(GetWorld(), BoxCenter, BoxExtent, FColor::Red, true, -1.0f, 0, 3.0f);

    // Draw corner markers
    DrawDebugSphere(GetWorld(), BoundaryMin, 20.0f, 8, FColor::Red, true, -1.0f);
    DrawDebugSphere(GetWorld(), BoundaryMax, 20.0f, 8, FColor::Red, true, -1.0f);
    DrawDebugSphere(GetWorld(), FVector(BoundaryMin.X, BoundaryMax.Y, 0), 20.0f, 8, FColor::Red, true, -1.0f);
    DrawDebugSphere(GetWorld(), FVector(BoundaryMax.X, BoundaryMin.Y, 0), 20.0f, 8, FColor::Red, true, -1.0f);

    // Draw labels
    DrawDebugString(GetWorld(), BoundaryMin + FVector(0, 0, 50), TEXT("MIN"), nullptr, FColor::Red, -1.0f);
    DrawDebugString(GetWorld(), BoundaryMax + FVector(0, 0, 50), TEXT("MAX"), nullptr, FColor::Red, -1.0f);
#endif
}

// Add to GameMode for testing
void APuzzleGameMode::DebugGridSystem()
{
    UE_LOG(LogTemp, Warning, TEXT("=== GRID SYSTEM DEBUG ==="));
    UE_LOG(LogTemp, Warning, TEXT("Grid Size: %dx%d"), PuzzleWidth, PuzzleHeight);
    UE_LOG(LogTemp, Warning, TEXT("Piece Spacing: %.1f"), PieceSpacing);
    UE_LOG(LogTemp, Warning, TEXT("Start Location: %s"), *PuzzleStartLocation.ToString());
    UE_LOG(LogTemp, Warning, TEXT("Boundary Min: %s"), *BoundaryMin.ToString());
    UE_LOG(LogTemp, Warning, TEXT("Boundary Max: %s"), *BoundaryMax.ToString());
    UE_LOG(LogTemp, Warning, TEXT("Grid Markers: %d"), GridMarkers.Num());
    UE_LOG(LogTemp, Warning, TEXT("Active Pieces: %d"), PuzzlePieces.Num());

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan,
            FString::Printf(TEXT("Grid: %dx%d, Spacing: %.0f, Markers: %d"),
                PuzzleWidth, PuzzleHeight, PieceSpacing, GridMarkers.Num()));
    }
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
    
    UE_LOG(LogTemp, Log, TEXT("Boundary padding set to %.1f"), BoundaryPadding);
}

void APuzzleGameMode::PrintAllPiecePositions()
{
    UE_LOG(LogTemp, Warning, TEXT("=== ALL PIECE POSITIONS ==="));
    
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
            
            UE_LOG(LogTemp, Warning, TEXT("Piece %d: GridID:%d Current(%s) Correct(%s) InPosition:%s Distance:%.2f"),
                Piece->GetPieceID(),
                CurrentGridID,
                *Location.ToString(),
                *CorrectPos.ToString(),
                bIsCorrect ? TEXT("Yes") : TEXT("No"),
                FVector::Dist2D(Location, CorrectPos));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Piece %d: NOT SPAWNED"), i);
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Total: %d, Spawned: %d, Correct: %d (%.1f%%)"),
        TotalPieces, SpawnedPieces, CorrectPieces, 
        SpawnedPieces > 0 ? (float)CorrectPieces / (float)SpawnedPieces * 100.0f : 0.0f);
    
    // Check completion
    bool bShouldBeComplete = SpawnedPieces == TotalPieces && CorrectPieces == TotalPieces;
    UE_LOG(LogTemp, Warning, TEXT("Should game be complete? %s"), bShouldBeComplete ? TEXT("YES") : TEXT("NO"));
    
    if (bShouldBeComplete && CurrentGameState != EPuzzleGameState::Completed)
    {
        UE_LOG(LogTemp, Error, TEXT("ERROR: Game should be complete but isn't! Forcing completion check..."));
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
    
    UE_LOG(LogTemp, Warning, TEXT("GetNearestGridPosition: from %s to %s (distance: %.1f)"), 
        *WorldPosition.ToString(), *NearestPosition.ToString(), MinDistance);
    
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
    UE_LOG(LogTemp, Warning, TEXT("RemovePieceFromAvailable: PieceID=%d, Removed=%d, Remaining=%d"), 
        PieceID, RemovedCount, AvailablePieceIDs.Num());
    
    // Debug: Print all remaining available pieces
    if (AvailablePieceIDs.Num() <= 3)
    {
        FString RemainingPieces = TEXT("Remaining available pieces: ");
        for (int32 ID : AvailablePieceIDs)
        {
            RemainingPieces += FString::Printf(TEXT("%d "), ID);
        }
        UE_LOG(LogTemp, Warning, TEXT("%s"), *RemainingPieces);
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
    
    UE_LOG(LogTemp, Verbose, TEXT("GetGridIDFromPosition: Pos(%s) -> Grid(%d,%d) -> ID %d"), 
        *WorldPosition.ToString(), Col, Row, GridID);
    
    return GridID;
}

FVector APuzzleGameMode::GetGridPositionFromID(int32 GridID)
{
    if (GridID < 0 || GridID >= PuzzleWidth * PuzzleHeight)
    {
        UE_LOG(LogTemp, Warning, TEXT("GetGridPositionFromID: Invalid GridID %d"), GridID);
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
        UE_LOG(LogTemp, Warning, TEXT("UpdateGridOccupancy: Invalid GridID %d"), GridID);
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
                UE_LOG(LogTemp, Warning, TEXT("Removed Piece %d from GridID %d"), PieceID, i);
            }
        }
        
        // Now update at new position
        GridOccupancy[GridID] = PieceID;
        UE_LOG(LogTemp, Warning, TEXT("Updated GridID %d with Piece %d"), GridID, PieceID);
    }
    else
    {
        // Clear the grid position
        GridOccupancy[GridID] = -1;
        UE_LOG(LogTemp, Warning, TEXT("Cleared GridID %d"), GridID);
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
        UE_LOG(LogTemp, Warning, TEXT("Swapping Piece %d at GridID %d with Piece %d at GridID %d"),
            Piece1->GetPieceID(), GridID1, Piece2->GetPieceID(), GridID2);
        
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
        UE_LOG(LogTemp, Warning, TEXT("Moving Piece %d from GridID %d to GridID %d"),
            Piece1->GetPieceID(), GridID1, GridID2);
        
        Piece1->MovePieceToLocation(GridPos2, false);
        
        // Update occupancy
        GridOccupancy[GridID1] = -1;
        GridOccupancy[GridID2] = Piece1->GetPieceID();
    }
    else if (!Piece1 && Piece2)
    {
        // Only position 2 occupied - move piece2 to position 1
        UE_LOG(LogTemp, Warning, TEXT("Moving Piece %d from GridID %d to GridID %d"),
            Piece2->GetPieceID(), GridID2, GridID1);
        
        Piece2->MovePieceToLocation(GridPos1, false);
        
        // Update occupancy
        GridOccupancy[GridID2] = -1;
        GridOccupancy[GridID1] = Piece2->GetPieceID();
    }
    // If neither position has a piece, nothing to do
}

void APuzzleGameMode::ForceCheckGameCompletion()
{
    UE_LOG(LogTemp, Warning, TEXT("=== FORCE CHECK GAME COMPLETION ==="));
    
    // First print all positions
    PrintAllPiecePositions();
    
    // Force check
    if (CheckGameCompletion())
    {
        UE_LOG(LogTemp, Warning, TEXT("Game is complete! Triggering OnGameComplete..."));
        OnGameComplete();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Game is NOT complete"));
    }
}

void APuzzleGameMode::DebugPuzzleState()
{
    UE_LOG(LogTemp, Warning, TEXT(""));
    UE_LOG(LogTemp, Warning, TEXT("========== DEBUG PUZZLE STATE =========="));
    UE_LOG(LogTemp, Warning, TEXT("Game State: %d"), (int32)CurrentGameState);
    UE_LOG(LogTemp, Warning, TEXT("Total Moves: %d"), TotalMoves);
    UE_LOG(LogTemp, Warning, TEXT("Game Time: %.1f seconds"), GameTime);
    UE_LOG(LogTemp, Warning, TEXT("Puzzle Config: %dx%d, Spacing: %.1f"), PuzzleWidth, PuzzleHeight, PieceSpacing);
    UE_LOG(LogTemp, Warning, TEXT("Puzzle Start Location: %s"), *PuzzleStartLocation.ToString());
    UE_LOG(LogTemp, Warning, TEXT(""));
    
    PrintAllPiecePositions();
    
    UE_LOG(LogTemp, Warning, TEXT(""));
    UE_LOG(LogTemp, Warning, TEXT("Grid Occupancy (Size: %d):"), GridOccupancy.Num());
    for (int32 i = 0; i < GridOccupancy.Num(); i++)
    {
        int32 PieceID = GridOccupancy[i];
        FVector GridPos = GetGridPositionFromID(i);
        if (PieceID >= 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("  GridID %d -> PieceID %d at %s"), i, PieceID, *GridPos.ToString());
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("  GridID %d -> EMPTY at %s"), i, *GridPos.ToString());
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("========================================"));
    UE_LOG(LogTemp, Warning, TEXT(""));
    
    // Also show on screen
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Yellow,
            FString::Printf(TEXT("Completed: %d/%d pieces"), GetCompletedPiecesCount(), PuzzlePieces.Num()));
    }
    
    // Check which piece is at wrong position
    UE_LOG(LogTemp, Warning, TEXT(""));
    UE_LOG(LogTemp, Warning, TEXT("=== CHECKING WRONG PIECES ==="));
    for (int32 i = 0; i < PuzzlePieces.Num(); i++)
    {
        if (PuzzlePieces[i] && !PuzzlePieces[i]->IsInCorrectPosition())
        {
            FVector CurrentPos = PuzzlePieces[i]->GetActorLocation();
            FVector CorrectPos = PuzzlePieces[i]->GetCorrectPosition();
            float Distance = FVector::Dist2D(CurrentPos, CorrectPos);
            
            UE_LOG(LogTemp, Error, TEXT("WRONG: Piece %d is at %s but should be at %s (Distance: %.2f)"),
                i, *CurrentPos.ToString(), *CorrectPos.ToString(), Distance);
        }
    }
    
    // Check if any grid position is empty
    UE_LOG(LogTemp, Warning, TEXT(""));
    UE_LOG(LogTemp, Warning, TEXT("=== CHECKING EMPTY GRIDS ==="));
    for (int32 i = 0; i < GridOccupancy.Num(); i++)
    {
        if (GridOccupancy[i] < 0)
        {
            FVector GridPos = GetGridPositionFromID(i);
            UE_LOG(LogTemp, Error, TEXT("EMPTY: GridID %d at %s has no piece!"), i, *GridPos.ToString());
        }
    }
    
    // Special check for Piece 8
    UE_LOG(LogTemp, Warning, TEXT(""));
    UE_LOG(LogTemp, Warning, TEXT("=== LOOKING FOR PIECE 8 ==="));
    if (PuzzlePieces.IsValidIndex(8) && PuzzlePieces[8])
    {
        FVector Piece8Pos = PuzzlePieces[8]->GetActorLocation();
        int32 Piece8GridID = GetGridIDFromPosition(Piece8Pos);
        UE_LOG(LogTemp, Warning, TEXT("Piece 8 is at %s (GridID: %d)"), *Piece8Pos.ToString(), Piece8GridID);
        
        // Check if it's registered in grid occupancy
        bool bFoundInOccupancy = false;
        for (int32 i = 0; i < GridOccupancy.Num(); i++)
        {
            if (GridOccupancy[i] == 8)
            {
                bFoundInOccupancy = true;
                UE_LOG(LogTemp, Warning, TEXT("Piece 8 is registered at GridID %d"), i);
                break;
            }
        }
        if (!bFoundInOccupancy)
        {
            UE_LOG(LogTemp, Error, TEXT("Piece 8 is NOT registered in grid occupancy!"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Piece 8 doesn't exist!"));
    }
}