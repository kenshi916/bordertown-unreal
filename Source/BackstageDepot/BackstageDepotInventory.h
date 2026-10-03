#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BackstageDepotInventory.generated.h"

class APawn;

// Keep this class name and schema compatible with the existing Depot save.
UCLASS(BlueprintType)
class BACKSTAGEDEPOT_API UBackstageDepotSaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadOnly) int32 Version = 1;
    UPROPERTY(BlueprintReadOnly) TArray<FName> Stash;
    UPROPERTY(BlueprintReadOnly) TArray<FName> FieldKit;
    UPROPERTY(BlueprintReadOnly) TMap<FName,int32> Quantities;
    UPROPERTY(BlueprintReadOnly) FName Primary = TEXT("rifle");
    UPROPERTY(BlueprintReadOnly) FName BuildPreset = TEXT("Balanced");
    UPROPERTY(BlueprintReadOnly) bool bReducedMotion = false;
    UPROPERTY(BlueprintReadOnly) float Health = 82.f;
    UPROPERTY(BlueprintReadOnly) FName SelectedMap = TEXT("BorderTown");
    UPROPERTY(BlueprintReadOnly) FName SelectedOperator = NAME_None;
    UPROPERTY(BlueprintReadOnly) TMap<FName,FName> WeaponFinishes;
    UPROPERTY(BlueprintReadOnly) bool bFpsPackStarterKitClaimed = false;
};

struct FDepotItem
{
    FName Id;
    FString Name;
    FString Category;
    FString Description;
    int32 Width = 1;
    int32 Height = 1;
    int32 Quantity = 1;
    float Weight = 0.f;
    bool bFirearm = false;
    bool bMedical = false;
    bool bPackWeapon = false;
    FString SourceAssetPath;
    FString IconPath;
};

USTRUCT(BlueprintType)
struct BACKSTAGEDEPOT_API FDepotOperator
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName Id;
    UPROPERTY(BlueprintReadOnly) FString DisplayName;
    UPROPERTY(BlueprintReadOnly) FString Description;
    UPROPERTY(BlueprintReadOnly) FString MeshPath;
};

USTRUCT(BlueprintType)
struct BACKSTAGEDEPOT_API FDepotWeaponFinish
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName Id;
    UPROPERTY(BlueprintReadOnly) FString DisplayName;
    UPROPERTY(BlueprintReadOnly) FString Description;
    UPROPERTY(BlueprintReadOnly) FLinearColor Accent = FLinearColor::White;
    UPROPERTY(BlueprintReadOnly) FName WeaponId;
    UPROPERTY(BlueprintReadOnly) FString SkinDataAssetPath;
    UPROPERTY(BlueprintReadOnly) FString IconPath;
};

USTRUCT(BlueprintType)
struct BACKSTAGEDEPOT_API FDepotLoadout
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName Primary;
    UPROPERTY(BlueprintReadOnly) TArray<FName> FieldKit;
    // Only quantities carried in Primary/FieldKit; never the entire stash.
    UPROPERTY(BlueprintReadOnly) TMap<FName,int32> Quantities;
    UPROPERTY(BlueprintReadOnly) FName BuildPreset;
    UPROPERTY(BlueprintReadOnly) float Health = 100.f;
    UPROPERTY(BlueprintReadOnly) FName SelectedOperator = NAME_None;
    // Cosmetic intent for carried firearms only. The verified weapon adapter applies it.
    UPROPERTY(BlueprintReadOnly) TMap<FName,FName> WeaponFinishes;
};

USTRUCT(BlueprintType)
struct BACKSTAGEDEPOT_API FDepotDestination
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName Id;
    UPROPERTY(BlueprintReadOnly) FString DisplayName;
    UPROPERTY(BlueprintReadOnly) FString Description;
    UPROPERTY(BlueprintReadOnly) FString MapPackage;
    UPROPERTY(BlueprintReadOnly) bool bMapInstalled = false;
};

/** Implement against the recovered, verified FPS Controller API. The base rejects deployment. */
UCLASS(Abstract, Blueprintable)
class BACKSTAGEDEPOT_API UBackstageDepotWeaponAdapter : public UObject
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintNativeEvent, Category="Depot|Weapons")
    bool ValidateLoadout(const FDepotLoadout& Loadout, TSubclassOf<APawn> PawnClass, FString& Reason) const;
    virtual bool ValidateLoadout_Implementation(const FDepotLoadout& Loadout, TSubclassOf<APawn> PawnClass, FString& Reason) const;

    // Called by the verified pawn-ready hook after possession and stock weapon initialization.
    UFUNCTION(BlueprintNativeEvent, Category="Depot|Weapons")
    bool ApplyLoadout(APawn* Pawn, const FDepotLoadout& Loadout, const FGuid& DeploymentId, FString& Reason);
    virtual bool ApplyLoadout_Implementation(APawn* Pawn, const FDepotLoadout& Loadout, const FGuid& DeploymentId, FString& Reason);
};

/** One inventory owner for title, hub and gameplay worlds. Controllers only present this state. */
UCLASS(Config=Game, DefaultConfig)
class BACKSTAGEDEPOT_API UBackstageDepotInventorySubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    // Idempotent initialization; override exists for isolated save-slot automation tests.
    void InitializeInventory(const FString& SlotOverride = FString());
    const TArray<FDepotItem>& Items() const { return Definitions; }
    const FDepotItem* FindItem(FName Id) const;
    UFUNCTION(BlueprintPure, Category="Depot|Inventory") UBackstageDepotSaveGame* Inventory() const { return Saved; }
    UFUNCTION(BlueprintCallable, Category="Depot|Inventory") bool SaveInventory();
    UFUNCTION(BlueprintCallable, Category="Depot|Inventory") bool EquipItem(FName Id);
    UFUNCTION(BlueprintCallable, Category="Depot|Inventory") bool ReturnItem(FName Id);
    UFUNCTION(BlueprintCallable, Category="Depot|Inventory") bool UseItem(FName Id);
    UFUNCTION(BlueprintCallable, Category="Depot|Inventory") bool SelectPreset(FName Preset);
    UFUNCTION(BlueprintCallable, Category="Depot|Inventory") bool ToggleMotion();
    UFUNCTION(BlueprintCallable, Category="Depot|Inventory") bool ResetInventory();
    UFUNCTION(BlueprintPure, Category="Depot|Inventory") FDepotLoadout CaptureLoadout() const;
    UFUNCTION(BlueprintPure, Category="Depot|Cosmetics") TArray<FDepotOperator> GetOperators() const;
    UFUNCTION(BlueprintPure, Category="Depot|Cosmetics") TArray<FDepotWeaponFinish> GetWeaponFinishes() const;
    UFUNCTION(BlueprintPure, Category="Depot|Cosmetics") TArray<FDepotWeaponFinish> GetWeaponFinishesForWeapon(FName WeaponId) const;
    UFUNCTION(BlueprintPure, Category="Depot|Cosmetics") FName GetSelectedOperator() const;
    UFUNCTION(BlueprintCallable, Category="Depot|Cosmetics") bool SelectOperator(FName Id);
    UFUNCTION(BlueprintPure, Category="Depot|Cosmetics") FName GetWeaponFinish(FName WeaponId) const;
    UFUNCTION(BlueprintCallable, Category="Depot|Cosmetics") bool SelectWeaponFinish(FName WeaponId, FName FinishId);
    UFUNCTION(BlueprintPure, Category="Depot|Inventory") bool CanAcquirePackStarterKit() const;
    UFUNCTION(BlueprintCallable, Category="Depot|Inventory") bool AcquirePackStarterKit();
    UFUNCTION(BlueprintPure, Category="Depot|Inventory") bool HasPackStarterKit() const;
    UFUNCTION(BlueprintPure, Category="Depot|Inventory") bool IsPackWeaponAvailable(FName WeaponId) const;
    UFUNCTION(BlueprintPure, Category="Depot|Deployment") TArray<FDepotDestination> GetDestinations() const;
    UFUNCTION(BlueprintCallable, Category="Depot|Deployment") bool SelectDestination(FName Id);
    UFUNCTION(BlueprintPure, Category="Depot|Deployment") FName GetSelectedDestination() const;
    UFUNCTION(BlueprintPure, Category="Depot|Deployment") bool CanDeploy(FName Id, FString& Reason) const;
    UFUNCTION(BlueprintCallable, Category="Depot|Deployment") bool TryDeploy(FName Id);
    UFUNCTION(BlueprintCallable, Category="Depot|Deployment") bool ApplyPendingLoadout(APawn* Pawn, FString& Reason);
    UFUNCTION(BlueprintCallable, Category="Depot|Deployment") void CancelPendingDeployment();
    UFUNCTION(BlueprintCallable, Category="Depot|Deployment") void SetWeaponAdapterClass(TSubclassOf<UBackstageDepotWeaponAdapter> AdapterClass);
    UFUNCTION(BlueprintPure, Category="Depot|Deployment") bool HasPendingDeployment() const { return bTravelPending; }
    UFUNCTION(BlueprintPure, Category="Depot|Inventory") FString GetStatus() const { return Status; }

    // Set only to an adapter verified against the owned pack. No default substitute.
    UPROPERTY(Config, EditAnywhere, Category="Depot|Deployment") FSoftClassPath WeaponAdapterClass;

private:
    UPROPERTY() TObjectPtr<UBackstageDepotSaveGame> Saved;
    UPROPERTY() TObjectPtr<UBackstageDepotWeaponAdapter> WeaponAdapter;
    UPROPERTY() FDepotLoadout PendingLoadout;
    UPROPERTY() TSubclassOf<APawn> PendingPawnClass;
    TArray<FDepotItem> Definitions;
    FString SaveSlot = TEXT("BackstageDepot_Inventory_v1");
    FString Status;
    FName PendingDestination;
    FGuid DeploymentId;
    TWeakObjectPtr<APawn> AppliedPawn;
    bool bTravelPending = false;
    bool bStorageReadOnly = false;
    bool bInvalidStorageOverride = false;
    FDelegateHandle TravelFailureHandle;
    void SetDefaults();
    void SanitizeInventory();
    bool CanEdit() const;
    bool OwnsFirearm(FName Id) const;
    bool SaveMutation(const FString& SuccessMessage);
    bool ResolveDeployment(FName Id, FString& Reason, FDepotDestination& Destination, UClass*& PawnClass) const;
    void OnTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error);
};
