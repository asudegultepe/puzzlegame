// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "PuzzlePieceDragDropOperation.generated.h"

/**
 * Drag drop operation for puzzle pieces
 */
UCLASS()
class PUZZLEGAME_API UPuzzlePieceDragDropOperation : public UDragDropOperation
{
    GENERATED_BODY()
    
public:
    // The piece ID being dragged
    UPROPERTY(BlueprintReadWrite, Category = "Puzzle")
    int32 PieceID;
    
    // Visual widget to show during drag
    UPROPERTY(BlueprintReadWrite, Category = "Puzzle")
    class UUserWidget* DragVisual;
};