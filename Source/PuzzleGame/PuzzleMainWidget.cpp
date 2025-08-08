// Fill out your copyright notice in the Description page of Project Settings.

#include "PuzzleMainWidget.h"
#include "PuzzlePieceWidget.h"
#include "PuzzleGameMode.h"
#include "PuzzlePlayerController.h"
#include "Components/TextBlock.h"
#include "Components/WrapBox.h"
#include "Components/PanelWidget.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"

void UPuzzleMainWidget::NativeConstruct()
{
    Super::NativeConstruct();
    
    // Check owning player
    APlayerController* OwningPC = GetOwningPlayer();
    
    
    // Try to find parent widget
    UPanelWidget* ParentPanel = GetParent();
    if (ParentPanel)
    {
    }
    
    // Don't process widgets without proper ownership
    if (!OwningPC)
    {
        // Force remove this widget
        RemoveFromParent();
        return;
    }
    
    // Only initialize if not already initialized and not scheduled
    if (!bIsInitialized && !bInitializationScheduled)
    {
        bInitializationScheduled = true;
        // Delay initialization to next frame to ensure proper setup
        GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
        {
            InitializeWidget();
            bInitializationScheduled = false;
        });
    }
}

void UPuzzleMainWidget::InitializeWidget()
{
    // Prevent multiple initialization
    if (bIsInitialized)
    {
        return;
    }
    
    // Check if this widget has an owning player
    APlayerController* OwningPC = GetOwningPlayer();
    if (!OwningPC)
    {
        return;
    }
    
    bIsInitialized = true;
    
    // Get game mode reference
    CachedGameMode = Cast<APuzzleGameMode>(UGameplayStatics::GetGameMode(this));
    
    if (CachedGameMode)
    {
        // Bind to game events
        CachedGameMode->OnStatsUpdated.AddDynamic(this, &UPuzzleMainWidget::UpdateGameStats);
        CachedGameMode->OnGameCompleted.AddDynamic(this, &UPuzzleMainWidget::ShowCompletionScreen);
        
        // Populate the piece list
        PopulatePieceList();
    }
    
    // Initialize displays
    UpdateGameStats(0.0f, 0);
}

void UPuzzleMainWidget::UpdateGameStats(float Time, int32 Moves)
{
    // Update timer
    if (TimerText)
    {
        int32 Minutes = FMath::FloorToInt(Time / 60.0f);
        int32 Seconds = FMath::FloorToInt(Time) % 60;
        
        FString TimeString = FString::Printf(TEXT("Time: %02d:%02d"), Minutes, Seconds);
        TimerText->SetText(FText::FromString(TimeString));
    }
    
    // Update move counter
    if (MoveCounterText)
    {
        FString MovesString = FString::Printf(TEXT("Moves: %d"), Moves);
        MoveCounterText->SetText(FText::FromString(MovesString));
    }
}

void UPuzzleMainWidget::PopulatePieceList()
{
    if (!PieceListBox)
    {
        return;
    }
    
    if (!CachedGameMode)
    {
        return;
    }
    
    if (!PuzzlePieceWidgetClass)
    {
        return;
    }
    
    // Get available pieces from game mode
    TArray<int32> AvailablePieces = CachedGameMode->GetAvailablePieceIDs();
    
    // Simple approach: Clear all and recreate
    PieceListBox->ClearChildren();
    
    // Create widget for each available piece
    for (int32 PieceID : AvailablePieces)
    {
        // Try to get a valid player controller
        APlayerController* PC = GetOwningPlayer();
        if (!PC)
        {
            PC = GetWorld()->GetFirstPlayerController();
        }
        
        if (!PC)
        {
            continue;
        }
        
        UUserWidget* NewWidget = CreateWidget<UUserWidget>(PC, PuzzlePieceWidgetClass);
        UPuzzlePieceWidget* PieceWidget = Cast<UPuzzlePieceWidget>(NewWidget);
        
        if (PieceWidget)
        {
            PieceWidget->SetPieceID(PieceID);
            PieceWidget->OnPieceClicked.AddDynamic(this, &UPuzzleMainWidget::OnPieceClicked);
            
            // Set material if GameMode has materials configured
            const TArray<UMaterialInterface*>& Materials = CachedGameMode->GetPieceMaterials();
            if (Materials.IsValidIndex(PieceID))
            {
                PieceWidget->SetPieceMaterial(Materials[PieceID]);
            }
            
            PieceListBox->AddChild(PieceWidget);
        }
    }
}

void UPuzzleMainWidget::RefreshPieceList()
{
    PopulatePieceList();
}

void UPuzzleMainWidget::ShowCompletionScreen(float TotalTime, int32 TotalMoves)
{
    
    // Check if we have a completion widget class set
    if (!GameCompleteWidgetClass)
    {
        return;
    }
    
    // Get player controller
    APlayerController* PC = GetOwningPlayer();
    if (!PC)
    {
        PC = GetWorld()->GetFirstPlayerController();
    }
    
    if (!PC)
    {
        return;
    }
    
    // Create the completion widget
    UUserWidget* CompletionWidget = CreateWidget<UUserWidget>(PC, GameCompleteWidgetClass);
    if (CompletionWidget)
    {
        // If the completion widget has properties for time and moves, we could set them here
        // For now, just show it
        CompletionWidget->AddToViewport(10); // Higher Z-order to show on top
        
        // Optionally pause the game
        PC->SetPause(true);
        
        // Set input mode to UI only
        FInputModeUIOnly InputMode;
        InputMode.SetWidgetToFocus(CompletionWidget->TakeWidget());
        PC->SetInputMode(InputMode);
        PC->bShowMouseCursor = true;
        
    }
    else
    {
    }
}

void UPuzzleMainWidget::InitializeWithPlayerController(APlayerController* PC)
{
    // Only initialize if not already initialized and not scheduled
    if (!bIsInitialized && !bInitializationScheduled)
    {
        InitializeWidget();
    }
}

void UPuzzleMainWidget::OnPieceClicked(int32 PieceID)
{
    
    // Try multiple methods to get the player controller
    APlayerController* BasePC = nullptr;
    
    // Method 1: Try GetOwningPlayer
    BasePC = GetOwningPlayer();
    if (!BasePC)
    {
        
        // Method 2: Try through local player
        ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
        if (LocalPlayer)
        {
            BasePC = LocalPlayer->GetPlayerController(GetWorld());
        }
    }
    
    if (!BasePC)
    {
        // Method 3: Get from world as last resort
        BasePC = GetWorld()->GetFirstPlayerController();
    }
    
    if (BasePC)
    {
        
        APuzzlePlayerController* PC = Cast<APuzzlePlayerController>(BasePC);
        if (PC)
        {
            
            // Start drag from UI
            PC->StartDragFromUI(PieceID);
            
            // No need to refresh immediately - it will be done in StartDragFromUI
        }
        else
        {
        }
    }
    else
    {
    }
}