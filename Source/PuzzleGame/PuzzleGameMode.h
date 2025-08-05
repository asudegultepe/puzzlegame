// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PuzzlePiece.h"
#include "Engine/TimerHandle.h"
#include "PuzzleGameMode.generated.h"

// Oyun durumunu temsil eden enum
UENUM(BlueprintType)
enum class EPuzzleGameState : uint8
{
    NotStarted    UMETA(DisplayName = "Not Started"),
    InProgress    UMETA(DisplayName = "In Progress"),
    Completed     UMETA(DisplayName = "Completed"),
    Paused        UMETA(DisplayName = "Paused")
};

// Game completion event için delegate
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGameCompleted, float, TotalTime, int32, TotalMoves);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStatsUpdated, float, CurrentTime, int32, CurrentMoves);

UCLASS()
class PUZZLEGAME_API APuzzleGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    APuzzleGameMode();

protected:
    virtual void BeginPlay() override;

    // Oyun istatistikleri
    UPROPERTY(BlueprintReadOnly, Category = "Game Stats")
    float GameTime;

    UPROPERTY(BlueprintReadOnly, Category = "Game Stats")
    int32 TotalMoves;

    UPROPERTY(BlueprintReadOnly, Category = "Game Stats")
    EPuzzleGameState CurrentGameState;

    // Puzzle konfigürasyonu
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle Config")
    int32 PuzzleWidth;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle Config")
    int32 PuzzleHeight;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle Config")
    float PieceSpacing;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle Config")
    FVector PuzzleStartLocation;

    // Puzzle parçaları
    UPROPERTY(BlueprintReadOnly, Category = "Puzzle")
    TArray<APuzzlePiece*> PuzzlePieces;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Puzzle")
    TSubclassOf<APuzzlePiece> PuzzlePieceClass;

    // Timer handle
    FTimerHandle GameTimerHandle;

public:
    // Event dispatchers
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnGameCompleted OnGameCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnStatsUpdated OnStatsUpdated;

    // Oyun kontrol fonksiyonları
    UFUNCTION(BlueprintCallable, Category = "Game Control")
    void StartGame();

    UFUNCTION(BlueprintCallable, Category = "Game Control")
    void PauseGame();

    UFUNCTION(BlueprintCallable, Category = "Game Control")
    void ResumeGame();

    UFUNCTION(BlueprintCallable, Category = "Game Control")
    void RestartGame();

    // İstatistik fonksiyonları
    UFUNCTION(BlueprintCallable, Category = "Game Stats")
    void IncrementMoveCount();

    UFUNCTION(BlueprintPure, Category = "Game Stats")
    float GetGameTime() const { return GameTime; }

    UFUNCTION(BlueprintPure, Category = "Game Stats")
    int32 GetTotalMoves() const { return TotalMoves; }

    UFUNCTION(BlueprintPure, Category = "Game Stats")
    EPuzzleGameState GetCurrentGameState() const { return CurrentGameState; }

    // Puzzle parçası fonksiyonları
    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    APuzzlePiece* SpawnPuzzlePiece(int32 PieceID, FVector SpawnLocation);

    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    bool CheckGameCompletion();

    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    void ShufflePuzzlePieces();

    UFUNCTION(BlueprintPure, Category = "Puzzle")
    TArray<APuzzlePiece*> GetPuzzlePieces() const { return PuzzlePieces; }

    UFUNCTION(BlueprintPure, Category = "Puzzle")
    int32 GetCompletedPiecesCount() const;

    UFUNCTION(BlueprintPure, Category = "Puzzle")
    float GetCompletionPercentage() const;

protected:
    // Internal fonksiyonlar
    void InitializePuzzle();
    void UpdateGameTimer();
    void OnGameComplete();

    UFUNCTION()
    void OnTimerTick();
};