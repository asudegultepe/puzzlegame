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
    PieceSpacing = 260.0f; 
    PuzzleStartLocation = FVector(-260.0f, -260.0f, 0.0f); 

    // Default puzzle piece class blueprint'te set edilecek
    PuzzlePieceClass = APuzzlePiece::StaticClass();
    
    // Boundary constraint ayarları
    bEnableBoundaryConstraint = true;
    BoundaryPadding = 200.0f; 

    // Dogru yerleştirme
    bShowGridMarkers = false;
    GridMarkerScale = 0.8f;
    GridMarkerColor = FLinearColor(0.0f, 1.0f, 0.0f, 0.3f);
    
    // Gridler için debug küpleri
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube"));
    if (CubeMeshFinder.Succeeded())
    {
        GridMarkerMesh = CubeMeshFinder.Object;
    }
}

APuzzleGameMode::~APuzzleGameMode()
{
    // Destructor
}

void APuzzleGameMode::BeginPlay()
{
    Super::BeginPlay();

    // Sahnede herhangi bir bulmaca parçasının mevcut kontrolü
    TArray<AActor*> FoundPieces;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), APuzzlePiece::StaticClass(), FoundPieces);
    if (FoundPieces.Num() > 0)
    {
        for (AActor* Actor : FoundPieces)
        {
            if (APuzzlePiece* Piece = Cast<APuzzlePiece>(Actor))
            {
                    
                // Önceki parçaları temizle
                Piece->Destroy();
            }
        }
    }

    // Puzzle'ı başlat
    InitializePuzzle();
}

void APuzzleGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Timer sıfırla
    GetWorldTimerManager().ClearTimer(GameTimerHandle);
    
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

    // Timerı durdur
    GetWorldTimerManager().ClearTimer(GameTimerHandle);

    // Puzzleı yeniden başlat
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
    
    // piece ID null mu
    if (PieceID < 0 || PieceID >= PuzzlePieces.Num())
    {
        return nullptr;
    }
    
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
    
    //İstenilen parça eşsiz mi
    if (PieceID >= 0 && PieceID < PuzzlePieces.Num() && PuzzlePieces[PieceID])
    {
        
        // Musait listedeyse spawn et
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
        
        // Bu parça için doğru pozisyon hesabı
        int32 Row = PieceID / PuzzleWidth;
        int32 Col = PieceID % PuzzleWidth;
        FVector CorrectPosition = PuzzleStartLocation + FVector(
            Col * PieceSpacing,
            Row * PieceSpacing,
            0.0f
        );
        NewPiece->SetCorrectPosition(CorrectPosition);
        
        // Set material et
        if (PieceMaterials.IsValidIndex(PieceID))
        {
            NewPiece->SetPieceMaterial(PieceMaterials[PieceID]);
        }
        else
        {
        }
        
        // Daha sonra ulaşmak için Array e ekle
        if (PieceID >= 0 && PieceID < PuzzlePieces.Num())
        {
            PuzzlePieces[PieceID] = NewPiece;
        }

        // Musait listesinden çıkar
        RemovePieceFromAvailable(PieceID);
        
        // Grid yerini güncelle
        int32 SpawnGridID = GetGridIDFromPosition(SpawnLocation);
        if (SpawnGridID >= 0)
        {
            UpdateGridOccupancy(SpawnGridID, NewPiece);
        }
        
        // Hamle sayısını artır
        //IncrementMoveCount();
        
        // İlk parça yerleştirildiğinde timerı başlat
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
    
    // Dogru pozisyon kontrolü
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
    
    // Doğru pozisyonlardaysa oyunu bitir
    if (SpawnedPieces == PuzzlePieces.Num() && CorrectPieces == PuzzlePieces.Num())
    {
        return true;
    }

    return false;
}


//Dogru konulan parça sayısı
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


// İlerleme yüzdesi
float APuzzleGameMode::GetCompletionPercentage() const
{
    if (PuzzlePieces.Num() == 0)
        return 0.0f;

    return (float)GetCompletedPiecesCount() / (float)PuzzlePieces.Num() * 100.0f;
}


//Oyun bitiminde sayacı durdur
void APuzzleGameMode::OnGameComplete()
{
    CurrentGameState = EPuzzleGameState::Completed;
    GetWorldTimerManager().ClearTimer(GameTimerHandle);

    // Completion event'ini broadcast et
    OnGameCompleted.Broadcast(GameTime, TotalMoves);

}


//Timer 
void APuzzleGameMode::OnTimerTick()
{
    if (CurrentGameState == EPuzzleGameState::InProgress)
    {
        GameTime += 1.0f;
        OnStatsUpdated.Broadcast(GameTime, TotalMoves);
    }
}


//Puzzle ı başlat
void APuzzleGameMode::InitializePuzzle()
{
    CurrentGameState = EPuzzleGameState::NotStarted;

    
    // Önceki parçaları sil
    for (int32 i = 0; i < PuzzlePieces.Num(); i++)
    {
        if (PuzzlePieces[i])
        {
            PuzzlePieces[i]->Destroy();
            PuzzlePieces[i] = nullptr;
        }
    }

    // Boundary oluştur
    CalculateBoundary();

    // Debug
    if (bShowGridMarkers)
    {
        CreateGridVisualization();
    }

    // Puzzle parçalarıını oluştur
    // UI tarafından oluşturulacak
    int32 TotalPieces = PuzzleWidth * PuzzleHeight;
    PuzzlePieces.Empty();
    PuzzlePieces.SetNum(TotalPieces);
    
    GridOccupancy.Empty();
    GridOccupancy.SetNum(TotalPieces);
    for (int32 i = 0; i < TotalPieces; i++)
    {
        GridOccupancy[i] = -1;
    }
    
    for (int32 i = 0; i < TotalPieces; i++)
    {
        PuzzlePieces[i] = nullptr;
    }
    
    // Parçaç ID leri karıştır 
    AvailablePieceIDs.Empty();
    for (int32 i = 0; i < TotalPieces; i++)
    {
        AvailablePieceIDs.Add(i);
    }
    
    // Duplike için kontrol et
    TSet<int32> UniqueCheck;
    for (int32 ID : AvailablePieceIDs)
    {
        bool bAlreadyInSet = false;
        UniqueCheck.Add(ID, &bAlreadyInSet);
        if (bAlreadyInSet)
        {
        }
    }
    
    for (int32 i = AvailablePieceIDs.Num() - 1; i > 0; i--)
    {
        int32 j = FMath::RandRange(0, i);
        AvailablePieceIDs.Swap(i, j);
    }
    
    // Debug: Log 
    FString InitialPieces = TEXT("Initial available pieces: ");
    for (int32 ID : AvailablePieceIDs)
    {
        InitialPieces += FString::Printf(TEXT("%d "), ID);
    }

}

void APuzzleGameMode::CreateGridVisualization()
{

    ClearGridVisualization();

    int32 TotalPieces = PuzzleWidth * PuzzleHeight;

    for (int32 i = 0; i < TotalPieces; i++)
    {
        int32 Row = i / PuzzleWidth;
        int32 Col = i % PuzzleWidth;

        FVector GridPosition = PuzzleStartLocation + FVector(
            Col * PieceSpacing,
            Row * PieceSpacing,
            -10.0f 
        );

        CreateGridMarker(GridPosition, i);

#if WITH_EDITOR
#endif
    }

}

// Gridleri görselleştirmek için oluşturulan fonksiyon 
void APuzzleGameMode::CreateGridMarker(const FVector& Position, int32 GridIndex)
{
    
    AStaticMeshActor* GridMarker = GetWorld()->SpawnActor<AStaticMeshActor>();

    if (GridMarker)
    {
        
        UStaticMeshComponent* MeshComp = GridMarker->GetStaticMeshComponent();

        if (MeshComp && GridMarkerMesh)
        {
            
            MeshComp->SetStaticMesh(GridMarkerMesh);

           
            GridMarker->SetActorLocation(Position);
            GridMarker->SetActorScale3D(FVector(GridMarkerScale, GridMarkerScale, 0.02f)); // Flat marker

            
            UMaterialInterface* DefaultMaterial = MeshComp->GetMaterial(0);
            if (DefaultMaterial)
            {
                UMaterialInstanceDynamic* DynamicMat = UMaterialInstanceDynamic::Create(DefaultMaterial, this);
                if (DynamicMat)
                {
                    
                    DynamicMat->SetVectorParameterValue(TEXT("BaseColor"), GridMarkerColor);
                    MeshComp->SetMaterial(0, DynamicMat);
                }
            }

           
            MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

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


// Boundry box alanı hesaplama
void APuzzleGameMode::CalculateBoundary()
{
   
    FVector MinCorner = PuzzleStartLocation;
    FVector MaxCorner = PuzzleStartLocation + FVector(
        (PuzzleWidth - 1) * PieceSpacing,
        (PuzzleHeight - 1) * PieceSpacing,
        0.0f
    );

    BoundaryMin = MinCorner - FVector(BoundaryPadding, BoundaryPadding, 0.0f);
    BoundaryMax = MaxCorner + FVector(BoundaryPadding, BoundaryPadding, 0.0f);

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

    ClampedLocation.X = FMath::Clamp(ClampedLocation.X, BoundaryMin.X, BoundaryMax.X);

    ClampedLocation.Y = FMath::Clamp(ClampedLocation.Y, BoundaryMin.Y, BoundaryMax.Y);

    // Z değeri değişmeyecek
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
    FVector BoxCenter = (BoundaryMin + BoundaryMax) * 0.5f;
    FVector BoxExtent = (BoundaryMax - BoundaryMin) * 0.5f;

#endif
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
            
            int32 CurrentGridID = GetGridIDFromPosition(Location);
            
        }
        else
        {
        }
    }
    
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
    
    NearestPosition.Z = 0.0f;
    
    
    return NearestPosition;
}

APuzzlePiece* APuzzleGameMode::GetPieceAtGridPosition(const FVector& GridPosition)
{
    for (APuzzlePiece* Piece : PuzzlePieces)
    {
        if (Piece)
        {
            FVector PieceLocation = Piece->GetActorLocation();
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
    FVector NearestGridPos = GetNearestGridPosition(WorldPosition);
    
    FVector RelativePos = NearestGridPos - PuzzleStartLocation;
    
    int32 Col = FMath::RoundToInt(RelativePos.X / PieceSpacing);
    int32 Row = FMath::RoundToInt(RelativePos.Y / PieceSpacing);
    
    if (Col < 0 || Col >= PuzzleWidth || Row < 0 || Row >= PuzzleHeight)
    {
        return -1; 
    }
    
    int32 GridID = Row * PuzzleWidth + Col;
    
    
    return GridID;
}

FVector APuzzleGameMode::GetGridPositionFromID(int32 GridID)
{
    if (GridID < 0 || GridID >= PuzzleWidth * PuzzleHeight)
    {
        return FVector::ZeroVector;
    }
    
    int32 Row = GridID / PuzzleWidth;
    int32 Col = GridID % PuzzleWidth;
    
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
        
        for (int32 i = 0; i < GridOccupancy.Num(); i++)
        {
            if (GridOccupancy[i] == PieceID && i != GridID)
            {
                GridOccupancy[i] = -1;
            }
        }
        
        GridOccupancy[GridID] = PieceID;
    }
    else
    {
        GridOccupancy[GridID] = -1;
    }
}

void APuzzleGameMode::SwapPiecesAtGridIDs(int32 GridID1, int32 GridID2)
{
    if (!GridOccupancy.IsValidIndex(GridID1) || !GridOccupancy.IsValidIndex(GridID2))
    {
        return;
    }
    
    APuzzlePiece* Piece1 = GetPieceAtGridID(GridID1);
    APuzzlePiece* Piece2 = GetPieceAtGridID(GridID2);
    
    FVector GridPos1 = GetGridPositionFromID(GridID1);
    FVector GridPos2 = GetGridPositionFromID(GridID2);
    
    if (Piece1 && Piece2)
    {
        
        Piece1->MovePieceToLocation(GridPos2, false);
        Piece2->MovePieceToLocation(GridPos1, false);
        
        GridOccupancy[GridID1] = Piece2->GetPieceID();
        GridOccupancy[GridID2] = Piece1->GetPieceID();
    }
    else if (Piece1 && !Piece2)
    {
        
        Piece1->MovePieceToLocation(GridPos2, false);
        
        GridOccupancy[GridID1] = -1;
        GridOccupancy[GridID2] = Piece1->GetPieceID();
    }
    else if (!Piece1 && Piece2)
    {
        
        Piece2->MovePieceToLocation(GridPos1, false);
        
        GridOccupancy[GridID2] = -1;
        GridOccupancy[GridID1] = Piece2->GetPieceID();
    }
}

void APuzzleGameMode::ForceCheckGameCompletion()
{
    
    PrintAllPiecePositions();
    
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
    
    
    
    for (int32 i = 0; i < PuzzlePieces.Num(); i++)
    {
        if (PuzzlePieces[i] && !PuzzlePieces[i]->IsInCorrectPosition())
        {
            FVector CurrentPos = PuzzlePieces[i]->GetActorLocation();
            FVector CorrectPos = PuzzlePieces[i]->GetCorrectPosition();
            float Distance = FVector::Dist2D(CurrentPos, CorrectPos);
            
        }
    }
    
    for (int32 i = 0; i < GridOccupancy.Num(); i++)
    {
        if (GridOccupancy[i] < 0)
        {
            FVector GridPos = GetGridPositionFromID(i);
        }
    }
    
    if (PuzzlePieces.IsValidIndex(8) && PuzzlePieces[8])
    {
        FVector Piece8Pos = PuzzlePieces[8]->GetActorLocation();
        int32 Piece8GridID = GetGridIDFromPosition(Piece8Pos);
        
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