#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BorderTownDoor.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;

/** Local walkthrough door. Origin is the hinge, local +X spans the closed leaf. */
UCLASS(BlueprintType)
class BORDERTOWN_API ABorderTownDoor : public AActor
{
    GENERATED_BODY()

public:
    ABorderTownDoor();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door") FString DoorName = TEXT("Door");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door", meta=(ClampMin="70", ClampMax="240")) float Width = 168.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door", meta=(ClampMin="190", ClampMax="350")) float Height = 260.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door", meta=(ClampMin="3", ClampMax="15")) float Thickness = 6.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door", meta=(ClampMin="-115", ClampMax="115")) float OpenAngle = 90.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door", meta=(ClampMin="20", ClampMax="160")) float DegreesPerSecond = 75.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door") bool bStartsOpen = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door") TObjectPtr<UMaterialInterface> WoodMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door") TObjectPtr<UMaterialInterface> TrimMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door") TObjectPtr<UMaterialInterface> MetalMaterial;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Door") FString ObstructionName;

    UFUNCTION(BlueprintCallable, Category="Door") void RefreshGeometry();
    UFUNCTION(BlueprintCallable, Category="Door") void ToggleDoor();
    UFUNCTION(BlueprintCallable, Category="Door") void RequestOpen(bool bOpen);
    UFUNCTION(BlueprintPure, Category="Door") bool IsOpen() const { return FMath::Abs(CurrentAngle) > 10.f; }
    UFUNCTION(BlueprintPure, Category="Door") bool IsMoving() const { return bMoving; }
    UFUNCTION(BlueprintPure, Category="Door") bool IsObstructed() const { return bObstructed; }
    UFUNCTION(BlueprintPure, Category="Door") float GetCurrentAngle() const { return CurrentAngle; }
    UFUNCTION(BlueprintPure, Category="Door") FVector GetInteractionPoint() const;

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere, Category="Door") TObjectPtr<USceneComponent> Hinge;
    UPROPERTY(VisibleAnywhere, Category="Door") TObjectPtr<USceneComponent> Swing;
    UPROPERTY(VisibleAnywhere, Category="Door") TObjectPtr<UStaticMeshComponent> Leaf;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Details;
    float CurrentAngle = 0.f;
    float TargetAngle = 0.f;
    bool bMoving = false;
    bool bObstructed = false;
    bool CanOccupyAngle(float Angle);
};
