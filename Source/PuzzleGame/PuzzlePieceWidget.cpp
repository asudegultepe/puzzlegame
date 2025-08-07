// Fill out your copyright notice in the Description page of Project Settings.

#include "PuzzlePieceWidget.h"
#include "PuzzlePieceDragDropOperation.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/Engine.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Styling/SlateBrush.h"

void UPuzzlePieceWidget::SetPieceID(int32 NewPieceID)
{
    PieceID = NewPieceID;
    UE_LOG(LogTemp, Warning, TEXT("PuzzlePieceWidget: Set PieceID to %d"), PieceID);
}

void UPuzzlePieceWidget::SetPieceMaterial(UMaterialInterface* Material)
{
    PieceMaterial = Material;
    UE_LOG(LogTemp, Warning, TEXT("PuzzlePieceWidget: Set material for piece %d, Material: %s"), 
        PieceID, Material ? *Material->GetName() : TEXT("NULL"));
    
    // Call Blueprint event to update button appearance
    OnMaterialSet();
}

void UPuzzlePieceWidget::NativeConstruct()
{
    Super::NativeConstruct();
    
    // Ensure widget is visible and can receive input
    SetVisibility(ESlateVisibility::Visible);
    
    UE_LOG(LogTemp, Warning, TEXT("PuzzlePieceWidget: NativeConstruct called for piece %d"), PieceID);
}

FText UPuzzlePieceWidget::GetPieceDisplayText() const
{
    return FText::FromString(FString::Printf(TEXT("Piece %d"), PieceID + 1));
}

void UPuzzlePieceWidget::HandleClick()
{
    UE_LOG(LogTemp, Warning, TEXT("HandleClick called for piece %d"), PieceID);
    UE_LOG(LogTemp, Warning, TEXT("OnPieceClicked has %d bindings"), OnPieceClicked.IsBound() ? 1 : 0);
    
    OnPieceClicked.Broadcast(PieceID);
    
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, 
            FString::Printf(TEXT("Piece %d clicked in UI"), PieceID));
    }
}

void UPuzzlePieceWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);
    
    UE_LOG(LogTemp, Warning, TEXT("Drag detected for piece %d"), PieceID);
    
    // Create drag drop operation
    UPuzzlePieceDragDropOperation* DragDropOp = NewObject<UPuzzlePieceDragDropOperation>();
    DragDropOp->PieceID = PieceID;
    
    // Create a visual widget for dragging (optional)
    if (UUserWidget* DragVisual = CreateWidget<UUserWidget>(GetWorld(), GetClass()))
    {
        if (UPuzzlePieceWidget* PieceVisual = Cast<UPuzzlePieceWidget>(DragVisual))
        {
            PieceVisual->SetPieceID(PieceID);
        }
        DragDropOp->DragVisual = DragVisual;
        DragDropOp->DefaultDragVisual = DragVisual;
        DragDropOp->Pivot = EDragPivot::CenterCenter;
    }
    
    OutOperation = DragDropOp;
    
    // Broadcast the click event when drag starts
    HandleClick();
}

FReply UPuzzlePieceWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    UE_LOG(LogTemp, Warning, TEXT("Mouse button down on piece %d"), PieceID);
    
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        // Call HandleClick immediately on mouse down
        HandleClick();
        
        // Detect drag for this widget
        return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
    }
    
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}