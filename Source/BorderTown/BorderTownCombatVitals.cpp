#include "BorderTownCombatVitals.h"
#include "BorderTownTrainingBot.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Net/UnrealNetwork.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace BorderTownVitals
{
FString Canonical(FString Name) { Name.ReplaceInline(TEXT(" "),TEXT("")); Name.ReplaceInline(TEXT("_"),TEXT("")); return Name.ToLower(); }
UFunction* Function(UObject* Object, const TCHAR* Name)
{
    if (!Object) return nullptr;
    if (UFunction* F = Object->FindFunction(FName(Name))) return F;
    const FString Wanted = Canonical(Name);
    for (TFieldIterator<UFunction> It(Object->GetClass()); It; ++It) if (Canonical(It->GetName()) == Wanted) return *It;
    return nullptr;
}
double Number(const UObject* Object, const TCHAR* Name, double Fallback=0.)
{
    const FNumericProperty* P = Object ? FindFProperty<FNumericProperty>(Object->GetClass(),Name) : nullptr;
    if (!P) return Fallback;
    const void* Value=P->ContainerPtrToValuePtr<void>(Object);
    return P->IsFloatingPoint() ? P->GetFloatingPointPropertyValue(Value) : double(P->GetSignedIntPropertyValue(Value));
}
bool Flag(const UObject* Object, const TCHAR* Name)
{
    const FBoolProperty* P=Object ? FindFProperty<FBoolProperty>(Object->GetClass(),Name) : nullptr;
    return P && P->GetPropertyValue_InContainer(Object);
}
void SetFlag(UObject* Object, const TCHAR* Name, bool Value)
{
    if (FBoolProperty* P=Object ? FindFProperty<FBoolProperty>(Object->GetClass(),Name) : nullptr) P->SetPropertyValue_InContainer(Object,Value);
}
UObject* ObjectProperty(const UObject* Object,const TCHAR* Name)
{
    const FObjectPropertyBase* P=Object ? FindFProperty<FObjectPropertyBase>(Object->GetClass(),Name) : nullptr;
    return P ? P->GetObjectPropertyValue_InContainer(Object) : nullptr;
}
void SetNumber(FProperty* P, void* Container, double Value)
{
    if (FNumericProperty* N=CastField<FNumericProperty>(P))
    {
        void* Ptr=N->ContainerPtrToValuePtr<void>(Container);
        if (N->IsFloatingPoint()) N->SetFloatingPointPropertyValue(Ptr,Value); else N->SetIntPropertyValue(Ptr,int64(Value));
    }
}
bool NumericCall(UObject* Object,const TCHAR* Name,const TCHAR* Param,double Value)
{
    UFunction* F=Function(Object,Name); if (!F) return false;
    FStructOnScope Args(F); bool bFound=false;
    for (TFieldIterator<FProperty> It(F); It; ++It) if (It->HasAnyPropertyFlags(CPF_Parm) && Canonical(It->GetName())==Canonical(Param)) { SetNumber(*It,Args.GetStructMemory(),Value); bFound=true; }
    if (bFound) Object->ProcessEvent(F,Args.GetStructMemory());
    return bFound;
}
float SplitShield(float Damage,float& Shield)
{
    if (!FMath::IsFinite(Damage) || Damage<=0.f) return 0.f;
    Shield=FMath::Max(0.f,Shield);
    const float Absorbed=FMath::Min(Shield,Damage); Shield-=Absorbed;
    return Damage-Absorbed;
}
bool Eligible(float Value,float Maximum,int32 Count,bool bDead,bool bBusy)
{
    return !bDead && !bBusy && Count>0 && FMath::IsFinite(Value) && Maximum>0.f && Value<Maximum-KINDA_SMALL_NUMBER;
}
bool ClaimCommit(bool& bCommitted,int32& Count)
{
    if (bCommitted || Count<=0) return false;
    bCommitted=true; --Count; return true;
}
void HitFeedback(AController* Instigator)
{
    APawn* Pawn=Instigator ? Instigator->GetPawn() : nullptr;
    UFunction* F=Function(Pawn,TEXT("ShowHitMarker")); if (!F) return;
    FStructOnScope Args(F);
    for (TFieldIterator<FProperty> It(F);It;++It) if (FBoolProperty* P=CastField<FBoolProperty>(*It)) if (P->HasAnyPropertyFlags(CPF_Parm)) P->SetPropertyValue_InContainer(Args.GetStructMemory(),true);
    Pawn->ProcessEvent(F,Args.GetStructMemory());
}
void ConfirmFeedback(AController* SourceController,const AActor* Victim,uint8 Kind)
{
    APawn* Shooter=SourceController?SourceController->GetPawn():nullptr;
    // Self-inflicted/environmental damage is not a successful enemy hit.
    if (!Shooter || Shooter==Victim) return;
    if (UBorderTownCombatVitals* Receiver=Shooter->FindComponentByClass<UBorderTownCombatVitals>()) Receiver->ClientConfirmHit(Kind);
}
}
using namespace BorderTownVitals;

UBorderTownCombatVitals::UBorderTownCombatVitals()
{
    PrimaryComponentTick.bCanEverTick=true;
    PrimaryComponentTick.TickGroup=TG_PrePhysics;
    SetIsReplicatedByDefault(true);
}
void UBorderTownCombatVitals::BeginPlay() { Super::BeginPlay(); ResolvePackComponents(); BindInput(); }
void UBorderTownCombatVitals::ResolvePackComponents()
{
    TInlineComponentArray<UActorComponent*> Components(GetOwner());
    for (UActorComponent* C:Components)
    {
        if (C->GetClass()->GetName().StartsWith(TEXT("AC_Health"))) PackHealth=C;
        if (C->GetClass()->GetName().StartsWith(TEXT("AC_PawnInfo"))) PawnInfo=C;
    }
    BindHitFeedback();
}
void UBorderTownCombatVitals::BindHitFeedback()
{
    if (!IsValid(PawnInfo) || BoundHitFeedbackInfo.Get()==PawnInfo) return;
    UnbindHitFeedback();
    if (FMulticastDelegateProperty* Property=FindFProperty<FMulticastDelegateProperty>(PawnInfo->GetClass(),TEXT("OnDamageDone")))
    {
        FScriptDelegate Callback; Callback.BindUFunction(this,GET_FUNCTION_NAME_CHECKED(UBorderTownCombatVitals,OnPackHit));
        Property->AddDelegate(Callback,PawnInfo); BoundHitFeedbackInfo=PawnInfo;
    }
}
void UBorderTownCombatVitals::UnbindHitFeedback()
{
    if (UActorComponent* Info=BoundHitFeedbackInfo.Get())
    {
        if (FMulticastDelegateProperty* Property=FindFProperty<FMulticastDelegateProperty>(Info->GetClass(),TEXT("OnDamageDone")))
        {
            FScriptDelegate Callback; Callback.BindUFunction(this,GET_FUNCTION_NAME_CHECKED(UBorderTownCombatVitals,OnPackHit));
            Property->RemoveDelegate(Callback,Info);
        }
    }
    BoundHitFeedbackInfo.Reset();
}
void UBorderTownCombatVitals::OnPackHit(bool IsDamagedActorACharacter)
{
    const APawn* OwnerPawn=Cast<APawn>(GetOwner());
    if (bRoutingDamage || !IsDamagedActorACharacter || !OwnerPawn || !OwnerPawn->IsLocallyControlled() || !GetWorld()) return;
    const float Now=GetWorld()->GetTimeSeconds();
    // A delayed generic pack callback must not replace authoritative cyan/red.
    if (Now-LastNativeHitFeedbackTime<.08f) return;
    LastHitFeedbackKind=1; LastHitFeedbackTime=Now;
}
void UBorderTownCombatVitals::ClientConfirmHit_Implementation(uint8 Kind)
{
    if (Kind<1 || Kind>3 || !GetWorld()) return;
    LastHitFeedbackKind=Kind; LastHitFeedbackTime=GetWorld()->GetTimeSeconds(); LastNativeHitFeedbackTime=LastHitFeedbackTime;
}
float UBorderTownCombatVitals::GetHitAge() const
{
    return GetWorld()?FMath::Max(0.f,GetWorld()->GetTimeSeconds()-LastHitFeedbackTime):100.f;
}
void UBorderTownCombatVitals::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UBorderTownCombatVitals,Shield); DOREPLIFETIME(UBorderTownCombatVitals,MaxShield);
    DOREPLIFETIME(UBorderTownCombatVitals,InjectorCount); DOREPLIFETIME(UBorderTownCombatVitals,CellCount);
    DOREPLIFETIME(UBorderTownCombatVitals,UseKind); DOREPLIFETIME(UBorderTownCombatVitals,UseElapsed); DOREPLIFETIME(UBorderTownCombatVitals,UseDuration);
}
float UBorderTownCombatVitals::GetHealth() const { return float(Number(PackHealth,TEXT("Health"))); }
float UBorderTownCombatVitals::GetMaxHealth() const { return float(Number(PackHealth,TEXT("MaxHealth"),100.)); }
bool UBorderTownCombatVitals::IsDead() const { return !PackHealth || Flag(PackHealth,TEXT("bDead")) || GetHealth()<=0.f; }
float UBorderTownCombatVitals::GetUseProgress() const { return UseKind && UseDuration>0.f ? FMath::Clamp(UseElapsed/UseDuration,0.f,1.f) : 0.f; }
FString UBorderTownCombatVitals::GetUseLabel() const { return UseKind==1 ? TEXT("INJECTOR") : UseKind==2 ? TEXT("CHARGING SHIELD") : TEXT(""); }
AActor* UBorderTownCombatVitals::CurrentWeapon() const
{
    return Cast<AActor>(ObjectProperty(ObjectProperty(GetOwner(),TEXT("InventorySystem")),TEXT("CurrentWeapon")));
}
bool UBorderTownCombatVitals::IsActionBusy() const
{
    for (const TCHAR* Name:{TEXT("bPerformingAction"),TEXT("bReloading"),TEXT("bChangingWeapon"),TEXT("bInspectingWeapon"),TEXT("bUsingLethal"),TEXT("bShooting"),TEXT("bRunning"),TEXT("bTacticalSprint")}) if (Flag(PawnInfo,Name)) return true;
    return false;
}
void UBorderTownCombatVitals::BindInput()
{
    APawn* Pawn=Cast<APawn>(GetOwner()); APlayerController* PC=Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    if (!bBindLocalKeys || !PC || !PC->IsLocalController() || LocalInput) return;
    LocalInput=NewObject<UInputComponent>(GetOwner(),TEXT("BorderTownConsumableInput")); LocalInput->RegisterComponent(); LocalInput->Priority=20;
    LocalInput->BindKey(EKeys::H,IE_Pressed,this,&UBorderTownCombatVitals::InjectorPressed).bConsumeInput=true;
    LocalInput->BindKey(EKeys::J,IE_Pressed,this,&UBorderTownCombatVitals::CellPressed).bConsumeInput=true;
    LocalInput->BindKey(EKeys::B,IE_Pressed,this,&UBorderTownCombatVitals::TrainingPressed).bConsumeInput=true;
    PC->PushInputComponent(LocalInput); InputController=PC;
}
void UBorderTownCombatVitals::InjectorPressed() { if (InputController.IsValid() && !InputController->bShowMouseCursor) TryUseInjector(); }
void UBorderTownCombatVitals::CellPressed() { if (InputController.IsValid() && !InputController->bShowMouseCursor) TryUseCell(); }
void UBorderTownCombatVitals::TrainingPressed() { SpawnTrainingTargets(); }
int32 UBorderTownCombatVitals::GetTrainingTargetCount() const
{
    int32 Count=0; for (const auto& Target:TrainingTargets) if (Target.IsValid() && !Target->IsDead()) ++Count; return Count;
}
void UBorderTownCombatVitals::ClearTrainingTargets()
{
    // Never enumerate/destroy other world bots: this weak list is the ownership boundary.
    for (const auto& Target:TrainingTargets) if (Target.IsValid()) Target->Destroy();
    TrainingTargets.Reset();
}
TArray<ABorderTownTrainingBot*> UBorderTownCombatVitals::SpawnTrainingTargets()
{
    TArray<ABorderTownTrainingBot*> Spawned;
    ResolvePackComponents();
    APawn* Player=Cast<APawn>(GetOwner()); APlayerController* PC=Player?Cast<APlayerController>(Player->GetController()):nullptr;
    if (!Player || !Player->HasAuthority() || !Player->IsLocallyControlled() || !PC || PC->bShowMouseCursor || IsDead() || UseKind || IsActionBusy() || Flag(PawnInfo,TEXT("bAiming")))
    {
        LastTrainingResult=TEXT("Training targets unavailable during this action"); return Spawned;
    }
    ClearTrainingTargets();
    Spawned=ABorderTownTrainingBot::SpawnTrainingBots(this,Player,2);
    if (Spawned.Num()!=2)
    {
        for (ABorderTownTrainingBot* Target:Spawned) if (IsValid(Target)) Target->Destroy();
        Spawned.Reset(); LastTrainingResult=TEXT("Move to an open area for two training targets"); return Spawned;
    }
    Spawned[0]->Shield=50.f; Spawned[0]->ForceNetUpdate();
    for (ABorderTownTrainingBot* Target:Spawned) TrainingTargets.Add(Target);
    LastTrainingResult=TEXT("Two training targets ready: one shielded, one unshielded - B resets this pair"); return Spawned;
}
bool UBorderTownCombatVitals::TryUseInjector() { return StartUse(1); }
bool UBorderTownCombatVitals::TryUseCell() { return StartUse(2); }
bool UBorderTownCombatVitals::StartUse(uint8 Kind)
{
    ResolvePackComponents();
    if (Kind!=1 && Kind!=2) return false;
    const bool bAllowed=Eligible(Kind==1?GetHealth():Shield,Kind==1?GetMaxHealth():MaxShield,Kind==1?InjectorCount:CellCount,IsDead(),UseKind!=0 || IsActionBusy());
    if (!bAllowed) { LastUseResult=TEXT("unavailable"); return false; }
    if (!GetOwner()->HasAuthority()) { ServerStartUse(Kind); return true; }
    UseKind=Kind; UseElapsed=0.f; UseDuration=FMath::Max(.1f,Kind==1?InjectorDuration:CellDuration); bCommitted=false;
    UseWeapon=CurrentWeapon(); bOwnsActionLock=!Flag(PawnInfo,TEXT("bPerformingAction")); SetFlag(PawnInfo,TEXT("bPerformingAction"),true);
    LastUseResult=TEXT("started"); OnUseStarted.Broadcast(Kind,UseDuration); GetOwner()->ForceNetUpdate(); return true;
}
void UBorderTownCombatVitals::ServerStartUse_Implementation(uint8 Kind) { StartUse(Kind); }
void UBorderTownCombatVitals::ReleaseActionLock()
{
    // Never clear a lock already acquired by a reload or weapon transition.
    if (bOwnsActionLock && !Flag(PawnInfo,TEXT("bReloading")) && !Flag(PawnInfo,TEXT("bChangingWeapon")) && !Flag(PawnInfo,TEXT("bUsingLethal"))) SetFlag(PawnInfo,TEXT("bPerformingAction"),false);
    bOwnsActionLock=false;
}
void UBorderTownCombatVitals::CancelUse()
{
    if (GetOwner() && !GetOwner()->HasAuthority()) { ServerCancelUse(); return; }
    if (!UseKind) return;
    ReleaseActionLock(); UseKind=0; UseElapsed=0.f; UseDuration=0.f; LastUseResult=bCommitted?TEXT("cancelled after commit"):TEXT("cancelled"); OnUseEnded.Broadcast(bCommitted); GetOwner()->ForceNetUpdate();
}
void UBorderTownCombatVitals::ServerCancelUse_Implementation() { CancelUse(); }
bool UBorderTownCombatVitals::CommitUse()
{
    if (!GetOwner()->HasAuthority() || !UseKind || bCommitted || IsDead()) return false;
    const float CommitTime=FMath::Clamp(UseKind==1?InjectorCommitTime:CellCommitTime,0.f,UseDuration);
    if (UseElapsed+KINDA_SMALL_NUMBER<CommitTime) return false;
    const uint8 Kind=UseKind; int32& Count=Kind==1?InjectorCount:CellCount;
    if (!Eligible(Kind==1?GetHealth():Shield,Kind==1?GetMaxHealth():MaxShield,Count,false,false)) { CancelUse(); return false; }
    // Check the real pack function before debiting inventory.
    if (Kind==1 && !Function(PackHealth,TEXT("HealActor"))) { CancelUse(); LastUseResult=TEXT("health API unavailable"); return false; }
    if (!ClaimCommit(bCommitted,Count)) { CancelUse(); return false; }
    if (Kind==1)
    {
        const float Before=GetHealth();
        NumericCall(PackHealth,TEXT("HealActor"),TEXT("Health"),-FMath::Min(InjectorRestore,GetMaxHealth()-Before));
        if (GetHealth()<=Before) { ++Count; bCommitted=false; CancelUse(); LastUseResult=TEXT("health commit failed"); return false; }
        else { ++CompletedUses; LastUseResult=TEXT("completed"); }
    }
    else { Shield=FMath::Clamp(Shield+CellRestore,0.f,MaxShield); ++CompletedUses; LastUseResult=TEXT("completed"); }
    OnUseCommitted.Broadcast(Kind); GetOwner()->ForceNetUpdate(); return true;
}
void UBorderTownCombatVitals::FinishUse()
{
    if (!UseKind) return;
    if (!bCommitted && !CommitUse()) { CancelUse(); return; }
    ReleaseActionLock(); UseKind=0; UseElapsed=0.f; UseDuration=0.f; OnUseEnded.Broadcast(bCommitted); GetOwner()->ForceNetUpdate();
}
void UBorderTownCombatVitals::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime,TickType,TickFunction); if (!PackHealth || !PawnInfo) ResolvePackComponents(); BindInput();
    if (GetOwner()->HasAuthority() && IsDead() && TrainingTargets.Num()) ClearTrainingTargets();
    if (!UseKind) return;
    APlayerController* PC=InputController.Get(); bool bInputCancel=false;
    if (PC) for (const FKey& Key:{EKeys::LeftMouseButton,EKeys::RightMouseButton,EKeys::R,EKeys::I,EKeys::One,EKeys::Two,EKeys::Three,EKeys::LeftShift}) bInputCancel |= PC->WasInputKeyJustPressed(Key);
    if (bInputCancel || (PC && PC->bShowMouseCursor) || IsDead() || UseWeapon.Get()!=CurrentWeapon() || Flag(PawnInfo,TEXT("bReloading")) || Flag(PawnInfo,TEXT("bChangingWeapon")) || Flag(PawnInfo,TEXT("bUsingLethal"))) { CancelUse(); return; }
    if (GetOwner()->HasAuthority())
    {
        UseElapsed+=DeltaTime;
        const float CommitTime=FMath::Clamp(UseKind==1?InjectorCommitTime:CellCommitTime,0.f,UseDuration);
        if (!bCommitted && UseElapsed>=CommitTime) CommitUse();
        if (UseKind && UseElapsed>=UseDuration) FinishUse();
    }
}
float UBorderTownCombatVitals::AbsorbShieldDamage(float Damage)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || IsDead()) return 0.f;
    if (!FMath::IsFinite(Damage) || Damage<=0.f) return 0.f;
    CancelUse(); const float Remaining=SplitShield(Damage,Shield); LastShieldDamage=Damage-Remaining;
    LastDamageTime=GetWorld()->GetTimeSeconds(); ClientDamagePulse(LastShieldDamage); GetOwner()->ForceNetUpdate(); return Remaining;
}
void UBorderTownCombatVitals::ClientDamagePulse_Implementation(float Absorbed) { LastShieldDamage=Absorbed; LastDamageTime=GetWorld()->GetTimeSeconds(); }
void UBorderTownCombatVitals::RouteIncomingDamage(float Damage,const UDamageType* DamageType,AController* InstigatedBy,AActor* DamageCauser)
{
    if (bRoutingDamage || !GetOwner() || !GetOwner()->HasAuthority()) return;
    ResolvePackComponents(); if (IsDead() || !FMath::IsFinite(Damage) || Damage<=0.f) return;
    UFunction* F=Function(GetOwner(),TEXT("On DamageTaken"));
    if (!F) { UE_LOG(LogTemp,Error,TEXT("BorderTown vitals: pack On DamageTaken route missing on %s"),*GetNameSafe(GetOwner())); return; }
    const float HealthBefore=GetHealth();
    const float Remaining=AbsorbShieldDamage(Damage);
    const float Absorbed=Damage-Remaining;
    if (Remaining<=0.f)
    {
        if (InstigatedBy && InstigatedBy->GetPawn()!=GetOwner()) HitFeedback(InstigatedBy);
        ConfirmFeedback(InstigatedBy,GetOwner(),2); return;
    }
    FStructOnScope Args(F);
    for (TFieldIterator<FProperty> It(F);It;++It)
    {
        if (!It->HasAnyPropertyFlags(CPF_Parm)) continue;
        const FString Name=Canonical(It->GetName());
        if (Name==TEXT("damage")) SetNumber(*It,Args.GetStructMemory(),Remaining);
        if (FObjectPropertyBase* P=CastField<FObjectPropertyBase>(*It))
        {
            UObject* Value=Name==TEXT("damagetype")?const_cast<UDamageType*>(DamageType):Name==TEXT("instigatorcontroller")?static_cast<UObject*>(InstigatedBy):Name==TEXT("damagecauser")?static_cast<UObject*>(DamageCauser):nullptr;
            P->SetObjectPropertyValue_InContainer(Args.GetStructMemory(),Value);
        }
    }
    TGuardValue<bool> Guard(bRoutingDamage,true); GetOwner()->ProcessEvent(F,Args.GetStructMemory());
    // Pack ShowHitMarker may emit its generic event synchronously above. Apply
    // authoritative classification afterwards, including overflow eliminations.
    if (Absorbed>0.f || GetHealth()<HealthBefore) ConfirmFeedback(InstigatedBy,GetOwner(),IsDead()?3:Absorbed>0.f?2:1);
}
void UBorderTownCombatVitals::EndPlay(const EEndPlayReason::Type Reason)
{
    ClearTrainingTargets();
    UnbindHitFeedback();
    ReleaseActionLock(); if (InputController.IsValid() && LocalInput) InputController->PopInputComponent(LocalInput);
    if (LocalInput) LocalInput->DestroyComponent(); Super::EndPlay(Reason);
}
FString UBorderTownCombatVitals::RunLogicSelfTest()
{
    TArray<FString> Failures; auto Check=[&](bool Value,const TCHAR* Name){if(!Value)Failures.Add(Name);};
    float S=25.f; Check(SplitShield(40.f,S)==15.f && S==0.f,TEXT("shield overflow"));
    S=25.f; Check(SplitShield(10.f,S)==0.f && S==15.f,TEXT("shield absorption"));
    Check(!Eligible(0.f,100.f,1,true,false),TEXT("dead cannot heal")); Check(!Eligible(100.f,100.f,1,false,false),TEXT("full cannot consume"));
    Check(!Eligible(40.f,100.f,1,false,true),TEXT("busy cannot consume"));
    bool Committed=false; int32 Count=2; Check(ClaimCommit(Committed,Count),TEXT("first commit")); Check(!ClaimCommit(Committed,Count)&&Count==1,TEXT("duplicate commit"));
    return Failures.IsEmpty()?TEXT("PASS: shield absorption/overflow, death/full/busy gates, single debit"):FString::Join(Failures,TEXT("; "));
}
