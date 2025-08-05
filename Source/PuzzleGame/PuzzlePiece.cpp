// Fill out your copyright notice in the Description page of Project Settings.


#include "PuzzlePiece.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"

APuzzlePiece::APuzzlePiece()
{
    PrimaryActorTick.bCanEverTick = true;

    // Root component olarak collision box olu?tur
    CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
    RootComponent = CollisionBox;
    CollisionBox->SetBoxExtent(FVector(50.0f, 50.0f, 25.0f));
    CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    CollisionBox->SetCollisionResponseToAllChannels(ECR_Block);

    // Mesh komponenti olu?tur
    PieceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PieceMesh"));
    PieceMesh->SetupAttachment(RootComponent);

    // Varsay?lan de?erler
    PieceID = -1;
    CorrectPosition = FVector::ZeroVector;
    bIsInCorrectPosition = false;
    bIsSelected = false;
    PositionTolerance = 100.0f;
    MoveSpeed = 1000.0f;
    bIsMoving = false;

    // Overlap event'ini ba?la
    CollisionBox->OnComponentBeginOverlap.AddDynamic(this, &APuzzlePiece::OnOverlapBegin);
}

void APuzzlePiece::BeginPlay()
{
    Super::BeginPlay();

    // Ba?lang?çta pozisyon kontrolü yap
    CheckIfInCorrectPosition();
}

void APuzzlePiece::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Smooth movement için
    if (bIsMoving)
    {
        FVector CurrentLocation = GetActorLocation();
        FVector NewLocation = FMath::VInterpTo(CurrentLocation, TargetLocation, DeltaTime, MoveSpeed / 100.0f);
        SetActorLocation(NewLocation);

        // Hedefe ula?t?k m? kontrol et
        if (FVector::Dist(NewLocation, TargetLocation) < 5.0f)
        {
            SetActorLocation(TargetLocation);
            bIsMoving = false;

            // Pozisyon kontrolü yap
            CheckIfInCorrectPosition();
        }
    }
}

bool APuzzlePiece::CheckIfInCorrectPosition()
{
    if (CorrectPosition == FVector::ZeroVector)
    {
        // Do?ru pozisyon henüz set edilmemi?
        bIsInCorrectPosition = false;
        return false;
    }

    float Distance = FVector::Dist(GetActorLocation(), CorrectPosition);
    bool bWasPreviouslyCorrect = bIsInCorrectPosition;
    bIsInCorrectPosition = Distance <= PositionTolerance;

    // Durum de?i?ti mi kontrol et
    if (bIsInCorrectPosition && !bWasPreviouslyCorrect)
    {
        // Do?ru pozisyona yerle?tirildi
        OnCorrectPlacement();

        // Debug mesaj?
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
                FString::Printf(TEXT("Piece %d placed correctly!"), PieceID));
        }
    }
    else if (!bIsInCorrectPosition && bWasPreviouslyCorrect)
    {
        // Do?ru pozisyondan ç?kar?ld?
        OnIncorrectPlacement();
    }

    return bIsInCorrectPosition;
}

void APuzzlePiece::MovePieceToLocation(FVector NewLocation, bool bSmoothMove)
{
    if (bSmoothMove)
    {
        TargetLocation = NewLocation;
        bIsMoving = true;
    }
    else
    {
        SetActorLocation(NewLocation);
        bIsMoving = false;
        CheckIfInCorrectPosition();
    }
}

void APuzzlePiece::SetSelected(bool bSelected)
{
    if (bIsSelected != bSelected)
    {
        bIsSelected = bSelected;

        if (bIsSelected)
        {
            OnPieceSelected();

            // Debug için seçili parçay? vurgula
            if (PieceMesh && PieceMesh->GetMaterial(0))
            {
                // Burada material parametresi de?i?tirilebilir
            }
        }
        else
        {
            OnPieceDeselected();
        }
    }
}

void APuzzlePiece::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    // Ba?ka bir puzzle parças? ile overlap oldu mu kontrol et
    APuzzlePiece* OtherPiece = Cast<APuzzlePiece>(OtherActor);
    if (OtherPiece && OtherPiece != this)
    {
        // ?ki parçan?n pozisyonunu de?i?tir
        FVector MyLocation = GetActorLocation();
        FVector OtherLocation = OtherPiece->GetActorLocation();

        MovePieceToLocation(OtherLocation, true);
        OtherPiece->MovePieceToLocation(MyLocation, true);

        // Debug mesaj?
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow,
                FString::Printf(TEXT("Swapped pieces %d and %d"), PieceID, OtherPiece->GetPieceID()));
        }
    }
}