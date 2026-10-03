#include "BorderTownTrainingBot.h"
#include "BorderTownCombatVitals.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

ABorderTownTrainingBot::ABorderTownTrainingBot()
{
    PrimaryActorTick.bCanEverTick=true;
    bReplicates=true; SetReplicateMovement(true); SetCanBeDamaged(true);
    AIControllerClass=ABorderTownTrainingController::StaticClass(); AutoPossessAI=EAutoPossessAI::PlacedInWorldOrSpawned;
    GetCapsuleComponent()->InitCapsuleSize(36.f,90.f);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    GetMesh()->SetRelativeLocation(FVector(0,0,-90)); GetMesh()->SetRelativeRotation(FRotator(0,-90,0));
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly); GetMesh()->SetCollisionObjectType(ECC_Pawn);
    GetMesh()->SetCollisionResponseToAllChannels(ECR_Ignore); GetMesh()->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    GetCharacterMovement()->MaxWalkSpeed=65.f;
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Body(TEXT("/Game/FPS_Controller/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
    if (Body.Succeeded()) GetMesh()->SetSkeletalMesh(Body.Object);
    GetMesh()->SetVisibility(false,false);

    auto Part=[this](const TCHAR* Name,const TCHAR* Asset,USkeletalMeshComponent* Parent,const TCHAR* Socket)->USkeletalMeshComponent*
    {
        USkeletalMeshComponent* C=CreateDefaultSubobject<USkeletalMeshComponent>(Name);
        C->SetupAttachment(Parent,FName(Socket));
        ConstructorHelpers::FObjectFinder<USkeletalMesh> PartAsset(Asset);
        if (PartAsset.Succeeded()) C->SetSkeletalMesh(PartAsset.Object);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        C->SetOwnerNoSee(false); C->SetOnlyOwnerSee(false);
        C->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        return C;
    };
    OskarBody=Part(TEXT("Body_TPP"),TEXT("/Game/FPS_Controller/Demo/MetaHumans/Oskar/Body/m_tal_nrw_body1.m_tal_nrw_body1"),GetMesh(),TEXT("None"));
    OskarParts.Add(Part(TEXT("Torso_TPP"),TEXT("/Game/FPS_Controller/Demo/MetaHumans/Common/Male/Tall/NormalWeight/Tops/Crewneckt/m_tal_nrw_top_crewneckt_nrm_Cinematic.m_tal_nrw_top_crewneckt_nrm_Cinematic"),OskarBody,TEXT("None")));
    OskarParts.Add(Part(TEXT("Legs_TPP"),TEXT("/Game/FPS_Controller/Demo/MetaHumans/Common/Male/Medium/NormalWeight/Bottoms/Yogapants/m_med_nrw_btm_yogaful_slm_Cinematic.m_med_nrw_btm_yogaful_slm_Cinematic"),OskarBody,TEXT("None")));
    OskarParts.Add(Part(TEXT("Face_TPP"),TEXT("/Game/FPS_Controller/Demo/MetaHumans/Oskar/Face/Oskar_FaceMesh.Oskar_FaceMesh"),OskarBody,TEXT("None")));
    OskarParts.Add(Part(TEXT("Feet_TPP"),TEXT("/Game/FPS_Controller/Demo/MetaHumans/Common/Male/Tall/NormalWeight/Shoes/RunningShoes/m_tal_nrw_shs_runningshoes_Cinematic.m_tal_nrw_shs_runningshoes_Cinematic"),OskarBody,TEXT("None")));
    OskarParts.Add(Part(TEXT("Gloves_TPP"),TEXT("/Game/FPS_Controller/Gloves/SKM_Gloves_Tall.SKM_Gloves_Tall"),OskarBody,TEXT("None")));

    // Pack's actual MeshTPP hand_r offset, plus its named attachment sockets.
    Rifle=Part(TEXT("TrainingRifle"),TEXT("/Game/FPS_Controller/Weapons/Ak/SKM_AK.SKM_AK"),OskarBody,TEXT("hand_r"));
    Rifle->SetRelativeLocation(FVector(-7.941524,2.561819,1.056154));
    Rifle->SetRelativeRotation(FRotator(4.467824,95.184821,16.366897));
    RifleMagazine=Part(TEXT("RifleMagazine"),TEXT("/Game/FPS_Controller/WeaponsAndAttachments/AK_Mag/SKM_AK_Mag.SKM_AK_Mag"),Rifle,TEXT("mag"));
    RifleStock=Part(TEXT("RifleStock"),TEXT("/Game/FPS_Controller/WeaponsAndAttachments/AK_Stock/SKM_AK_Stock.SKM_AK_Stock"),Rifle,TEXT("stock"));
    RifleMuzzle=Part(TEXT("RifleMuzzle"),TEXT("/Game/FPS_Controller/WeaponsAndAttachments/AK_Muzzle/SKM_AK_Muzzle.SKM_AK_Muzzle"),Rifle,TEXT("muzzle"));
    MuzzleLight=CreateDefaultSubobject<UPointLightComponent>(TEXT("TrainingMuzzleFlash"));
    MuzzleLight->SetupAttachment(Rifle,TEXT("muzzleFlash")); MuzzleLight->SetIntensity(1800.f); MuzzleLight->SetAttenuationRadius(180.f);
    MuzzleLight->SetLightColor(FLinearColor(1.f,.65f,.2f)); MuzzleLight->SetCastShadows(false); MuzzleLight->SetVisibility(false);
    static ConstructorHelpers::FObjectFinder<USoundBase> Sound(TEXT("/Game/FPS_Controller/Audio/Metasounds/MS_AssaultRilfe_Shot_TPP.MS_AssaultRilfe_Shot_TPP"));
    ShotSound=Sound.Object;
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Idle(TEXT("/Game/FPS_Controller/Animations/TPP_Anims/AR/Locomotion/A_TPP_AR_Idle_Pose_G1.A_TPP_AR_Idle_Pose_G1"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Walk(TEXT("/Game/FPS_Controller/Animations/TPP_Anims/Locomotion/RootLocomotion/A_TPP_SMG_Walk_Right.A_TPP_SMG_Walk_Right"));
    IdleAnimation=Idle.Object; WalkAnimation=Walk.Object;
}
void ABorderTownTrainingBot::BeginPlay()
{
    Super::BeginPlay(); Home=GetActorLocation(); StrafeAxis=GetActorRightVector(); Health=MaxHealth;
    // The native CDO is constructed before IKRig's Default-phase module loads.
    // Load this IK-retarget graph only after startup, when its node structs exist.
    if (UClass* RetargetClass=LoadClass<UAnimInstance>(nullptr,TEXT("/Game/FPS_Controller/Animations/AnimationsBlueprints/Retarget/ABP_Retarget_MetahumanTall.ABP_Retarget_MetahumanTall_C")))
    {
        OskarBody->SetAnimInstanceClass(RetargetClass);
    }
    else
    {
        UE_LOG(LogTemp,Error,TEXT("Training bot could not load the Oskar retarget animation class."));
    }
    OskarBody->AddTickPrerequisiteComponent(GetMesh());
    for (USkeletalMeshComponent* Part:OskarParts) { Part->SetLeaderPoseComponent(OskarBody); Part->AddTickPrerequisiteComponent(OskarBody); }
    Rifle->AddTickPrerequisiteComponent(OskarBody);
    RifleMagazine->HideBoneByName(TEXT("mag_02"),EPhysBodyOp::PBO_None);
    GetCharacterMovement()->DisableMovement(); UpdatePose(false);
    if (HasAuthority()) SetTrainingTarget(UGameplayStatics::GetPlayerPawn(this,0));
}
void ABorderTownTrainingBot::SetTrainingTarget(APawn* Pawn) { Target=Pawn; VisibleSince=-1.f; }
bool ABorderTownTrainingBot::SetTrainingHealth(float NewHealth)
{
    if (!HasAuthority() || bDead || !FMath::IsFinite(NewHealth)) return false;
    Health=FMath::Clamp(NewHealth,.01f,FMath::Max(.01f,MaxHealth)); ForceNetUpdate(); return true;
}
void ABorderTownTrainingBot::UpdatePose(bool bWalking)
{
    UAnimSequence* Clip=bWalking?WalkAnimation:IdleAnimation;
    if (Clip && (bPlayingWalk!=bWalking || GetMesh()->GetAnimationMode()!=EAnimationMode::AnimationSingleNode))
    {
        GetMesh()->PlayAnimation(Clip,true); bPlayingWalk=bWalking;
    }
}
void ABorderTownTrainingBot::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (MuzzleLight) MuzzleLight->SetVisibility(GetWorld()->GetTimeSeconds()<FlashUntil);
    if (!HasAuthority() || bDead) return;
    if (!Target.IsValid()) SetTrainingTarget(UGameplayStatics::GetPlayerPawn(this,0));
    APawn* Pawn=Target.Get(); if (!Pawn || !Pawn->CanBeDamaged()) { bHasLineOfSight=false; VisibleSince=-1.f; UpdatePose(false); return; }
    const FVector ToTarget=Pawn->GetActorLocation()-GetActorLocation();
    if (ToTarget.SizeSquared()>FMath::Square(AttackRange)) { bHasLineOfSight=false; VisibleSince=-1.f; UpdatePose(false); return; }
    SetActorRotation(FRotator(0,ToTarget.Rotation().Yaw,0));
    bool bMoved=false;
    if (bEnableMovement && StrafeRadius>0.f)
    {
        const float Offset=FVector::DotProduct(GetActorLocation()-Home,StrafeAxis);
        if (Offset*StrafeDirection>=StrafeRadius) StrafeDirection*=-1.f;
        const FVector Delta=StrafeAxis*(StrafeDirection*StrafeSpeed*FMath::Min(DeltaSeconds,.05f));
        // Require ground under the next step; movement is a short bounded drill, not pathfinding.
        FHitResult Ground; FCollisionQueryParams Params(SCENE_QUERY_STAT(TrainingGround),false,this); Params.AddIgnoredActor(Pawn);
        const FVector Next=GetActorLocation()+Delta;
        const bool bGround=GetWorld()->LineTraceSingleByChannel(Ground,Next,Next-FVector(0,0,140),ECC_Visibility,Params);
        if (bGround && Ground.ImpactNormal.Z>.65f)
        {
            FHitResult Block; AddActorWorldOffset(Delta,true,&Block); bMoved=!Block.bBlockingHit;
            if (Block.bBlockingHit) StrafeDirection*=-1.f;
        }
        else StrafeDirection*=-1.f;
    }
    UpdatePose(bMoved);
    const FVector Start=Rifle && Rifle->DoesSocketExist(TEXT("muzzleFlash"))?Rifle->GetSocketLocation(TEXT("muzzleFlash")):GetActorLocation()+FVector(0,0,55);
    const FVector Aim=Pawn->GetActorLocation()+FVector(0,0,25);
    FHitResult Hit; FCollisionQueryParams Params(SCENE_QUERY_STAT(TrainingShot),true,this);
    const bool bBlocked=GetWorld()->LineTraceSingleByChannel(Hit,Start,Aim,ECC_Visibility,Params);
    // Stock player Pawn/CharacterMesh profiles ignore Visibility. This ray tests
    // obstruction to a known target, so a clear path is a valid shot too.
    bHasLineOfSight=!bBlocked || Hit.GetActor()==Pawn;
    if (!bBlocked)
    {
        Hit=FHitResult(Pawn,Cast<UPrimitiveComponent>(Pawn->GetRootComponent()),Aim,(Start-Aim).GetSafeNormal());
        Hit.TraceStart=Start; Hit.TraceEnd=Aim;
    }
    const float Now=GetWorld()->GetTimeSeconds();
    if (!bHasLineOfSight) { VisibleSince=-1.f; return; }
    if (VisibleSince<0.f) VisibleSince=Now;
    if (!bEnableFiring || Now-VisibleSince<ReactionDelay || Now<NextShotTime) return;
    NextShotTime=Now+FMath::Max(.2f,ShotInterval); ++ShotsFired;
    const float Applied=UGameplayStatics::ApplyPointDamage(Pawn,ShotDamage,(Aim-Start).GetSafeNormal(),Hit,GetController(),this,UDamageType::StaticClass());
    if (Applied>0.f) ++ConfirmedHits;
    MulticastShot(Start,Hit.ImpactPoint);
}
void ABorderTownTrainingBot::MulticastShot_Implementation(FVector Start,FVector End)
{
    FlashUntil=GetWorld()->GetTimeSeconds()+.055f;
    if (ShotSound) UGameplayStatics::PlaySoundAtLocation(this,ShotSound,Start,.4f);
    if (bDrawShotTraces) DrawDebugLine(GetWorld(),Start,End,FColor(255,190,75),false,.08f,0,1.f);
}
void ABorderTownTrainingBot::NotifyShooter(AController* SourceController,uint8 HitKind)
{
    APawn* Pawn=SourceController?SourceController->GetPawn():nullptr;
    UFunction* F=Pawn?Pawn->FindFunction(TEXT("ShowHitMarker")):nullptr;
    if (F)
    {
        FStructOnScope Args(F);
        for (TFieldIterator<FProperty> It(F);It;++It) if (FBoolProperty* P=CastField<FBoolProperty>(*It)) if (P->HasAnyPropertyFlags(CPF_Parm)) P->SetPropertyValue_InContainer(Args.GetStructMemory(),true);
        Pawn->ProcessEvent(F,Args.GetStructMemory());
    }
    if (Pawn) if (UBorderTownCombatVitals* Receiver=Pawn->FindComponentByClass<UBorderTownCombatVitals>()) Receiver->ClientConfirmHit(HitKind);
}
float ABorderTownTrainingBot::TakeDamage(float Amount,const FDamageEvent& Event,AController* SourceController,AActor* Causer)
{
    if (!HasAuthority() || bDead || !CanBeDamaged() || !FMath::IsFinite(Amount) || Amount<=0.f) return 0.f;
    Shield=FMath::Max(0.f,Shield);
    const float Absorbed=FMath::Min(Shield,Amount); Shield-=Absorbed;
    const float HealthDamage=FMath::Min(Health,Amount-Absorbed);
    Health=FMath::Max(0.f,Health-HealthDamage);
    const float Actual=Absorbed+HealthDamage;
    NotifyShooter(SourceController,Health<=0.f?3:Absorbed>0.f?2:1);
    Super::TakeDamage(Actual,Event,SourceController,Causer);
    if (Health<=0.f) { bDead=true; OnRep_Dead(); DetachFromControllerPendingDestroy(); SetLifeSpan(12.f); }
    ForceNetUpdate(); return Actual;
}
void ABorderTownTrainingBot::OnRep_Dead()
{
    if (!bDead) return;
    SetCanBeDamaged(false); GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->Stop(); GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
    if (GetMesh()->GetPhysicsAsset()) GetMesh()->SetSimulatePhysics(true);
    else GetMesh()->SetRelativeRotation(FRotator(0,-90,80));
}
void ABorderTownTrainingBot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(ABorderTownTrainingBot,Health); DOREPLIFETIME(ABorderTownTrainingBot,Shield); DOREPLIFETIME(ABorderTownTrainingBot,bDead);
}
TArray<ABorderTownTrainingBot*> ABorderTownTrainingBot::SpawnTrainingBots(UObject* Context,APawn* Player,int32 Count)
{
    TArray<ABorderTownTrainingBot*> Bots;
    UWorld* World=Context?Context->GetWorld():nullptr;
    if (!World || !Player || !Player->HasAuthority()) return Bots;
    const FVector Center=Player->GetActorLocation(); const float Facing=Player->GetActorRotation().Yaw;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(TrainingSpawn),false,Player);
    // Search modest open positions around the current start, with no map modification.
    for (float Radius:{650.f,900.f,1200.f,1500.f})
    {
        for (float Angle:{0.f,35.f,-35.f,70.f,-70.f,110.f,-110.f,180.f})
        {
            if (Bots.Num()>=FMath::Clamp(Count,0,3)) return Bots;
            const FVector Point=Center+FRotator(0,Facing+Angle,0).Vector()*Radius;
            FHitResult Floor;
            if (!World->LineTraceSingleByChannel(Floor,Point+FVector(0,0,200),Point-FVector(0,0,350),ECC_Visibility,Params) || Floor.ImpactNormal.Z<.75f) continue;
            const FVector Spawn=Floor.ImpactPoint+FVector(0,0,93);
            if (FMath::Abs(Spawn.Z-Center.Z)>120.f) continue;
            if (World->OverlapBlockingTestByChannel(Spawn,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(40,90),Params)) continue;
            FHitResult Sight;
            if (World->LineTraceSingleByChannel(Sight,Center+FVector(0,0,50),Spawn+FVector(0,0,50),ECC_Visibility,Params)) continue;
            bool bTooClose=false; for (ABorderTownTrainingBot* Existing:Bots) if (FVector::DistSquared(Existing->GetActorLocation(),Spawn)<FMath::Square(300.f)) bTooClose=true;
            if (bTooClose) continue;
            FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
            ABorderTownTrainingBot* Bot=World->SpawnActor<ABorderTownTrainingBot>(Spawn,(Center-Spawn).Rotation(),SP);
            if (Bot) { Bot->SetTrainingTarget(Player); Bots.Add(Bot); Params.AddIgnoredActor(Bot); }
        }
    }
    return Bots;
}
