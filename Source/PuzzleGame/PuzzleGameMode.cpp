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

void APuzzleGameMode::InitializePuzzle()
{
    CurrentGameState = EPuzzleGameState::NotStarted;

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
        }
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Blue,
            FString::Printf(TEXT("Initialized puzzle with %d pieces"), PuzzlePieces.Num()));
    }
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