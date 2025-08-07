// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Engine.h"
#include "PuzzlePiece.generated.h"

UCLASS()
class PUZZLEGAME_API APuzzlePiece : public AActor
{
    GENERATED_BODY()

public:
    APuzzlePiece();

protected:
    virtual void BeginPlay() override;

    // Mesh komponenti - puzzle parçasının görsel temsili
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* PieceMesh;

    // Collision komponenti - etkileşim için
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBoxComponent* CollisionBox;

    // Puzzle parçasının benzersiz ID'si
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
    int32 PieceID;

    // Bu parçanın doğru olması gereken pozisyon
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
    FVector CorrectPosition;

    // Parçanın şu anda doğru konumda olup olmadığı
    UPROPERTY(BlueprintReadOnly, Category = "Puzzle")
    bool bIsInCorrectPosition;

    // Parçanın seçili olup olmadığı (sürükleme için)
    UPROPERTY(BlueprintReadOnly, Category = "Puzzle")
    bool bIsSelected;

    // Doğru pozisyona ne kadar yakın olması gerektiği (tolerance)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
    float PositionTolerance;

public:
    virtual void Tick(float DeltaTime) override;

    // Parçanın doğru konumda olup olmadığını kontrol et
    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    bool CheckIfInCorrectPosition();

    // Parçayı belirtilen konuma taşı (animasyonlu)
    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    void MovePieceToLocation(FVector NewLocation, bool bSmoothMove = true);

    // Parçayı seç/seçimi kaldır
    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    void SetSelected(bool bSelected);

    // Getter fonksiyonları
    UFUNCTION(BlueprintPure, Category = "Puzzle")
    int32 GetPieceID() const { return PieceID; }

    UFUNCTION(BlueprintPure, Category = "Puzzle")
    FVector GetCorrectPosition() const { return CorrectPosition; }

    UFUNCTION(BlueprintPure, Category = "Puzzle")
    bool IsInCorrectPosition() const { return bIsInCorrectPosition; }

    // Setter fonksiyonları
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

    // Debug utility function
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void DebugPrintInfo();

protected:
    // Overlap event'leri
    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

private:
    // Smooth movement için
    FVector TargetLocation;
    bool bIsMoving;
    float MoveSpeed;
};