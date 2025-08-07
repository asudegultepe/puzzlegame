// Fill out your copyright notice in the Description page of Project Settings.

#include "PuzzleMainWidget.h"
#include "PuzzlePieceWidget.h"
#include "PuzzleGameMode.h"
#include "PuzzlePlayerController.h"
#include "Components/TextBlock.h"
#include "Components/WrapBox.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"

void UPuzzleMainWidget::NativeConstruct()
{
    Super::NativeConstruct();
    
    // Delay initialization to next frame to ensure proper setup
    GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
    {
        InitializeWidget();
    });
}

void UPuzzleMainWidget::InitializeWidget()
{
    UE_LOG(LogTemp, Warning, TEXT("InitializeWidget called"));
    
    // Check if this widget has an owning player
    APlayerController* OwningPC = GetOwningPlayer();
    if (OwningPC)
    {
        UE_LOG(LogTemp, Warning, TEXT("Main widget has owning player: %s"), *OwningPC->GetClass()->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Main widget has NO owning player!"));
    }
    
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
    UE_LOG(LogTemp, Warning, TEXT("PopulatePieceList called"));
    
    if (!PieceListBox)
    {
        UE_LOG(LogTemp, Error, TEXT("PieceListBox is null! Make sure it's bound in Blueprint"));
        return;
    }
    
    if (!CachedGameMode)
    {
        UE_LOG(LogTemp, Error, TEXT("CachedGameMode is null!"));
        return;
    }
    
    if (!PuzzlePieceWidgetClass)
    {
        UE_LOG(LogTemp, Error, TEXT("PuzzlePieceWidgetClass is null! Set it in Blueprint"));
        return;
    }
    
    // Clear existing pieces
    PieceListBox->ClearChildren();
    
    // Get available pieces from game mode
    TArray<int32> AvailablePieces = CachedGameMode->GetAvailablePieceIDs();
    UE_LOG(LogTemp, Warning, TEXT("PopulatePieceList: Available pieces count: %d"), AvailablePieces.Num());
    
    // Debug: Show which pieces are available
    if (AvailablePieces.Num() > 0)
    {
        FString PieceList = TEXT("Available pieces: ");
        for (int32 ID : AvailablePieces)
        {
            PieceList += FString::Printf(TEXT("%d "), ID);
        }
        UE_LOG(LogTemp, Warning, TEXT("%s"), *PieceList);
    }
    
    // Check if PuzzlePieceWidgetClass is set
    if (!PuzzlePieceWidgetClass)
    {
        UE_LOG(LogTemp, Error, TEXT("PuzzlePieceWidgetClass is not set! Set it in WBP_MainWidget Blueprint"));
        return;
    }
    
    // Create widget for each available piece
    for (int32 PieceID : AvailablePieces)
    {
        UE_LOG(LogTemp, Warning, TEXT("Creating widget for piece %d"), PieceID);
        
        // Try to get a valid player controller
        APlayerController* PC = GetOwningPlayer();
        if (!PC)
        {
            PC = GetWorld()->GetFirstPlayerController();
        }
        
        if (!PC)
        {
            UE_LOG(LogTemp, Error, TEXT("No player controller found for widget creation!"));
            continue;
        }
        
        UUserWidget* NewWidget = CreateWidget<UUserWidget>(PC, PuzzlePieceWidgetClass);
        UPuzzlePieceWidget* PieceWidget = Cast<UPuzzlePieceWidget>(NewWidget);
        
        if (PieceWidget)
        {
            PieceWidget->SetPieceID(PieceID);
            PieceWidget->OnPieceClicked.AddDynamic(this, &UPuzzleMainWidget::OnPieceClicked);
            
            // Set material if GameMode has materials configured
            APuzzleGameMode* PuzzleGameMode = Cast<APuzzleGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
            if (PuzzleGameMode)
            {
                const TArray<UMaterialInterface*>& Materials = PuzzleGameMode->GetPieceMaterials();
                if (Materials.IsValidIndex(PieceID))
                {
                    PieceWidget->SetPieceMaterial(Materials[PieceID]);
                    UE_LOG(LogTemp, Warning, TEXT("Set material for piece %d"), PieceID);
                }
            }
            
            PieceListBox->AddChild(PieceWidget);
            UE_LOG(LogTemp, Warning, TEXT("Added piece %d to wrap box"), PieceID);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to create widget for piece %d"), PieceID);
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Wrap box now has %d children"), PieceListBox->GetChildrenCount());
}

void UPuzzleMainWidget::ShowCompletionScreen(float TotalTime, int32 TotalMoves)
{
    // This will be implemented in Blueprint for better UI control
    // For now, just log
    UE_LOG(LogTemp, Warning, TEXT("Game Complete! Time: %.1f, Moves: %d"), TotalTime, TotalMoves);
}

void UPuzzleMainWidget::InitializeWithPlayerController(APlayerController* PC)
{
    if (PC)
    {
        UE_LOG(LogTemp, Warning, TEXT("InitializeWithPlayerController called with PC: %s"), *PC->GetClass()->GetName());
        // This is a workaround - the widget should already have an owner from CreateWidget
        // but if it doesn't, this won't fix it as ownership is set during widget creation
    }
    
    InitializeWidget();
}

void UPuzzleMainWidget::OnPieceClicked(int32 PieceID)
{
    UE_LOG(LogTemp, Warning, TEXT("PuzzleMainWidget::OnPieceClicked - PieceID: %d"), PieceID);
    
    // Try multiple methods to get the player controller
    APlayerController* BasePC = nullptr;
    
    // Method 1: Try GetOwningPlayer
    BasePC = GetOwningPlayer();
    if (!BasePC)
    {
        UE_LOG(LogTemp, Warning, TEXT("GetOwningPlayer() failed, trying GetOwningLocalPlayer"));
        
        // Method 2: Try through local player
        ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
        if (LocalPlayer)
        {
            BasePC = LocalPlayer->GetPlayerController(GetWorld());
            UE_LOG(LogTemp, Warning, TEXT("Got PlayerController through LocalPlayer"));
        }
    }
    
    if (!BasePC)
    {
        // Method 3: Get from world as last resort
        BasePC = GetWorld()->GetFirstPlayerController();
        UE_LOG(LogTemp, Warning, TEXT("Had to get PlayerController from World"));
    }
    
    if (BasePC)
    {
        UE_LOG(LogTemp, Warning, TEXT("Found base PlayerController of class: %s"), *BasePC->GetClass()->GetName());
        
        APuzzlePlayerController* PC = Cast<APuzzlePlayerController>(BasePC);
        if (PC)
        {
            UE_LOG(LogTemp, Warning, TEXT("Cast to PuzzlePlayerController successful, calling StartDragFromUI"));
            
            // Start drag from UI
            PC->StartDragFromUI(PieceID);
            
            // Refresh the piece list after a small delay to ensure piece is removed
            GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
            {
                PopulatePieceList();
            });
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Cast to PuzzlePlayerController failed! BasePC class: %s"), *BasePC->GetClass()->GetName());
            UE_LOG(LogTemp, Error, TEXT("Make sure BP_PuzzleGameMode uses BP_PuzzlePlayerController as Player Controller Class"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Could not find any PlayerController!"));
    }
}