// Fill out your copyright notice in the Description page of Project Settings.

#include "PuzzlePiece.h"
#include "PuzzleGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"

APuzzlePiece::APuzzlePiece()
{
    PrimaryActorTick.bCanEverTick = true;

    // Root component olarak collision box oluştur
    CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
    RootComponent = CollisionBox;
    CollisionBox->SetBoxExtent(FVector(50.0f, 50.0f, 25.0f));
    CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    CollisionBox->SetCollisionResponseToAllChannels(ECR_Block);
    CollisionBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    // Mesh komponenti oluştur
    PieceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PieceMesh"));
    PieceMesh->SetupAttachment(RootComponent);

    // Varsayılan değerler
    PieceID = -1;
    CorrectPosition = FVector::ZeroVector;
    bIsInCorrectPosition = false;
    bIsSelected = false;
    PositionTolerance = 100.0f;
    MoveSpeed = 1000.0f;
    bIsMoving = false;

    // Default scale (1,1,1) garantisi
    SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));

    // Overlap event'ini bağla
    CollisionBox->OnComponentBeginOverlap.AddDynamic(this, &APuzzlePiece::OnOverlapBegin);
}

void APuzzlePiece::BeginPlay()
{
    Super::BeginPlay();

    // Başlangıçta pozisyon kontrolü yap
    CheckIfInCorrectPosition();
}

void APuzzlePiece::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Only do smooth movement if not being selected/dragged
    if (bIsMoving && !bIsSelected)
    {
        FVector CurrentLocation = GetActorLocation();
        FVector NewLocation = FMath::VInterpTo(CurrentLocation, TargetLocation, DeltaTime, MoveSpeed / 100.0f);

        // Z koordinatını sabit tut (XY movement constraint)
        NewLocation.Z = 0.0f;

        SetActorLocation(NewLocation);

        // Hedefe ulaştık mı kontrol et (2D distance kullan)
        if (FVector::Dist2D(NewLocation, TargetLocation) < 5.0f)
        {
            TargetLocation.Z = 0.0f; // Target'ı da Z=0 yap
            SetActorLocation(TargetLocation);
            bIsMoving = false;

            // Pozisyon kontrolü yap
            CheckIfInCorrectPosition();
        }
    }
}

bool APuzzlePiece::CheckIfInCorrectPosition()
{
    // Remove the ZeroVector check - (0,0,0) is a valid position for center piece!
    
    // 2D distance kullan (Z ignore et)
    float Distance = FVector::Dist2D(GetActorLocation(), CorrectPosition);
    bool bWasPreviouslyCorrect = bIsInCorrectPosition;
    bIsInCorrectPosition = Distance <= PositionTolerance;

    // Durum değişti mi kontrol et
    if (bIsInCorrectPosition && !bWasPreviouslyCorrect)
    {
        // Doğru pozisyona yerleştirildi
        OnCorrectPlacement();
    }
    else if (!bIsInCorrectPosition && bWasPreviouslyCorrect)
    {
        // Doğru pozisyondan çıkarıldı
        OnIncorrectPlacement();
    }

    return bIsInCorrectPosition;
}

void APuzzlePiece::MovePieceToLocation(FVector NewLocation, bool bSmoothMove)
{
    // Z koordinatını sabit tut (ground level) - XY movement constraint
    NewLocation.Z = 0.0f;

    // Boundary constraint check - but skip if piece is being dragged
    if (!bIsSelected)
    {
        APuzzleGameMode* GameMode = Cast<APuzzleGameMode>(GetWorld()->GetAuthGameMode());
        if (GameMode && GameMode->IsBoundaryConstraintEnabled())
        {
            if (!GameMode->IsLocationWithinBoundary(NewLocation))
            {
                NewLocation = GameMode->ClampLocationToBoundary(NewLocation);
            }
        }
    }


    if (bSmoothMove)
    {
        TargetLocation = NewLocation;
        bIsMoving = true;
    }
    else
    {
        // For instant move, cancel any ongoing smooth movement
        bIsMoving = false;
        TargetLocation = NewLocation;
        
        SetActorLocation(NewLocation);
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
    // Overlap detection for visual feedback only
    // Actual swapping is handled by the PlayerController during drag-and-drop
}

// Additional utility function for debugging
void APuzzlePiece::DebugPrintInfo()
{
    FVector Location = GetActorLocation();
    FVector Scale = GetActorScale3D();


}

void APuzzlePiece::SetPieceMaterial(UMaterialInterface* NewMaterial)
{
    if (PieceMesh && NewMaterial)
    {
        PieceMesh->SetMaterial(0, NewMaterial);
    }
}
