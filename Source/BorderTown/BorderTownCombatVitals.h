#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BorderTownCombatVitals.generated.h"

class UInputComponent;
class APlayerController;
class UDamageType;
class ABorderTownTrainingBot;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBorderTownUseStarted,int32,Kind,float,Duration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBorderTownUseEnded,bool,bEffectCommitted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBorderTownUseCommitted,int32,Kind);

/** Owns shields and consumable transactions; AC_Health remains the health authority. */
UCLASS(ClassGroup=(BorderTown), meta=(BlueprintSpawnableComponent))
class BORDERTOWN_API UBorderTownCombatVitals : public UActorComponent
{
    GENERATED_BODY()
public:
    UBorderTownCombatVitals();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Vitals") float Shield = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Vitals") float MaxShield = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Consumables") int32 InjectorCount = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Consumables") int32 CellCount = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Consumables") float InjectorRestore = 60.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Consumables") float CellRestore = 25.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Consumables") float InjectorDuration = 2.4f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Consumables") float CellDuration = 2.8f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Consumables") float InjectorCommitTime = 1.65f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Consumables") float CellCommitTime = 2.05f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Consumables") bool bBindLocalKeys = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vitals") float LastDamageTime = -100.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vitals") float LastShieldDamage = 0.f;
    /** Local confirmed feedback: 1 health, 2 shield, 3 elimination; zero before any hit. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hit Feedback") uint8 LastHitFeedbackKind = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hit Feedback") float LastHitFeedbackTime = -100.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Consumables") int32 CompletedUses = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Consumables") FString LastUseResult;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Training") FString LastTrainingResult;
    UPROPERTY(BlueprintAssignable, Category="Consumables") FBorderTownUseStarted OnUseStarted;
    UPROPERTY(BlueprintAssignable, Category="Consumables") FBorderTownUseCommitted OnUseCommitted;
    UPROPERTY(BlueprintAssignable, Category="Consumables") FBorderTownUseEnded OnUseEnded;

    UFUNCTION(BlueprintPure, Category="Vitals") float GetHealth() const;
    UFUNCTION(BlueprintPure, Category="Vitals") float GetMaxHealth() const;
    UFUNCTION(BlueprintPure, Category="Vitals") bool IsDead() const;
    UFUNCTION(BlueprintPure, Category="Hit Feedback") uint8 GetHitKind() const { return LastHitFeedbackKind; }
    UFUNCTION(BlueprintPure, Category="Hit Feedback") float GetHitAge() const;
    /** Called by authoritative damage recipients on their shooter's component. */
    UFUNCTION(Client, Reliable) void ClientConfirmHit(uint8 Kind);
    UFUNCTION(BlueprintPure, Category="Consumables") float GetUseProgress() const;
    UFUNCTION(BlueprintPure, Category="Consumables") FString GetUseLabel() const;
    UFUNCTION(BlueprintPure, Category="Consumables") FString GetInjectorKeyLabel() const { return TEXT("H"); }
    UFUNCTION(BlueprintPure, Category="Consumables") FString GetCellKeyLabel() const { return TEXT("J"); }
    UFUNCTION(BlueprintCallable, Category="Consumables") bool TryUseInjector();
    UFUNCTION(BlueprintCallable, Category="Consumables") bool TryUseCell();
    UFUNCTION(BlueprintCallable, Category="Consumables") void CancelUse();
    UFUNCTION(BlueprintCallable, Category="Consumables") bool CommitUse();
    /** B: replaces only this component's training pair; returns none if two safe positions cannot be found. */
    UFUNCTION(BlueprintCallable, Category="Training") TArray<ABorderTownTrainingBot*> SpawnTrainingTargets();
    UFUNCTION(BlueprintPure, Category="Training") int32 GetTrainingTargetCount() const;
    UFUNCTION(BlueprintPure, Category="Training") FString GetTrainingKeyLabel() const { return TEXT("B"); }

    /** Owned pawn AnyDamage calls this once INSTEAD OF its parent's AnyDamage event. */
    UFUNCTION(BlueprintCallable, Category="Vitals") void RouteIncomingDamage(float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);
    UFUNCTION(BlueprintCallable, Category="Vitals") float AbsorbShieldDamage(float Damage);
    UFUNCTION(BlueprintCallable, Category="Vitals|Validation") static FString RunLogicSelfTest();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    UFUNCTION(Server, Reliable) void ServerStartUse(uint8 Kind);
    UFUNCTION(Server, Reliable) void ServerCancelUse();
    UFUNCTION(Client, Reliable) void ClientDamagePulse(float Absorbed);

private:
    UPROPERTY(Transient) TObjectPtr<UActorComponent> PackHealth;
    UPROPERTY(Transient) TObjectPtr<UActorComponent> PawnInfo;
    UPROPERTY(Transient) TObjectPtr<UInputComponent> LocalInput;
    TWeakObjectPtr<APlayerController> InputController;
    TWeakObjectPtr<AActor> UseWeapon;
    TWeakObjectPtr<UActorComponent> BoundHitFeedbackInfo;
    TArray<TWeakObjectPtr<ABorderTownTrainingBot>> TrainingTargets;
    UPROPERTY(Replicated) uint8 UseKind = 0;
    UPROPERTY(Replicated) float UseElapsed = 0.f;
    UPROPERTY(Replicated) float UseDuration = 0.f;
    bool bOwnsActionLock = false;
    bool bCommitted = false;
    bool bRoutingDamage = false;
    float LastNativeHitFeedbackTime = -100.f;
    UFUNCTION() void OnPackHit(bool IsDamagedActorACharacter);
    void BindHitFeedback();
    void UnbindHitFeedback();
    void ResolvePackComponents();
    void BindInput();
    void InjectorPressed();
    void CellPressed();
    void TrainingPressed();
    void ClearTrainingTargets();
    bool StartUse(uint8 Kind);
    void FinishUse();
    void ReleaseActionLock();
    bool IsActionBusy() const;
    AActor* CurrentWeapon() const;
};
