#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "BorderTownMenu.generated.h"

class UButton;
class UCanvasPanel;
class UImage;
class UTextBlock;
class UTexture2D;

/** A saved map reference keeps the title art in the cook dependency graph. */
UCLASS(BlueprintType)
class BORDERTOWN_API ABorderTownMenuScene : public AActor
{
    GENERATED_BODY()
public:
    ABorderTownMenuScene();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Title") TObjectPtr<UTexture2D> BackgroundTexture;
};

UCLASS()
class BORDERTOWN_API UBorderTownMenuWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetBackground(UTexture2D* Texture);
    UFUNCTION(BlueprintCallable, Category="Title") void EnterTown();
    UFUNCTION(BlueprintCallable, Category="Title") void ShowSettings();
    UFUNCTION(BlueprintCallable, Category="Title") void ShowMain();
    UFUNCTION(BlueprintCallable, Category="Title") void SetLow();
    UFUNCTION(BlueprintCallable, Category="Title") void SetMedium();
    UFUNCTION(BlueprintCallable, Category="Title") void SetHigh();
    UFUNCTION(BlueprintCallable, Category="Title") void ToggleDisplayMode();
    UFUNCTION(BlueprintCallable, Category="Title") void ExitGame();
    UFUNCTION(BlueprintPure, Category="Title") bool IsSettingsOpen() const { return bSettingsOpen; }
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
    UPROPERTY() TObjectPtr<UTexture2D> BackgroundTexture;
    UPROPERTY() TObjectPtr<UImage> BackgroundImage;
    UPROPERTY() TObjectPtr<UCanvasPanel> MainPage;
    UPROPERTY() TObjectPtr<UCanvasPanel> SettingsPage;
    UPROPERTY() TObjectPtr<UTextBlock> PresetStatus;
    UPROPERTY() TObjectPtr<UTextBlock> DisplayStatus;
    UPROPERTY() TArray<TObjectPtr<UButton>> MainButtons;
    UPROPERTY() TArray<TObjectPtr<UButton>> SettingsButtons;
    bool bSettingsOpen = false;
    bool bEntering = false;
    int32 FocusIndex = 0;
    UTextBlock* Text(UCanvasPanel* Canvas, const TCHAR* Name, const FString& Content,
        FVector2D Position, FVector2D Size, int32 FontSize, FLinearColor Color, bool bBold=false);
    UButton* Button(UCanvasPanel* Canvas, const TCHAR* Name, const FString& Label,
        FVector2D Position, FVector2D Size, bool bPrimary=false);
    void FocusButton(int32 Index);
    void ActivateFocused();
    void ApplyPreset(int32 Level);
    void RefreshSettings();
};

UCLASS()
class BORDERTOWN_API ABorderTownMenuPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadOnly, Category="Title") TObjectPtr<UBorderTownMenuWidget> MenuWidget;
protected:
    virtual void BeginPlay() override;
    virtual void PlayerTick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    FString CaptureFilename;
    float CaptureElapsed = 0.f;
    float CaptureRequestTime = 0.f;
    bool bCaptureRequested = false;
    bool bCaptureExit = false;
};

UCLASS()
class BORDERTOWN_API ABorderTownMenuGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ABorderTownMenuGameMode();
};
