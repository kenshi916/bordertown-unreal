#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DepotOperatorPreview.generated.h"

class USkeletalMeshComponent;

// Visual-only composition of the downloaded pack's authored third-person Oskar parts
UCLASS()
class BACKSTAGEDEPOT_API ADepotOperatorPreview : public AActor
{
    GENERATED_BODY()
public:
    ADepotOperatorPreview();
    static bool IsPackOperatorAvailable();
    UFUNCTION(BlueprintCallable, Category="Depot|Cosmetics") bool InitializePackOperator();
    UFUNCTION(BlueprintPure, Category="Depot|Cosmetics") bool IsPackOperatorReady() const { return bReady; }
private:
    UPROPERTY() TObjectPtr<USkeletalMeshComponent> PoseDriver;
    UPROPERTY() TObjectPtr<USkeletalMeshComponent> Body;
    UPROPERTY() TArray<TObjectPtr<USkeletalMeshComponent>> Parts;
    bool bReady = false;
};
