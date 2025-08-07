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

    // SCALE FIX - Default scale (1,1,1) garantisi
    SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));

    // Debug log for scale initialization
    UE_LOG(LogTemp, Warning, TEXT("PuzzlePiece spawned with normalized scale (1,1,1)"));

    // Overlap event'ini bağla
    CollisionBox->OnComponentBeginOverlap.AddDynamic(this, &APuzzlePiece::OnOverlapBegin);
}

void APuzzlePiece::BeginPlay()
{
    Super::BeginPlay();

    // SCALE VERIFICATION - BeginPlay'de scale'i kontrol et ve düzelt
    FVector CurrentScale = GetActorScale3D();
    if (!CurrentScale.Equals(FVector(1.0f, 1.0f, 1.0f), 0.01f))
    {
        UE_LOG(LogTemp, Warning, TEXT("Scale inconsistency detected for Piece %d: %s"), PieceID, *CurrentScale.ToString());
        SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
        UE_LOG(LogTemp, Warning, TEXT("Scale corrected to (1,1,1) for Piece %d"), PieceID);

        // Screen debug message
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange,
                FString::Printf(TEXT("Scale corrected for Piece %d"), PieceID));
        }
    }

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

    // SCALE MONITORING - Her frame scale'i kontrol et (performance için her 30 frame'de bir)
    static int32 FrameCounter = 0;
    FrameCounter++;
    if (FrameCounter % 30 == 0) // Her saniyede bir kontrol et (60fps varsayımıyla)
    {
        FVector CurrentScale = GetActorScale3D();
        if (!CurrentScale.Equals(FVector(1.0f, 1.0f, 1.0f), 0.01f))
        {
            UE_LOG(LogTemp, Warning, TEXT("Runtime scale drift detected for Piece %d: %s"), PieceID, *CurrentScale.ToString());
            SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));

            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red,
                    FString::Printf(TEXT("Runtime scale corrected for Piece %d"), PieceID));
            }
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

        // Debug mesajı
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
                FString::Printf(TEXT("Piece %d placed correctly! Distance: %.1f"), PieceID, Distance));
        }
    }
    else if (!bIsInCorrectPosition && bWasPreviouslyCorrect)
    {
        // Doğru pozisyondan çıkarıldı
        OnIncorrectPlacement();

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red,
                FString::Printf(TEXT("Piece %d moved from correct position"), PieceID));
        }
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
                FVector OriginalLocation = NewLocation;
                NewLocation = GameMode->ClampLocationToBoundary(NewLocation);

                UE_LOG(LogTemp, Warning, TEXT("Piece %d boundary constraint: %s -> %s"),
                    PieceID, *OriginalLocation.ToString(), *NewLocation.ToString());

                if (GEngine)
                {
                    GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Yellow,
                        FString::Printf(TEXT("Piece %d constrained to boundary"), PieceID));
                }
            }
        }
    }

    // Scale consistency check during movement
    FVector CurrentScale = GetActorScale3D();
    if (!CurrentScale.Equals(FVector(1.0f, 1.0f, 1.0f), 0.01f))
    {
        SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
        UE_LOG(LogTemp, Warning, TEXT("Scale corrected during movement for Piece %d"), PieceID);
    }

    if (bSmoothMove)
    {
        TargetLocation = NewLocation;
        bIsMoving = true;

        UE_LOG(LogTemp, Log, TEXT("Piece %d smooth move to: %s"), PieceID, *NewLocation.ToString());
    }
    else
    {
        // For instant move, cancel any ongoing smooth movement
        bIsMoving = false;
        TargetLocation = NewLocation;
        
        SetActorLocation(NewLocation);
        CheckIfInCorrectPosition();

        UE_LOG(LogTemp, Warning, TEXT("Piece %d instant move to: %s (actual: %s)"), 
            PieceID, *NewLocation.ToString(), *GetActorLocation().ToString());
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

            // Scale consistency check during selection
            FVector CurrentScale = GetActorScale3D();
            if (!CurrentScale.Equals(FVector(1.0f, 1.0f, 1.0f), 0.01f))
            {
                SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
                UE_LOG(LogTemp, Warning, TEXT("Scale corrected during selection for Piece %d"), PieceID);
            }

            // Debug için seçili parçayı vurgula
            if (PieceMesh && PieceMesh->GetMaterial(0))
            {
                // Burada material parametresi değiştirilebilir
                // CreateDynamicMaterialInstance ile glow effect eklenebilir
            }

            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Cyan,
                    FString::Printf(TEXT("Piece %d selected"), PieceID));
            }
        }
        else
        {
            OnPieceDeselected();

            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::White,
                    FString::Printf(TEXT("Piece %d deselected"), PieceID));
            }
        }
    }
}

void APuzzlePiece::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    // Overlap detection for visual feedback only
    // Actual swapping is handled by the PlayerController during drag-and-drop
    
    APuzzlePiece* OtherPiece = Cast<APuzzlePiece>(OtherActor);
    if (OtherPiece && OtherPiece != this)
    {
        // Visual feedback for overlap
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Yellow,
                FString::Printf(TEXT("Piece %d overlapping with %d"), PieceID, OtherPiece->GetPieceID()));
        }
        
        // Could add visual highlighting here if needed
    }
}

// Additional utility function for debugging
void APuzzlePiece::DebugPrintInfo()
{
    FVector Location = GetActorLocation();
    FVector Scale = GetActorScale3D();

    UE_LOG(LogTemp, Warning, TEXT("=== Piece %d Debug Info ==="), PieceID);
    UE_LOG(LogTemp, Warning, TEXT("Location: %s"), *Location.ToString());
    UE_LOG(LogTemp, Warning, TEXT("Scale: %s"), *Scale.ToString());
    UE_LOG(LogTemp, Warning, TEXT("Correct Position: %s"), *CorrectPosition.ToString());
    UE_LOG(LogTemp, Warning, TEXT("Is In Correct Position: %s"), bIsInCorrectPosition ? TEXT("True") : TEXT("False"));
    UE_LOG(LogTemp, Warning, TEXT("Is Selected: %s"), bIsSelected ? TEXT("True") : TEXT("False"));
    UE_LOG(LogTemp, Warning, TEXT("Is Moving: %s"), bIsMoving ? TEXT("True") : TEXT("False"));

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Magenta,
            FString::Printf(TEXT("Piece %d: Pos(%s) Scale(%s)"), PieceID, *Location.ToString(), *Scale.ToString()));
    }
}

void APuzzlePiece::SetPieceMaterial(UMaterialInterface* NewMaterial)
{
    if (PieceMesh && NewMaterial)
    {
        PieceMesh->SetMaterial(0, NewMaterial);
        UE_LOG(LogTemp, Warning, TEXT("Set material for Piece %d"), PieceID);
    }
}
