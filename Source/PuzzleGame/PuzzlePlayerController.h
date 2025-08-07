// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PuzzlePiece.h"
#include "PuzzleGameMode.h"
#include "Engine/Engine.h"
#include "Blueprint/UserWidget.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "PuzzlePlayerController.generated.h"

// Mouse interaction states
UENUM(BlueprintType)
enum class EMouseInteractionState : uint8
{
    None            UMETA(DisplayName = "None"),
    DraggingFromUI  UMETA(DisplayName = "Dragging From UI"),
    DraggingPiece   UMETA(DisplayName = "Dragging 3D Piece"),
    Hovering        UMETA(DisplayName = "Hovering")
};

UCLASS()
class PUZZLEGAME_API APuzzlePlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    APuzzlePlayerController();

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void Tick(float DeltaTime) override;

    // Mouse interaction state
    UPROPERTY(BlueprintReadOnly, Category = "Input")
    EMouseInteractionState CurrentInteractionState;

    // Currently selected/dragged piece
    UPROPERTY(BlueprintReadOnly, Category = "Puzzle")
    APuzzlePiece* SelectedPiece;

    // Enhanced Input System
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enhanced Input")
    class UInputMappingContext* DefaultMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enhanced Input")
    class UInputAction* LeftClickAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enhanced Input")
    class UInputAction* RightClickAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enhanced Input")
    class UInputAction* ToggleUIAction;

    // UI Widget references
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UUserWidget> MainWidgetClass;

    UPROPERTY(BlueprintReadOnly, Category = "UI")
    UUserWidget* MainWidget;

    // Mouse position tracking
    UPROPERTY(BlueprintReadOnly, Category = "Input")
    FVector2D CurrentMousePosition;

    UPROPERTY(BlueprintReadOnly, Category = "Input")
    FVector MouseWorldPosition;

    UPROPERTY(BlueprintReadOnly, Category = "Input")
    FVector MouseWorldDirection;

    // Drag settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drag Settings")
    float DragHeight;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drag Settings")
    float DragSmoothness;

    // Line trace settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace Settings")
    float TraceDistance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace Settings")
    TEnumAsByte<ECollisionChannel> TraceChannel;

public:
    // Enhanced Input callbacks
    UFUNCTION()
    void OnLeftClickPressed(const FInputActionValue& Value);

    UFUNCTION()
    void OnLeftClickReleased(const FInputActionValue& Value);

    UFUNCTION()
    void OnRightClickPressed(const FInputActionValue& Value);

    UFUNCTION()
    void OnToggleUI(const FInputActionValue& Value);

    // Drag and drop functions
    UFUNCTION(BlueprintCallable, Category = "Drag Drop")
    void StartDragFromUI(int32 PieceID);

    UFUNCTION(BlueprintCallable, Category = "Drag Drop")
    void StartDragPiece(APuzzlePiece* Piece);

    UFUNCTION(BlueprintCallable, Category = "Drag Drop")
    void EndDrag();

    UFUNCTION(BlueprintCallable, Category = "Drag Drop")
    void UpdateDragPosition();

    // Line trace functions
    UFUNCTION(BlueprintCallable, Category = "Trace")
    bool TraceUnderMouse(FHitResult& HitResult);

    UFUNCTION(BlueprintCallable, Category = "Trace")
    FVector GetMouseWorldLocation();

    UFUNCTION(BlueprintCallable, Category = "Trace")
    APuzzlePiece* GetPuzzlePieceUnderMouse();

    // UI management
    UFUNCTION(BlueprintCallable, Category = "UI")
    void ShowMainWidget();

    UFUNCTION(BlueprintCallable, Category = "UI")
    void HideMainWidget();

    UFUNCTION(BlueprintCallable, Category = "UI")
    void ToggleMainWidget();

    // Utility functions
    UFUNCTION(BlueprintPure, Category = "Input")
    EMouseInteractionState GetCurrentInteractionState() const { return CurrentInteractionState; }

    UFUNCTION(BlueprintPure, Category = "Puzzle")
    APuzzlePiece* GetSelectedPiece() const { return SelectedPiece; }

    UFUNCTION(BlueprintPure, Category = "Input")
    FVector2D GetCurrentMousePosition() const { return CurrentMousePosition; }

    // Blueprint implementable events
    UFUNCTION(BlueprintImplementableEvent, Category = "Events")
    void OnPieceSelected(APuzzlePiece* Piece);

    UFUNCTION(BlueprintImplementableEvent, Category = "Events")
    void OnPieceDeselected(APuzzlePiece* Piece);

    UFUNCTION(BlueprintImplementableEvent, Category = "Events")
    void OnDragStarted(APuzzlePiece* Piece);

    UFUNCTION(BlueprintImplementableEvent, Category = "Events")
    void OnDragEnded(APuzzlePiece* Piece);

protected:
    // Internal helper functions
    void UpdateMousePosition();
    void HandlePieceSelection();
    void HandleDragUpdate();
    APuzzleGameMode* GetPuzzleGameMode();
    
public:
    // Debug commands
    UFUNCTION(Exec)
    void DebugPuzzle();
    
    UFUNCTION(Exec)
    void CheckPuzzleComplete();

private:
    // Drag state variables
    FVector DragStartLocation;
    FVector DragOffset;
    bool bIsDragging;
    bool bMousePressed;

    // Reference caching
    APuzzleGameMode* CachedGameMode;
};