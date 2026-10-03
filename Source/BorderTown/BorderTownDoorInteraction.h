#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"
#include "BorderTownDoorInteraction.generated.h"

class ABorderTownDoor;
class UTextBlock;

UCLASS()
class BORDERTOWN_API UBorderTownDoorPrompt : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetPrompt(const FString& Prompt);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    UPROPERTY() TObjectPtr<UTextBlock> Label;
};

/** One local interaction coordinator: does not replace the template pawn, controller, or input mappings. */
UCLASS(BlueprintType)
class BORDERTOWN_API ABorderTownDoorInteraction : public AActor
{
    GENERATED_BODY()
public:
    ABorderTownDoorInteraction();
    virtual void Tick(float DeltaSeconds) override;
    UFUNCTION(BlueprintCallable, Category="Interaction") bool TryInteract();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction") float Reach = 280.f;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
    UPROPERTY() TObjectPtr<UBorderTownDoorPrompt> PromptWidget;
    TArray<TWeakObjectPtr<ABorderTownDoor>> Doors;
    float Elapsed = 0.f;
    float CaptureDelay = 20.f;
    FString CaptureFilename;
    bool bCaptureRequested = false;
    bool bCaptureConfigured = false;
    bool bExitAfterCapture = false;
    float CaptureRequestTime = 0.f;
    void ConfigureCapture(class APlayerController* Controller);
    ABorderTownDoor* FindFocusedDoor(class APlayerController* Controller) const;
};
