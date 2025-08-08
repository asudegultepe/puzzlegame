// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PuzzlePieceWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPieceClicked, int32, PieceID);

/**
 * Widget for individual puzzle piece in UI
 */
UCLASS()
class PUZZLEGAME_API UPuzzlePieceWidget : public UUserWidget
{
    GENERATED_BODY()
    
public:
    // Set the piece ID for this widget
    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    void SetPieceID(int32 NewPieceID);
    
    // Get the piece ID
    UFUNCTION(BlueprintPure, Category = "Puzzle")
    int32 GetPieceID() const { return PieceID; }
    
    // Get display text for the piece
    UFUNCTION(BlueprintPure, Category = "Puzzle")
    FText GetPieceDisplayText() const;
    
    // Set the material/texture for this piece
    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    void SetPieceMaterial(UMaterialInterface* Material);
    
    // Get the material for this piece
    UFUNCTION(BlueprintPure, Category = "Puzzle")
    UMaterialInterface* GetPieceMaterial() const { return PieceMaterial; }
    
    // Blueprint event to update button appearance
    UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle")
    void OnMaterialSet();
    
    // Hide this widget when piece is placed
    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    void HideWidget();
    
    // Bind to button in Blueprint
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "UI")
    class UButton* PieceButton;
    
    // Event when piece is clicked/dragged
    UPROPERTY(BlueprintAssignable, Category = "Puzzle")
    FOnPieceClicked OnPieceClicked;
    
protected:
    // Called when the widget is clicked
    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    void HandleClick();
    
    // Called when drag is detected
    virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    
    // Called when widget is constructed
    virtual void NativeConstruct() override;
    
private:
    UPROPERTY()
    int32 PieceID;
    
    UPROPERTY()
    UMaterialInterface* PieceMaterial;
};