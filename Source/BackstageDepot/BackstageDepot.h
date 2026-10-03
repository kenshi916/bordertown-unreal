#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "BackstageDepotInventory.h"
#include "BackstageDepot.generated.h"

class SBackstageDepotScreen;
class UTexture2D;
class UTexture;
class ADepotPackPreview;

UCLASS()
class BACKSTAGEDEPOT_API ABackstageDepotPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    ABackstageDepotPlayerController();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupInputComponent() override;
    const TArray<FDepotItem>& Items() const;
    const FDepotItem* FindItem(FName Id) const;
    UFUNCTION(BlueprintPure,Category="Backstage Depot|Inventory")
    UBackstageDepotSaveGame* Inventory() const;
    UFUNCTION(BlueprintPure,Category="Backstage Depot|Inventory")
    UBackstageDepotInventorySubsystem* GetInventorySubsystem() const { return InventoryState; }
    UFUNCTION(BlueprintCallable,Category="Backstage Depot|Deployment")
    void SelectDestination(FName Id);
    UFUNCTION(BlueprintCallable,Category="Backstage Depot|Deployment")
    bool DeploySelectedMap();
    UFUNCTION(BlueprintCallable,Category="Backstage Depot|Deployment")
    bool ExploreMexicoMap();
    UTexture2D* Icon(FName Id) const;
    UTexture2D* AssetIcon(const FString& Path);
    UTexture* PackWeaponPreviewTexture() const;
    UFUNCTION(BlueprintCallable,Category="Backstage Depot|Cosmetics")
    bool SetPackWeaponPreview(FName WeaponId,FName FinishId);
    void HidePackWeaponPreview();
    UFUNCTION(BlueprintCallable,Category="Backstage Depot|Cosmetics")
    void RotateCosmeticPreview(float Degrees);
    UFUNCTION(BlueprintCallable,Category="Backstage Depot|Inventory")
    bool AcquirePackStarterKit();
    UFUNCTION(BlueprintCallable,Category="Backstage Depot|Cosmetics")
    bool PreviewOperator(FName Id);
    UFUNCTION(BlueprintCallable,Category="Backstage Depot|Cosmetics")
    bool SelectOperator(FName Id);
    UFUNCTION(BlueprintPure,Category="Backstage Depot|Cosmetics")
    FName GetSelectedOperator() const;
    UFUNCTION(BlueprintCallable,Category="Backstage Depot|Cosmetics")
    bool SelectWeaponFinish(FName WeaponId,FName FinishId);
    UFUNCTION(BlueprintPure,Category="Backstage Depot|Cosmetics")
    FName GetWeaponFinish(FName WeaponId) const;
    UFUNCTION(BlueprintCallable,Category="Backstage Depot|Inventory")
    bool SaveInventory();
    UFUNCTION(BlueprintCallable,Category="Backstage Depot|Inventory")
    void EquipItem(FName Id);
    UFUNCTION(BlueprintCallable,Category="Backstage Depot|Inventory")
    void ReturnItem(FName Id);
    UFUNCTION(BlueprintCallable,Category="Backstage Depot|Inventory")
    void UseItem(FName Id);
    UFUNCTION(BlueprintCallable,Category="Backstage Depot|Inventory")
    void SelectPreset(FName Preset);
    UFUNCTION(BlueprintCallable,Category="Backstage Depot|Settings")
    void ToggleMotion();
    UFUNCTION(BlueprintCallable,Category="Backstage Depot|Inventory")
    void ResetInventory();
    UFUNCTION(BlueprintPure,Category="Backstage Depot|Inventory")
    TArray<FName> GetStashItems() const;
    UFUNCTION(BlueprintPure,Category="Backstage Depot|Inventory")
    TArray<FName> GetFieldKitItems() const;
    UFUNCTION(BlueprintPure,Category="Backstage Depot|Inventory")
    FName GetEquippedPrimary() const;
    UFUNCTION(BlueprintPure,Category="Backstage Depot|Inventory")
    int32 GetItemQuantity(FName Id) const;
    UFUNCTION(BlueprintPure,Category="Backstage Depot|Inventory")
    float GetOperatorHealth() const;
    UFUNCTION(Exec,BlueprintCallable,Category="Backstage Depot|Interface")
    void BackstagePanel(const FString& Name);
    UFUNCTION(Exec,BlueprintCallable,Category="Backstage Depot|Interface")
    void BackstageShot(const FString& FileName);
    UFUNCTION(Exec,BlueprintCallable,Category="Backstage Depot|Interface")
    void BackstageCleanShot(const FString& FileName);
    void FocusHubCamera();
    FString Feedback;
private:
    void EscapePressed();
    void LoadoutPressed();
    void RotateLeft();
    void RotateRight();
    void RefreshScreen();
    void ApplyMotionPreference();
    bool ApplyOperatorMesh(FName Id);
    UPROPERTY() TObjectPtr<UBackstageDepotInventorySubsystem> InventoryState;
    UPROPERTY() TMap<FName,TObjectPtr<UTexture2D>> Icons;
    UPROPERTY() TObjectPtr<ADepotPackPreview> PackPreviewActor;
    TArray<TWeakObjectPtr<AActor>> Fans;
    TWeakObjectPtr<AActor> OperatorActor;
    TSharedPtr<SBackstageDepotScreen> Screen;
    TSharedPtr<SWidget> ViewportWidget;
    float Runtime = 0.f;
    bool bPackPreviewActive=false;
};

UCLASS()
class BACKSTAGEDEPOT_API ABackstageDepotGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ABackstageDepotGameMode();
};
