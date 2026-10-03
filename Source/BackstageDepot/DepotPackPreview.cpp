#include "DepotPackPreview.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInterface.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UnrealType.h"

namespace DepotPackPreview
{
    const TCHAR* PackRoot = TEXT("/Game/FPS_Controller/");

    UObject* ReadObject(const void* Container, FProperty* Property)
    {
        if (!Container || !Property) return nullptr;
        if (const FSoftObjectProperty* Soft = CastField<FSoftObjectProperty>(Property))
        {
            return Soft->GetPropertyValue_InContainer(Container).LoadSynchronous();
        }
        if (const FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(Property))
        {
            return Object->GetObjectPropertyValue_InContainer(Container);
        }
        return nullptr;
    }

    UObject* ReadObject(UObject* Object, FName Name)
    {
        return Object ? ReadObject(Object, Object->GetClass()->FindPropertyByName(Name)) : nullptr;
    }

    FName ReadName(UObject* Object, FName Name)
    {
        if (!Object) return NAME_None;
        FProperty* Property = Object->GetClass()->FindPropertyByName(Name);
        if (const FNameProperty* Value = CastField<FNameProperty>(Property))
            return Value->GetPropertyValue_InContainer(Object);
        if (const FStrProperty* Value = CastField<FStrProperty>(Property))
            return FName(*Value->GetPropertyValue_InContainer(Object));
        return NAME_None;
    }

    int32 ReadInteger(UObject* Object, FName Name, int32 Fallback)
    {
        if (!Object) return Fallback;
        if (const FNumericProperty* Property = CastField<FNumericProperty>(Object->GetClass()->FindPropertyByName(Name)))
        {
            if (Property->IsInteger())
                return static_cast<int32>(Property->GetSignedIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Object)));
        }
        return Fallback;
    }

    FString WeaponDataPath(FName Id)
    {
        FString Name;
        if (Id == TEXT("pack_ak")) Name = TEXT("DA_Weapon_Ak");
        else if (Id == TEXT("pack_m14")) Name = TEXT("DA_Weapon_M14");
        else if (Id == TEXT("pack_mac10")) Name = TEXT("DA_Weapon_Mac-10");
        else if (Id == TEXT("pack_x24")) Name = TEXT("DA_Weapon_X24");
        else if (Id == TEXT("pack_shotgun")) Name = TEXT("DA_Weapon_Shotgun");
        if (Name.IsEmpty()) return FString();
        return FString(PackRoot) + TEXT("Blueprints/DataAssets/WeaponsData/") + Name + TEXT(".") + Name;
    }

    bool OwnsSkin(UObject* Weapon, UObject* Skin)
    {
        if (!Weapon || !Skin) return false;
        const FArrayProperty* Property = CastField<FArrayProperty>(Weapon->GetClass()->FindPropertyByName(TEXT("WeaponSkins")));
        if (!Property) return false;
        FScriptArrayHelper Array(Property, Property->ContainerPtrToValuePtr<void>(Weapon));
        for (int32 Index = 0; Index < Array.Num(); ++Index)
        {
            // Array elements have no container offset: read their storage directly.
            if (const FSoftObjectProperty* Soft = CastField<FSoftObjectProperty>(Property->Inner))
            {
                if (Soft->GetPropertyValue(Array.GetRawPtr(Index)).LoadSynchronous() == Skin) return true;
            }
            else if (const FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(Property->Inner))
            {
                if (Object->GetObjectPropertyValue(Array.GetRawPtr(Index)) == Skin) return true;
            }
        }
        return false;
    }
}

ADepotPackPreview::ADepotPackPreview()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 1.f / 30.f;
    SetReplicates(false);
    PreviewRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PreviewRoot"));
    SetRootComponent(PreviewRoot);
    WeaponPivot = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponPivot"));
    WeaponPivot->SetupAttachment(PreviewRoot);
    Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("WeaponCapture"));
    Capture->SetupAttachment(PreviewRoot);
    Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    Capture->bCaptureEveryFrame = false;
    Capture->bCaptureOnMovement = false;
    Capture->bAlwaysPersistRenderingState = true;
    Capture->FOVAngle = 35.f;
    Capture->ShowFlags.SetAtmosphere(false);
    Capture->ShowFlags.SetFog(false);
    Capture->ShowFlags.SetVolumetricFog(false);
    Capture->ShowFlags.SetMotionBlur(false);
    Capture->ShowFlags.SetEyeAdaptation(false);
    Capture->ShowFlags.SetBloom(false);
    Capture->ShowFlags.SetSkyLighting(false);
    Capture->PostProcessSettings.bOverride_AutoExposureMethod = true;
    Capture->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
    Capture->PostProcessSettings.bOverride_AutoExposureBias = true;
    Capture->PostProcessSettings.AutoExposureBias = 0.f;
    Capture->PostProcessBlendWeight = 1.f;
    Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
    Capture->PostProcessSettings.bOverride_VignetteIntensity = true;
    Capture->PostProcessSettings.VignetteIntensity = 0.f;
    Capture->PostProcessSettings.bOverride_FilmGrainIntensity = true;
    Capture->PostProcessSettings.FilmGrainIntensity = 0.f;

    for (int32 Index = 0; Index < 3; ++Index)
    {
        const FName Name(*FString::Printf(TEXT("StudioLight%d"), Index));
        UPointLightComponent* Light = CreateDefaultSubobject<UPointLightComponent>(Name);
        Light->SetupAttachment(PreviewRoot);
        Light->SetMobility(EComponentMobility::Movable);
        Light->SetIntensityUnits(ELightUnits::Candelas);
        Light->SetIntensity(Index == 0 ? 3.f : 1.5f);
        Light->SetAttenuationRadius(1300.f);
        Light->SetSourceRadius(45.f);
        Light->SetSoftSourceRadius(60.f);
        Light->SetCastShadows(false);
        Light->SetLightColor(FLinearColor::White);
        Light->SetVisibility(false);
        StudioLights.Add(Light);
    }
}

void ADepotPackPreview::BeginPlay()
{
    Super::BeginPlay();
    SetActorLocation(FVector(0.f, 0.f, -100000.f));
}

void ADepotPackPreview::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bPreviewActive && PreviewTexture && !Parts.IsEmpty())
    {
        TextureRefreshTime -= DeltaSeconds;
        if (TextureRefreshTime <= 0.f)
        {
            // The inspection stage is far from the main player camera, so its
            // textures need an explicit close-up request instead of distance LODs.
            // Timed residency expires when the panel closes or this gun changes.
            for (USkeletalMeshComponent* Part : Parts) Part->PrestreamTextures(8.f, false);
            TextureRefreshTime = 4.f;
        }
        // Keep the inspection live while textures and shaders stream in, and
        // while the user rotates or changes a finish. Hidden panels do not tick.
        Capture->CaptureScene();
    }
}

void ADepotPackPreview::ClearParts()
{
    Capture->ClearShowOnlyComponents();
    for (USkeletalMeshComponent* Part : Parts)
    {
        if (Part) Part->DestroyComponent();
    }
    Parts.Reset();
    CurrentWeapon = NAME_None;
    CurrentSkin.Reset();
    WeaponPivot->SetRelativeTransform(FTransform::Identity);
}

USkeletalMeshComponent* ADepotPackPreview::AddPart(USkeletalMesh* Mesh, USceneComponent* Parent, FName Socket)
{
    if (!Mesh || !Mesh->GetPathName().StartsWith(DepotPackPreview::PackRoot)) return nullptr;
    USkeletalMeshComponent* Part = NewObject<USkeletalMeshComponent>(this);
    AddInstanceComponent(Part);
    Part->SetupAttachment(Parent, Socket);
    Part->SetMobility(EComponentMobility::Movable);
    Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Part->SetGenerateOverlapEvents(false);
    Part->SetCanEverAffectNavigation(false);
    Part->SetSkeletalMeshAsset(Mesh);
    Part->SetForcedLOD(1);
    Part->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    Part->SetVisibleInSceneCaptureOnly(true);
    Part->SetCastShadow(false);
    Part->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Part->RegisterComponent();
    // The donor's BP_PreviewWeapon hides loose bullets/shells and its
    // BP_Base_Mag hides mag_02: these are spare pieces for reload animations.
    // Keep the fitted mag_01 visible while reproducing that preview cleanup.
    const FReferenceSkeleton& Skeleton = Mesh->GetRefSkeleton();
    for (int32 BoneIndex = 0; BoneIndex < Skeleton.GetNum(); ++BoneIndex)
    {
        const FName Bone = Skeleton.GetBoneName(BoneIndex);
        const FString Name = Bone.ToString().ToLower();
        if (Name.Contains(TEXT("bullet")) || Name.Contains(TEXT("shell")) || Name == TEXT("mag_02"))
            Part->HideBoneByName(Bone, EPhysBodyOp::PBO_None);
    }
    Part->RefreshBoneTransforms();
    Capture->ShowOnlyComponent(Part);
    Parts.Add(Part);
    return Part;
}

bool ADepotPackPreview::BuildDefaultAttachments(UObject* WeaponData, USkeletalMeshComponent* Body)
{
    using namespace DepotPackPreview;
    const FStructProperty* Attachments = CastField<FStructProperty>(WeaponData->GetClass()->FindPropertyByName(TEXT("DefaultAttachments")));
    if (!Attachments) return Fail(TEXT("Stock weapon attachment data is unavailable"));
    const void* Values = Attachments->ContainerPtrToValuePtr<void>(WeaponData);
    for (TFieldIterator<FProperty> It(Attachments->Struct); It; ++It)
    {
        UObject* Data = ReadObject(Values, *It);
        if (!Data) continue; // Empty optional attachment fields are authored defaults.
        USkeletalMesh* Mesh = Cast<USkeletalMesh>(ReadObject(Data, TEXT("Mesh")));
        if (!Mesh) return Fail(TEXT("A stock attachment mesh could not be loaded"));
        UClass* AttachmentClass = Cast<UClass>(ReadObject(Data, TEXT("AttachmentBluerpint")));
        FName Socket = ReadName(AttachmentClass ? AttachmentClass->GetDefaultObject() : nullptr, TEXT("Socket"));
        // These are the pack's actual named attachment sockets, verified on all
        // five weapon meshes. Some child Blueprints set Socket at construction
        // time, so their CDO cannot provide it for this cosmetic-only assembly.
        if (Socket.IsNone())
        {
            const FString Field = It->GetName();
            if (Field.StartsWith(TEXT("Muzzle_"))) Socket = TEXT("muzzle");
            else if (Field.StartsWith(TEXT("Stock_"))) Socket = TEXT("stock");
            else if (Field.StartsWith(TEXT("Magazine_"))) Socket = TEXT("mag");
            else if (Field.StartsWith(TEXT("Underbarrel_"))) Socket = TEXT("underbarrel");
            else if (Field.StartsWith(TEXT("Optic_"))) Socket = TEXT("optic");
        }
        if (Socket.IsNone() || !Body->DoesSocketExist(Socket))
        {
            return Fail(FString::Printf(TEXT("Stock attachment socket is unavailable: %s (%s)"), *It->GetName(), *Socket.ToString()));
        }
        if (!AddPart(Mesh, Body, Socket)) return Fail(TEXT("Stock attachment is outside the installed pack"));
    }
    return true;
}

bool ADepotPackPreview::ConfigureWeapon(FName WeaponId, const FString& SkinDataAssetPath)
{
    using namespace DepotPackPreview;
    if (CurrentWeapon == WeaponId && CurrentSkin == SkinDataAssetPath && !Parts.IsEmpty())
    {
        SetPreviewActive(true);
        return true;
    }
    ClearParts();
    const FString DataPath = WeaponDataPath(WeaponId);
    if (DataPath.IsEmpty() || !SkinDataAssetPath.StartsWith(FString(PackRoot) + TEXT("Blueprints/DataAssets/SkinsData/")))
        return Fail(TEXT("Select an installed pack weapon and its stock finish"));
    UObject* WeaponData = FSoftObjectPath(DataPath).TryLoad();
    UObject* SkinData = FSoftObjectPath(SkinDataAssetPath).TryLoad();
    if (!OwnsSkin(WeaponData, SkinData)) return Fail(TEXT("That stock finish does not belong to this weapon"));
    USkeletalMesh* Mesh = Cast<USkeletalMesh>(ReadObject(WeaponData, TEXT("WeaponMesh")));
    UMaterialInterface* Material = Cast<UMaterialInterface>(ReadObject(SkinData, TEXT("Material")));
    const int32 MaterialIndex = ReadInteger(SkinData, TEXT("MaterialIndex"), INDEX_NONE);
    if (!Mesh || !Material || !Material->GetPathName().StartsWith(PackRoot))
        return Fail(TEXT("The installed weapon mesh or finish could not be loaded"));
    USkeletalMeshComponent* Body = AddPart(Mesh, WeaponPivot, NAME_None);
    if (!Body || MaterialIndex < 0 || MaterialIndex >= Body->GetNumMaterials())
        return Fail(TEXT("The stock finish material slot is unavailable"));
    Body->SetMaterial(MaterialIndex, Material);
    if (!BuildDefaultAttachments(WeaponData, Body)) return false;
    if (!PreviewTexture)
    {
        PreviewTexture = NewObject<UTextureRenderTarget2D>(this);
        PreviewTexture->ClearColor = FLinearColor(0.018f, 0.017f, 0.015f, 1.f);
        PreviewTexture->RenderTargetFormat = RTF_RGBA8;
        PreviewTexture->InitCustomFormat(1280, 640, PF_B8G8R8A8, false);
        PreviewTexture->UpdateResourceImmediate(true);
        Capture->TextureTarget = PreviewTexture;
    }
    CurrentWeapon = WeaponId;
    CurrentSkin = SkinDataAssetPath;
    PreviewStatus = TEXT("Stock pack weapon and finish");
    FrameWeapon();
    SetPreviewActive(true);
    return true;
}

void ADepotPackPreview::FrameWeapon()
{
    WeaponPivot->SetRelativeTransform(FTransform::Identity);
    FBox Bounds(ForceInit);
    int32 VisibleVertices = 0;
    int32 FallbackParts = 0;
    for (USkeletalMeshComponent* Part : Parts)
    {
        Part->RefreshBoneTransforms();
        Part->UpdateComponentToWorld();
        FBox PartBounds(ForceInit);
        FSkeletalMeshRenderData* RenderData = Part->GetSkeletalMeshRenderData();
        FSkinWeightVertexBuffer* Weights = Part->GetSkinWeightBuffer(0);
        // Imported bounds include hidden reload props, and attachments can have
        // no physics asset. Bound the actual vertices of the visible assembly.
        if (RenderData && RenderData->LODRenderData.IsValidIndex(0) && Weights)
        {
            const FSkeletalMeshLODRenderData& LOD = RenderData->LODRenderData[0];
            const FPositionVertexBuffer& Positions = LOD.StaticVertexBuffers.PositionVertexBuffer;
            const bool bWeightsRetained = Weights->GetNeedsCPUAccess()
                && Weights->GetDataVertexBuffer()->GetWeightData()
                && (!Weights->GetVariableBonesPerVertex()
                    || (Weights->GetLookupVertexBuffer()->GetNeedsCPUAccess()
                        && Weights->GetLookupVertexBuffer()->GetNumVertices() >= Positions.GetNumVertices()));
            if (Positions.GetVertexData() && Positions.GetAllowCPUAccess() && bWeightsRetained
                && Positions.GetNumVertices() > 0 && Weights->GetNumVertices() >= Positions.GetNumVertices())
            {
                const FReferenceSkeleton& Skeleton = Part->GetSkeletalMeshAsset()->GetRefSkeleton();
                TBitArray<> VisibleBones(false, Skeleton.GetNum());
                for (int32 BoneIndex = 0; BoneIndex < Skeleton.GetNum(); ++BoneIndex)
                {
                    bool bVisible = !Part->IsBoneHidden(BoneIndex);
                    for (int32 Parent = Skeleton.GetParentIndex(BoneIndex); bVisible && Parent != INDEX_NONE;
                        Parent = Skeleton.GetParentIndex(Parent))
                        bVisible = !Part->IsBoneHidden(Parent);
                    VisibleBones[BoneIndex] = bVisible;
                }
                TArray<FMatrix44f> RefToLocal;
                TArray<FVector3f> SkinnedPositions;
                Part->GetCurrentRefToLocalMatrices(RefToLocal, 0);
                USkinnedMeshComponent::ComputeSkinnedPositions(Part, SkinnedPositions, RefToLocal, LOD, *Weights);
                for (const FSkelMeshRenderSection& Section : LOD.RenderSections)
                {
                    if (!Section.IsValid()) continue;
                    const uint32 End = FMath::Min(Section.BaseVertexIndex + Section.NumVertices, Positions.GetNumVertices());
                    for (uint32 Vertex = Section.BaseVertexIndex; Vertex < End; ++Vertex)
                    {
                        bool bVisible = false;
                        for (uint32 Influence = 0; Influence < Weights->GetMaxBoneInfluences(); ++Influence)
                        {
                            if (Weights->GetBoneWeight(Vertex, Influence) == 0) continue;
                            const uint32 BoneMapIndex = Weights->GetBoneIndex(Vertex, Influence);
                            if (!Section.BoneMap.IsValidIndex(BoneMapIndex)) continue;
                            const int32 BoneIndex = Section.BoneMap[BoneMapIndex];
                            if (VisibleBones.IsValidIndex(BoneIndex) && VisibleBones[BoneIndex])
                            {
                                bVisible = true;
                                break;
                            }
                        }
                        if (bVisible && SkinnedPositions.IsValidIndex(Vertex))
                        {
                            const FVector Position = Part->GetComponentTransform().TransformPosition(FVector(SkinnedPositions[Vertex]));
                            if (!Position.ContainsNaN())
                            {
                                PartBounds += Position;
                                ++VisibleVertices;
                            }
                        }
                    }
                }
            }
        }
        if (!PartBounds.IsValid)
        {
            // CPU data can be stripped from cooked meshes; never dereference it.
            Part->UpdateBounds();
            PartBounds = Part->Bounds.GetBox();
            ++FallbackParts;
        }
        Bounds += PartBounds;
    }
    if (!Bounds.IsValid) return;
    const FVector Center = Bounds.GetCenter() - GetActorLocation();
    AssemblyCenter = Center;
    const FVector Extent = Bounds.GetExtent();
    // The pack stores different weapons along different axes. Put their longest
    // horizontal dimension across the frame without changing attachment sockets.
    const FRotator Alignment(0.f, Extent.Y > Extent.X ? 90.f : 0.f, 0.f);
    WeaponPivot->SetRelativeRotation(Alignment);
    WeaponPivot->SetRelativeLocation(-Alignment.RotateVector(Center));
    const float Width = FMath::Max(Extent.X, Extent.Y) * 2.f;
    const float Height = Extent.Z * 2.f;
    UE_LOG(LogTemp, Display, TEXT("Depot weapon framing %s: width %.2f height %.2f center %s visible vertices %d fallback parts %d"),
        *CurrentWeapon.ToString(), Width, Height, *Center.ToString(), VisibleVertices, FallbackParts);
    // Horizontal FOV with a 2:1 target: reserve 20% breathing room on each axis.
    const float Distance = FMath::Max(Width * 0.6f, Height * 1.2f) / FMath::Tan(FMath::DegreesToRadians(17.5f));
    const FVector Eye(Width * 0.14f, -FMath::Max(Distance, 40.f), FMath::Max(Height * 0.45f, 7.f));
    Capture->SetRelativeLocation(Eye);
    Capture->SetRelativeRotation((-Eye).Rotation());
    const float LightDistance = FMath::Max(Width * 1.6f, 100.f);
    // Maintain the same studio illumination for a pistol and a long rifle.
    const float IntensityScale = FMath::Square(LightDistance / 100.f);
    for (int32 Index = 0; Index < StudioLights.Num(); ++Index)
        StudioLights[Index]->SetIntensity((Index == 0 ? 3.f : 1.5f) * IntensityScale);
    StudioLights[0]->SetRelativeLocation(FVector(-LightDistance * 0.45f, -LightDistance, LightDistance * 0.7f));
    StudioLights[1]->SetRelativeLocation(FVector(LightDistance * 0.7f, -LightDistance * 0.15f, LightDistance * 0.2f));
    StudioLights[2]->SetRelativeLocation(FVector(0.f, LightDistance * 0.8f, LightDistance * 0.65f));
}

void ADepotPackPreview::SetPreviewActive(bool bActive)
{
    bPreviewActive = bActive;
    SetActorTickEnabled(bActive);
    for (UPointLightComponent* Light : StudioLights) Light->SetVisibility(bActive);
    if (bActive) TextureRefreshTime = 0.f;
}

void ADepotPackPreview::RotatePreview(float Degrees)
{
    if (Parts.IsEmpty()) return;
    WeaponPivot->AddLocalRotation(FRotator(0.f, FMath::Clamp(Degrees, -30.f, 30.f), 0.f));
    WeaponPivot->SetRelativeLocation(-WeaponPivot->GetRelativeRotation().RotateVector(AssemblyCenter));
}

bool ADepotPackPreview::Fail(const FString& Reason)
{
    PreviewStatus = Reason;
    ClearParts();
    SetPreviewActive(false);
    UE_LOG(LogTemp, Warning, TEXT("Depot pack preview: %s"), *Reason);
    return false;
}
