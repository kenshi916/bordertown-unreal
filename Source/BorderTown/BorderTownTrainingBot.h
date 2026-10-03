#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "BorderTownTrainingBot.generated.h"

class UAnimSequence;
class USkeletalMeshComponent;
class UPointLightComponent;
class USoundBase;

UCLASS()
class BORDERTOWN_API ABorderTownTrainingController : public AController
{
    GENERATED_BODY()
};

/** Bounded combat training opponent; deliberately independent of player respawn/UI graphs. */
UCLASS(Blueprintable)
class BORDERTOWN_API ABorderTownTrainingBot : public ACharacter
{
    GENERATED_BODY()
public:
    ABorderTownTrainingBot();
    virtual void Tick(float DeltaSeconds) override;
    virtual float TakeDamage(float DamageAmount,const FDamageEvent& DamageEvent,AController* EventInstigator,AActor* DamageCauser) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Training") float MaxHealth=100.f;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Replicated,Category="Training") float Health=100.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Replicated,Category="Training") float Shield=0.f;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,ReplicatedUsing=OnRep_Dead,Category="Training") bool bDead=false;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Training") float ShotDamage=8.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Training") float ShotInterval=1.2f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Training") float ReactionDelay=.8f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Training") float AttackRange=2200.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Training") float StrafeRadius=125.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Training") float StrafeSpeed=65.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Training") bool bEnableFiring=true;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Training") bool bEnableMovement=true;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Training") bool bDrawShotTraces=false;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Training") TObjectPtr<UAnimSequence> IdleAnimation;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Training") TObjectPtr<UAnimSequence> WalkAnimation;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Training") int32 ShotsFired=0;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Training") int32 ConfirmedHits=0;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Training") bool bHasLineOfSight=false;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Training") FString VisualStatus=TEXT("Oskar TPP body/clothing retargeted from pack Manny; stock AK training assembly. Hair groom and custom combat choreography not included.");
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Training|Visuals") TObjectPtr<USkeletalMeshComponent> OskarBody;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Training|Visuals") TArray<TObjectPtr<USkeletalMeshComponent>> OskarParts;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Training|Visuals") TObjectPtr<USkeletalMeshComponent> Rifle;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Training|Visuals") TObjectPtr<USkeletalMeshComponent> RifleMagazine;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Training|Visuals") TObjectPtr<USkeletalMeshComponent> RifleStock;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Training|Visuals") TObjectPtr<USkeletalMeshComponent> RifleMuzzle;
    UFUNCTION(BlueprintCallable,Category="Training") void SetTrainingTarget(APawn* Pawn);
    UFUNCTION(BlueprintPure,Category="Training") bool HasClearShot() const { return bHasLineOfSight; }
    UFUNCTION(BlueprintPure,Category="Training") bool IsDead() const { return bDead; }
    /** Test/setup hook for a living training opponent; never resurrects a dead actor. */
    UFUNCTION(BlueprintCallable,Category="Training|Validation") bool SetTrainingHealth(float NewHealth);
    /** Explicit opt-in only. Tests floor, capsule clearance, and visibility before spawning. */
    UFUNCTION(BlueprintCallable,Category="Training",meta=(WorldContext="WorldContextObject")) static TArray<ABorderTownTrainingBot*> SpawnTrainingBots(UObject* WorldContextObject,APawn* NearPlayer,int32 Count=3);

protected:
    virtual void BeginPlay() override;
    UFUNCTION() void OnRep_Dead();
    UFUNCTION(NetMulticast,Unreliable) void MulticastShot(FVector Start,FVector End);

private:
    TWeakObjectPtr<APawn> Target;
    FVector Home=FVector::ZeroVector;
    FVector StrafeAxis=FVector::RightVector;
    float StrafeDirection=1.f;
    float NextShotTime=0.f;
    float VisibleSince=-1.f;
    bool bPlayingWalk=false;
    UPROPERTY(Transient) TObjectPtr<UPointLightComponent> MuzzleLight;
    UPROPERTY() TObjectPtr<USoundBase> ShotSound;
    float FlashUntil=-1.f;
    void UpdatePose(bool bWalking);
    void NotifyShooter(AController* SourceController,uint8 HitKind);
};
