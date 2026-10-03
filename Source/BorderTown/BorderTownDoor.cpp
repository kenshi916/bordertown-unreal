#include "BorderTownDoor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

ABorderTownDoor::ABorderTownDoor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    SetReplicates(false);
    Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
    SetRootComponent(Hinge);
    Swing = CreateDefaultSubobject<USceneComponent>(TEXT("Swing"));
    Swing->SetupAttachment(Hinge);
    Leaf = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorLeaf"));
    Leaf->SetupAttachment(Swing);
    Leaf->SetMobility(EComponentMobility::Movable);
    Leaf->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Leaf->SetCanEverAffectNavigation(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Cube.Succeeded()) Leaf->SetStaticMesh(Cube.Object);
    for (int32 Index = 0; Index < 12; ++Index)
    {
        UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Detail%02d"), Index));
        Part->SetupAttachment(Swing);
        Part->SetMobility(EComponentMobility::Movable);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetCanEverAffectNavigation(false);
        if (Cube.Succeeded()) Part->SetStaticMesh(Cube.Object);
        Details.Add(Part);
    }
}

void ABorderTownDoor::RefreshGeometry()
{
    Width = FMath::Clamp(Width, 70.f, 240.f);
    Height = FMath::Clamp(Height, 190.f, 350.f);
    Thickness = FMath::Clamp(Thickness, 3.f, 15.f);
    Leaf->SetRelativeLocation(FVector(Width * .5f, 0, Height * .5f));
    Leaf->SetRelativeScale3D(FVector(Width, Thickness, Height) / 100.f);
    if (WoodMaterial) Leaf->SetMaterial(0, WoodMaterial);
    auto Part = [this](int32 I, FVector Position, FVector Size, UMaterialInterface* Material)
    {
        Details[I]->SetRelativeLocation(Position);
        Details[I]->SetRelativeScale3D(Size / 100.f);
        if (Material) Details[I]->SetMaterial(0, Material);
    };
    // Paired timber stiles and rails, recessed central panels, and handles on both faces.
    for (int32 Face = 0; Face < 2; ++Face)
    {
        const float Y = (Face == 0 ? -1.f : 1.f) * (Thickness * .5f + 1.f);
        const int32 N = Face * 6;
        Part(N, FVector(5, Y, Height*.5f), FVector(10, 2, Height), TrimMaterial);
        Part(N+1, FVector(Width-5, Y, Height*.5f), FVector(10, 2, Height), TrimMaterial);
        Part(N+2, FVector(Width*.5f, Y, 10), FVector(Width-16, 2, 14), TrimMaterial);
        Part(N+3, FVector(Width*.5f, Y, Height-10), FVector(Width-16, 2, 14), TrimMaterial);
        Part(N+4, FVector(Width*.5f, Y, Height*.46f), FVector(Width-16, 2, 9), TrimMaterial);
        Part(N+5, FVector(Width-18, Y*1.8f, 108), FVector(3, 5, 19), MetalMaterial);
    }
    if (!HasActorBegunPlay())
    {
        CurrentAngle = bStartsOpen ? OpenAngle : 0.f;
        TargetAngle = CurrentAngle;
        Swing->SetRelativeRotation(FRotator(0, CurrentAngle, 0));
    }
}

void ABorderTownDoor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RefreshGeometry();
}

void ABorderTownDoor::BeginPlay()
{
    Super::BeginPlay();
    RefreshGeometry();
    CurrentAngle = bStartsOpen ? OpenAngle : 0.f;
    TargetAngle = CurrentAngle;
    Swing->SetRelativeRotation(FRotator(0, CurrentAngle, 0));
    SetActorTickEnabled(false);
}

FVector ABorderTownDoor::GetInteractionPoint() const
{
    return Swing->GetComponentTransform().TransformPosition(FVector(Width*.65f, 0, FMath::Min(Height*.5f, 140.f)));
}

bool ABorderTownDoor::CanOccupyAngle(float Angle)
{
    if (!GetWorld()) return false;
    const FTransform Rotated(FRotator(0, Angle, 0));
    const FTransform Pose = Rotated * Hinge->GetComponentTransform();
    const FVector Center = Pose.TransformPosition(FVector(Width*.5f, 0, Height*.5f));
    // A little extra depth protects hands/capsules; vertical margin avoids testing the floor.
    const FVector Extent(Width*.5f-.5f, Thickness*.5f+2.f, Height*.5f-.5f);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(BorderTownDoorSafety), false, this);
    TArray<FOverlapResult> Overlaps;
    GetWorld()->OverlapMultiByChannel(Overlaps, Center, Pose.GetRotation(), ECC_Pawn,
        FCollisionShape::MakeBox(Extent), Params);
    for (const FOverlapResult& Hit : Overlaps)
    {
        if (!Hit.bBlockingHit) continue;
        ObstructionName = GetNameSafe(Hit.GetActor());
        UE_LOG(LogTemp, Display, TEXT("BorderTown door %s stopped before %s at %.2f degrees"), *DoorName, *ObstructionName, Angle);
        return false;
    }
    return true;
}

void ABorderTownDoor::ToggleDoor()
{
    if (bObstructed)
    {
        RequestOpen(!FMath::IsNearlyZero(TargetAngle));
        return;
    }
    RequestOpen(bMoving ? FMath::IsNearlyZero(TargetAngle) : !IsOpen());
}

void ABorderTownDoor::RequestOpen(bool bOpen)
{
    TargetAngle = bOpen ? OpenAngle : 0.f;
    bObstructed = false;
    ObstructionName.Reset();
    bMoving = !FMath::IsNearlyEqual(CurrentAngle, TargetAngle, .05f);
    SetActorTickEnabled(bMoving);
}

void ABorderTownDoor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    // Limit angular steps, including after a hitch: rotating component sweeps alone do not protect pawns.
    const float Desired = FMath::FInterpConstantTo(CurrentAngle, TargetAngle, FMath::Min(DeltaSeconds, .05f), DegreesPerSecond);
    const int32 Steps = FMath::Max(1, FMath::CeilToInt(FMath::Abs(Desired-CurrentAngle) / 1.5f));
    const float Step = (Desired-CurrentAngle) / Steps;
    for (int32 I = 0; I < Steps; ++I)
    {
        const float Next = CurrentAngle + Step;
        if (!CanOccupyAngle(Next))
        {
            bObstructed = true;
            bMoving = false;
            SetActorTickEnabled(false);
            return;
        }
        CurrentAngle = Next;
        Swing->SetRelativeRotation(FRotator(0, CurrentAngle, 0));
    }
    if (FMath::IsNearlyEqual(CurrentAngle, TargetAngle, .05f))
    {
        bMoving = false;
        SetActorTickEnabled(false);
    }
}
