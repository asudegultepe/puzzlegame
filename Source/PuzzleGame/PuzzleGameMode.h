// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PuzzlePiece.h"
#include "Engine/TimerHandle.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMeshActor.h"
#include "DrawDebugHelpers.h"
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
    virtual ~APuzzleGameMode();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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
    
    // Available piece IDs for UI (shuffled)
    UPROPERTY(BlueprintReadOnly, Category = "Puzzle")
    TArray<int32> AvailablePieceIDs;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Puzzle")
    TSubclassOf<APuzzlePiece> PuzzlePieceClass;
    
    // Piece materials array - set in Blueprint
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Puzzle")
    TArray<UMaterialInterface*> PieceMaterials;

    // Timer handle
    FTimerHandle GameTimerHandle;

    // Grid visualization - NEW
    UPROPERTY(BlueprintReadOnly, Category = "Grid")
    TArray<AStaticMeshActor*> GridMarkers;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    bool bShowGridMarkers;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    float GridMarkerScale;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    FLinearColor GridMarkerColor;

    // Grid marker mesh
    UPROPERTY()
    UStaticMesh* GridMarkerMesh;

    // Boundary constraint - NEW
    UPROPERTY(BlueprintReadOnly, Category = "Boundary")
    FVector BoundaryMin;

    UPROPERTY(BlueprintReadOnly, Category = "Boundary")
    FVector BoundaryMax;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boundary")
    bool bEnableBoundaryConstraint;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boundary")
    float BoundaryPadding;

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
    
    UFUNCTION(BlueprintPure, Category = "Puzzle")
    const TArray<UMaterialInterface*>& GetPieceMaterials() const { return PieceMaterials; }

    // Grid visualization functions - NEW
    UFUNCTION(BlueprintCallable, Category = "Grid")
    void CreateGridVisualization();

    UFUNCTION(BlueprintCallable, Category = "Grid")
    void ClearGridVisualization();

    UFUNCTION(BlueprintCallable, Category = "Grid")
    void ToggleGridVisualization();

    UFUNCTION(BlueprintCallable, Category = "Grid")
    void RefreshGridVisualization();

    // Boundary constraint functions - NEW
    UFUNCTION(BlueprintCallable, Category = "Boundary")
    void CalculateBoundary();

    UFUNCTION(BlueprintCallable, Category = "Boundary")
    bool IsLocationWithinBoundary(const FVector& Location);

    UFUNCTION(BlueprintCallable, Category = "Boundary")
    FVector ClampLocationToBoundary(const FVector& Location);

    UFUNCTION(BlueprintCallable, Category = "Boundary")
    void EnforceAllPieceBoundaries();

    UFUNCTION(BlueprintCallable, Category = "Boundary")
    void SetBoundaryPadding(float NewPadding);

    UFUNCTION(BlueprintPure, Category = "Boundary")
    bool IsBoundaryConstraintEnabled() const { return bEnableBoundaryConstraint; }
    
    float GetPieceSpacing() const { return PieceSpacing; }

    // Grid snapping function
    UFUNCTION(BlueprintCallable, Category = "Grid")
    FVector GetNearestGridPosition(const FVector& WorldPosition);
    
    // Grid ID based functions
    UFUNCTION(BlueprintCallable, Category = "Grid")
    int32 GetGridIDFromPosition(const FVector& WorldPosition);
    
    UFUNCTION(BlueprintCallable, Category = "Grid")
    FVector GetGridPositionFromID(int32 GridID);
    
    // Find piece at grid position
    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    APuzzlePiece* GetPieceAtGridPosition(const FVector& GridPosition);
    
    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    APuzzlePiece* GetPieceAtGridID(int32 GridID);
    
    // Grid occupation management
    UFUNCTION(BlueprintCallable, Category = "Grid")
    void UpdateGridOccupancy(int32 GridID, APuzzlePiece* Piece);
    
    UFUNCTION(BlueprintCallable, Category = "Grid")
    void SwapPiecesAtGridIDs(int32 GridID1, int32 GridID2);
    
    // Get available pieces for UI
    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    TArray<int32> GetAvailablePieceIDs() const { return AvailablePieceIDs; }
    
    // Remove piece from available list when spawned
    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    void RemovePieceFromAvailable(int32 PieceID);

    // Debug functions - NEW
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void DrawBoundaryDebug();

    UFUNCTION(BlueprintCallable, Category = "Debug")
    void DebugGridSystem();

    UFUNCTION(BlueprintCallable, Category = "Debug")
    void PrintAllPiecePositions();
    
    UFUNCTION(BlueprintCallable, Category = "Debug", Exec)
    void ForceCheckGameCompletion();
    
    UFUNCTION(BlueprintCallable, Category = "Debug", Exec)
    void DebugPuzzleState();

protected:
    // Internal fonksiyonlar
    void InitializePuzzle();
    void UpdateGameTimer();
    void OnGameComplete();

    UFUNCTION()
    void OnTimerTick();

    // Grid internal functions - NEW
    void CreateGridMarker(const FVector& Position, int32 GridIndex);
    void UpdateGridMarkerVisibility();
    void SetGridMarkerMaterial(AStaticMeshActor* GridMarker, const FLinearColor& Color, float Opacity = 0.3f);

    // Boundary internal functions - NEW
    void UpdateBoundaryConstraints();
    bool ValidatePuzzleConfiguration();

private:
    // Internal state tracking - NEW
    bool bGridInitialized;
    bool bBoundaryCalculated;
    int32 LastBoundaryCheckFrame;

    // Performance optimization - NEW
    UPROPERTY()
    TArray<UMaterialInstanceDynamic*> GridMarkerMaterials;
    
    // Grid occupation tracking - NEW
    // Using array instead of TMap to avoid pointer issues
    UPROPERTY()
    TArray<int32> GridOccupancy; // GridID -> PieceID mapping (-1 means empty)
};