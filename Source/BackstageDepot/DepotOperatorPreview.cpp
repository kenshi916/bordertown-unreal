#include "DepotOperatorPreview.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/PackageName.h"

namespace DepotOskar
{
    const TCHAR* Driver = TEXT("/Game/FPS_Controller/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple");
    const TCHAR* Body = TEXT("/Game/FPS_Controller/Demo/MetaHumans/Oskar/Body/m_tal_nrw_body1.m_tal_nrw_body1");
    const TCHAR* Retarget = TEXT("/Game/FPS_Controller/Animations/AnimationsBlueprints/Retarget/ABP_Retarget_MetahumanTall.ABP_Retarget_MetahumanTall_C");
    const TCHAR* Idle = TEXT("/Game/FPS_Controller/Animations/TPP_Anims/Knife/Locomotion/A_TPP_Knife_Idle.A_TPP_Knife_Idle");
    const TCHAR* PartPaths[] = {
        TEXT("/Game/FPS_Controller/Demo/MetaHumans/Oskar/Face/Oskar_FaceMesh.Oskar_FaceMesh"),
        TEXT("/Game/FPS_Controller/Demo/MetaHumans/Common/Male/Tall/NormalWeight/Tops/Crewneckt/m_tal_nrw_top_crewneckt_nrm_Cinematic.m_tal_nrw_top_crewneckt_nrm_Cinematic"),
        TEXT("/Game/FPS_Controller/Demo/MetaHumans/Common/Male/Medium/NormalWeight/Bottoms/Yogapants/m_med_nrw_btm_yogaful_slm_Cinematic.m_med_nrw_btm_yogaful_slm_Cinematic"),
        TEXT("/Game/FPS_Controller/Demo/MetaHumans/Common/Male/Tall/NormalWeight/Shoes/RunningShoes/m_tal_nrw_shs_runningshoes_Cinematic.m_tal_nrw_shs_runningshoes_Cinematic"),
        TEXT("/Game/FPS_Controller/Gloves/SKM_Gloves_Tall.SKM_Gloves_Tall")
    };
    bool Present(const TCHAR* Path) { return FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(FString(Path))); }
}

ADepotOperatorPreview::ADepotOperatorPreview()
{
    PrimaryActorTick.bCanEverTick = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    PoseDriver = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PackPoseDriver"));
    PoseDriver->SetupAttachment(RootComponent);
    PoseDriver->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PoseDriver->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body_TPP"));
    Body->SetupAttachment(PoseDriver);
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    const TCHAR* Names[] = { TEXT("Face_TPP"), TEXT("Torso_TPP"), TEXT("Legs_TPP"), TEXT("Feet_TPP"), TEXT("Gloves_TPP") };
    for (const TCHAR* Name : Names)
    {
        USkeletalMeshComponent* Part = CreateDefaultSubobject<USkeletalMeshComponent>(Name);
        Part->SetupAttachment(Body);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Parts.Add(Part);
    }
}

bool ADepotOperatorPreview::IsPackOperatorAvailable()
{
    if (!DepotOskar::Present(DepotOskar::Driver) || !DepotOskar::Present(DepotOskar::Body) ||
        !DepotOskar::Present(DepotOskar::Retarget) || !DepotOskar::Present(DepotOskar::Idle)) return false;
    for (const TCHAR* Path : DepotOskar::PartPaths) if (!DepotOskar::Present(Path)) return false;
    return true;
}

bool ADepotOperatorPreview::InitializePackOperator()
{
    if (bReady) return true;
    if (!IsPackOperatorAvailable()) return false;
    USkeletalMesh* DriverMesh = LoadObject<USkeletalMesh>(nullptr, DepotOskar::Driver);
    USkeletalMesh* BodyMesh = LoadObject<USkeletalMesh>(nullptr, DepotOskar::Body);
    UClass* RetargetClass = LoadClass<UAnimInstance>(nullptr, DepotOskar::Retarget);
    UAnimSequence* Idle = LoadObject<UAnimSequence>(nullptr, DepotOskar::Idle);
    TArray<USkeletalMesh*> PartMeshes;
    for (const TCHAR* Path : DepotOskar::PartPaths)
    {
        USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, Path);
        if (!Mesh) return false;
        PartMeshes.Add(Mesh);
    }
    if (!DriverMesh || !BodyMesh || !RetargetClass || !Idle) return false;
    PoseDriver->SetSkeletalMesh(DriverMesh);
    PoseDriver->SetVisibility(false);
    PoseDriver->SetHiddenInGame(true);
    PoseDriver->SetCastShadow(false);
    PoseDriver->PlayAnimation(Idle, true);
    Body->SetSkeletalMesh(BodyMesh);
    Body->SetAnimInstanceClass(RetargetClass);
    Body->AddTickPrerequisiteComponent(PoseDriver);
    for (int32 Index = 0; Index < Parts.Num(); ++Index)
    {
        Parts[Index]->SetSkeletalMesh(PartMeshes[Index]);
        Parts[Index]->SetLeaderPoseComponent(Body, true);
        Parts[Index]->AddTickPrerequisiteComponent(Body);
        Parts[Index]->SetForcedLOD(2);
    }
    Body->SetForcedLOD(2);
    Tags.AddUnique(TEXT("HubOperator"));
    Tags.AddUnique(TEXT("FPSMC2_Oskar"));
    bReady = true;
    return true;
}
