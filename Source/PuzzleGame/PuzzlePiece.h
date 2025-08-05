// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "PuzzlePiece.generated.h"

UCLASS()
class PUZZLEGAME_API APuzzlePiece : public AActor
{
    GENERATED_BODY()

public:
    APuzzlePiece();

protected:
    virtual void BeginPlay() override;

    // Mesh komponenti - puzzle par�as?n?n g�rsel temsili
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* PieceMesh;

    // Collision komponenti - etkile?im i�in
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBoxComponent* CollisionBox;

    // Puzzle par�as?n?n benzersiz ID'si
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
    int32 PieceID;

    // Bu par�an?n do?ru olmas? gereken pozisyon
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
    FVector CorrectPosition;

    // Par�an?n ?u anda do?ru konumda olup olmad???
    UPROPERTY(BlueprintReadOnly, Category = "Puzzle")
    bool bIsInCorrectPosition;

    // Par�an?n se�ili olup olmad??? (s�r�kleme i�in)
    UPROPERTY(BlueprintReadOnly, Category = "Puzzle")
    bool bIsSelected;

    // Do?ru pozisyona ne kadar yak?n olmas? gerekti?i (tolerance)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
    float PositionTolerance;

public:
    virtual void Tick(float DeltaTime) override;

    // Par�an?n do?ru konumda olup olmad???n? kontrol et
    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    bool CheckIfInCorrectPosition();

    // Par�ay? belirtilen konuma ta?? (animasyonlu)
    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    void MovePieceToLocation(FVector NewLocation, bool bSmoothMove = true);

    // Par�ay? se�/se�imi kald?r
    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    void SetSelected(bool bSelected);

    // Getter fonksiyonlar?
    UFUNCTION(BlueprintPure, Category = "Puzzle")
    int32 GetPieceID() const { return PieceID; }

    UFUNCTION(BlueprintPure, Category = "Puzzle")
    FVector GetCorrectPosition() const { return CorrectPosition; }

    UFUNCTION(BlueprintPure, Category = "Puzzle")
    bool IsInCorrectPosition() const { return bIsInCorrectPosition; }

    // Setter fonksiyonlar?
    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    void SetPieceID(int32 NewID) { PieceID = NewID; }

    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    void SetCorrectPosition(FVector NewPosition) { CorrectPosition = NewPosition; }

    // Blueprint'te override edilebilir event'ler
    UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle")
    void OnCorrectPlacement();

    UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle")
    void OnIncorrectPlacement();

    UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle")
    void OnPieceSelected();

    UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle")
    void OnPieceDeselected();

protected:
    // Overlap event'leri
    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

private:
    // Smooth movement i�in
    FVector TargetLocation;
    bool bIsMoving;
    float MoveSpeed;
};
