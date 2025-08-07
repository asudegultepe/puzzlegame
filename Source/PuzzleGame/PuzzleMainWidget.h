// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PuzzleMainWidget.generated.h"

class UTextBlock;
class UWrapBox;
class UPuzzlePieceWidget;
class APuzzleGameMode;

/**
 * Main UI widget for puzzle game
 */
UCLASS()
class PUZZLEGAME_API UPuzzleMainWidget : public UUserWidget
{
    GENERATED_BODY()
    
public:
    // Initialize the widget
    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    void InitializeWidget();
    
    // Update game stats (matches OnStatsUpdated delegate signature)
    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    void UpdateGameStats(float Time, int32 Moves);
    
    // Populate piece list
    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    void PopulatePieceList();
    
    // Show completion screen
    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    void ShowCompletionScreen(float TotalTime, int32 TotalMoves);
    
    // Manual initialization with player controller
    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    void InitializeWithPlayerController(APlayerController* PC);
    
protected:
    virtual void NativeConstruct() override;
    
    // UI Components - bind these in Blueprint
    UPROPERTY(meta = (BindWidget))
    UTextBlock* TimerText;
    
    UPROPERTY(meta = (BindWidget))
    UTextBlock* MoveCounterText;
    
    UPROPERTY(meta = (BindWidget))
    UWrapBox* PieceListBox;
    
    // Widget class for puzzle pieces
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UI")
    TSubclassOf<UUserWidget> PuzzlePieceWidgetClass;
    
private:
    UFUNCTION()
    void OnPieceClicked(int32 PieceID);
    
    UPROPERTY()
    APuzzleGameMode* CachedGameMode;
};