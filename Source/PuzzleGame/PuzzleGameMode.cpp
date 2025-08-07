// Fill out your copyright notice in the Description page of Project Settings.


#include "PuzzleGameMode.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "Kismet/KismetMathLibrary.h"

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
    PieceSpacing = 120.0f;
    PuzzleStartLocation = FVector(0.0f, 0.0f, 0.0f);

    // Default puzzle piece class - Blueprint'te set edilecek
    PuzzlePieceClass = APuzzlePiece::StaticClass();
}

void APuzzleGameMode::BeginPlay()
{
    Super::BeginPlay();

    // Puzzle'ı başlat
    InitializePuzzle();
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
    if (!PuzzlePieceClass)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("PuzzlePieceClass not set!"));
        }
        return nullptr;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    APuzzlePiece* NewPiece = GetWorld()->SpawnActor<APuzzlePiece>(PuzzlePieceClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);

    if (NewPiece)
    {
        NewPiece->SetPieceID(PieceID);
        PuzzlePieces.Add(NewPiece);

        // Hamle sayısını artır
        IncrementMoveCount();
    }

    return NewPiece;
}

bool APuzzleGameMode::CheckGameCompletion()
{
    if (PuzzlePieces.Num() == 0)
        return false;

    for (APuzzlePiece* Piece : PuzzlePieces)
    {
        if (IsValid(Piece) && !Piece->IsInCorrectPosition())
        {
            return false;
        }
    }

    return true;
}

void APuzzleGameMode::ShufflePuzzlePieces()
{
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

    // Calculate boundary first
    CalculateBoundary();

    // Create grid visualization
    if (bShowGridMarkers)
    {
        CreateGridVisualization();
    }

    // Puzzle parçalarını oluştur
    int32 TotalPieces = PuzzleWidth * PuzzleHeight;

    for (int32 i = 0; i < TotalPieces; i++)
    {
        // Her parçanın doğru pozisyonunu hesapla
        int32 Row = i / PuzzleWidth;
        int32 Col = i % PuzzleWidth;

        FVector CorrectPosition = PuzzleStartLocation + FVector(
            Col * PieceSpacing,
            Row * PieceSpacing,
            0.0f
        );

        // Başlangıç pozisyonu (şimdilik aynı, sonra karıştırılacak)
        FVector StartPosition = CorrectPosition + FVector(0.0f, 0.0f, 100.0f); // Biraz yukarıda başlat

        APuzzlePiece* NewPiece = GetWorld()->SpawnActor<APuzzlePiece>(PuzzlePieceClass, StartPosition, FRotator::ZeroRotator);

        if (NewPiece)
        {
            NewPiece->SetPieceID(i);
            NewPiece->SetCorrectPosition(CorrectPosition);
            PuzzlePieces.Add(NewPiece);

            UE_LOG(LogTemp, Log, TEXT("Piece %d: Correct Position %s"), i, *CorrectPosition.ToString());
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("Initialized puzzle with %d pieces"), PuzzlePieces.Num());
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

        if (MeshComp)
        {
            // Set cube mesh (engine default)
            static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube"));
            if (CubeMesh.Succeeded())
            {
                MeshComp->SetStaticMesh(CubeMesh.Object);
            }

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
                    // Set grid marker color (semi-transparent green)
                    DynamicMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.0f, 1.0f, 0.0f, 0.3f));
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
    
    for (APuzzlePiece* Piece : PuzzlePieces)
    {
        if (Piece)
        {
            FVector Location = Piece->GetActorLocation();
            FVector CorrectPos = Piece->GetCorrectPosition();
            bool bIsCorrect = Piece->IsInCorrectPosition();
            
            UE_LOG(LogTemp, Warning, TEXT("Piece %d: Current(%s) Correct(%s) InPosition:%s"),
                Piece->GetPieceID(),
                *Location.ToString(),
                *CorrectPos.ToString(),
                bIsCorrect ? TEXT("Yes") : TEXT("No"));
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Total Pieces: %d, Completed: %d (%.1f%%)"),
        PuzzlePieces.Num(), GetCompletedPiecesCount(), GetCompletionPercentage());
}