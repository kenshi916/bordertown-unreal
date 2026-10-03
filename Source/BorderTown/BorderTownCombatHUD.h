#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/HUD.h"
#include "BorderTownCombatHUD.generated.h"

class UBorderTownCombatVitals;

/** Resolution-independent live reconstruction of health_shield_concept_v1.png. */
UCLASS()
class BORDERTOWN_API UBorderTownCombatWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(Transient) TObjectPtr<UBorderTownCombatVitals> Vitals;
protected:
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
        const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 Layer,
        const FWidgetStyle& Style, bool bParentEnabled) const override;
    virtual TSharedRef<SWidget> RebuildWidget() override;
};

/** Owned game-mode HUD; the pack retains its crosshair, ammunition and hit feedback. */
UCLASS()
class BORDERTOWN_API ABorderTownCombatHUD : public AHUD
{
    GENERATED_BODY()
public:
    ABorderTownCombatHUD();
    virtual void Tick(float DeltaSeconds) override;
    /** Isolated editor validation only; runtime builds return false. */
    UFUNCTION(BlueprintCallable, Category="Combat|Validation")
    static bool RequestWindowedValidation(int32 Width=1600, int32 Height=900);
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(Transient) TObjectPtr<UBorderTownCombatWidget> CombatWidget;
    UPROPERTY(Transient) TObjectPtr<UClass> StockHealthWidgetClass;
    UPROPERTY(Transient) TObjectPtr<UClass> StockHitWidgetClass;
    float WidgetScanTime = 0.f;
    bool bAutoTraining=false;
    int32 AutoTrainingAttempts=0;
    float AutoTrainingDelay=2.f;
};
