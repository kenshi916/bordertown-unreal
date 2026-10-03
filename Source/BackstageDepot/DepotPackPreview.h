#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DepotPackPreview.generated.h"

class UPointLightComponent;
class USceneCaptureComponent2D;
class USceneComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class UTextureRenderTarget2D;

/** Cosmetic-only rendering of the installed FPS Controller pack. No weapon gameplay runs. */
UCLASS()
class BACKSTAGEDEPOT_API ADepotPackPreview : public AActor
{
    GENERATED_BODY()
public:
    ADepotPackPreview();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION(BlueprintCallable, Category="Backstage Depot|Pack Preview")
    bool ConfigureWeapon(FName WeaponId, const FString& SkinDataAssetPath);
    UFUNCTION(BlueprintPure, Category="Backstage Depot|Pack Preview")
    UTextureRenderTarget2D* GetPreviewTexture() const { return PreviewTexture; }
    UFUNCTION(BlueprintCallable, Category="Backstage Depot|Pack Preview")
    void SetPreviewActive(bool bActive);
    UFUNCTION(BlueprintCallable, Category="Backstage Depot|Pack Preview")
    void RotatePreview(float Degrees);
    UFUNCTION(BlueprintPure, Category="Backstage Depot|Pack Preview")
    FString GetPreviewStatus() const { return PreviewStatus; }
    UFUNCTION(BlueprintPure, Category="Backstage Depot|Pack Preview")
    int32 GetPreviewPartCount() const { return Parts.Num(); }

private:
    void ClearParts();
    USkeletalMeshComponent* AddPart(USkeletalMesh* Mesh, USceneComponent* Parent, FName Socket);
    bool BuildDefaultAttachments(UObject* WeaponData, USkeletalMeshComponent* Body);
    void FrameWeapon();
    bool Fail(const FString& Reason);

    UPROPERTY() TObjectPtr<USceneComponent> PreviewRoot;
    UPROPERTY() TObjectPtr<USceneComponent> WeaponPivot;
    UPROPERTY() TObjectPtr<USceneCaptureComponent2D> Capture;
    UPROPERTY() TObjectPtr<UTextureRenderTarget2D> PreviewTexture;
    UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> StudioLights;
    UPROPERTY() TArray<TObjectPtr<USkeletalMeshComponent>> Parts;
    FString PreviewStatus;
    FName CurrentWeapon;
    FString CurrentSkin;
    FVector AssemblyCenter = FVector::ZeroVector;
    bool bPreviewActive = false;
    float TextureRefreshTime = 0.f;
};
