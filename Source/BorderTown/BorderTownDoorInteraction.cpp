#include "BorderTownDoorInteraction.h"
#include "BorderTownDoor.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HighResScreenshot.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

static TAutoConsoleVariable<int32> CVarBorderTownHidePrompts(
    TEXT("BorderTown.HidePrompts"), 0, TEXT("Hide the local door interaction prompt for scene review captures."));

TSharedRef<SWidget> UBorderTownDoorPrompt::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PromptCanvas"));
        WidgetTree->RootWidget = Canvas;
        UBorder* Backing = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PromptBacking"));
        Backing->SetBrushColor(FLinearColor(.014f,.018f,.015f,.86f));
        Backing->SetPadding(FMargin(22.f, 13.f));
        UCanvasPanelSlot* PromptSlot = Canvas->AddChildToCanvas(Backing);
        PromptSlot->SetAnchors(FAnchors(.5f,.83f));
        PromptSlot->SetAlignment(FVector2D(.5f,.5f));
        PromptSlot->SetAutoSize(true);
        Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PromptLabel"));
        Label->SetColorAndOpacity(FSlateColor(FLinearColor(.95f,.92f,.80f,1.f)));
        FSlateFontInfo Font = Label->GetFont();
        Font.Size = 19;
        Label->SetFont(Font);
        Backing->SetContent(Label);
    }
    return Super::RebuildWidget();
}

void UBorderTownDoorPrompt::SetPrompt(const FString& Prompt)
{
    if (Label) Label->SetText(FText::FromString(Prompt));
    SetVisibility(Prompt.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

ABorderTownDoorInteraction::ABorderTownDoorInteraction()
{
    PrimaryActorTick.bCanEverTick = true;
    SetReplicates(false);
}

void ABorderTownDoorInteraction::BeginPlay()
{
    Super::BeginPlay();
    for (TActorIterator<ABorderTownDoor> It(GetWorld()); It; ++It) Doors.Add(*It);
    FParse::Value(FCommandLine::Get(), TEXT("BorderTownCapture="), CaptureFilename);
    FParse::Value(FCommandLine::Get(), TEXT("BorderTownCaptureDelay="), CaptureDelay);
    CaptureDelay = FMath::Clamp(CaptureDelay, 5.f, 120.f);
    bExitAfterCapture = FParse::Param(FCommandLine::Get(), TEXT("BorderTownCaptureExit"));
}

void ABorderTownDoorInteraction::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (PromptWidget) PromptWidget->RemoveFromParent();
    Super::EndPlay(EndPlayReason);
}

void ABorderTownDoorInteraction::ConfigureCapture(APlayerController* Controller)
{
    bCaptureConfigured = true;
    if (CaptureFilename.IsEmpty()) return;
    CaptureFilename = FPaths::ConvertRelativePathToFull(CaptureFilename);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(CaptureFilename), true);
    FString ViewSpec;
    if (FParse::Value(FCommandLine::Get(), TEXT("BorderTownCaptureView="), ViewSpec, false))
    {
        TArray<FString> Values;
        ViewSpec.ParseIntoArray(Values, TEXT(","), true);
        if (Values.Num() == 5)
        {
            FVector Location(FCString::Atof(*Values[0]), FCString::Atof(*Values[1]), FCString::Atof(*Values[2]));
            FRotator Rotation(FCString::Atof(*Values[3]), FCString::Atof(*Values[4]), 0.f);
            ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(Location, Rotation);
            if (Camera)
            {
                float FOV = 82.f;
                FParse::Value(FCommandLine::Get(), TEXT("BorderTownCaptureFOV="), FOV);
                Camera->GetCameraComponent()->SetFieldOfView(FMath::Clamp(FOV, 35.f, 110.f));
                Camera->GetCameraComponent()->bConstrainAspectRatio = false;
                Controller->SetViewTarget(Camera);
                UE_LOG(LogTemp, Display, TEXT("BorderTown capture camera: location=%s rotation=%s FOV=%.2f"),
                    *Camera->GetActorLocation().ToString(), *Camera->GetActorRotation().ToString(), Camera->GetCameraComponent()->FieldOfView);
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("BorderTownCaptureView expected 5 comma-separated values; received %d: %s"), Values.Num(), *ViewSpec);
        }
    }
    UE_LOG(LogTemp, Display, TEXT("BorderTown capture warming for %.1fs: %s"), CaptureDelay, *CaptureFilename);
}

void ABorderTownDoorInteraction::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0);
    if (!Controller || !Controller->IsLocalController()) return;
    if (!bCaptureConfigured) ConfigureCapture(Controller);
    Elapsed += DeltaSeconds;
    if (!CaptureFilename.IsEmpty())
    {
        if (PromptWidget) PromptWidget->SetPrompt(TEXT(""));
        if (!bCaptureRequested && Elapsed >= CaptureDelay && GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
        {
            if (IConsoleVariable* Delay = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HighResScreenshotDelay"))) Delay->Set(4);
            FHighResScreenshotConfig& Config = GetHighResScreenshotConfig();
            Config.SetHDRCapture(false);
            Config.SetMaskEnabled(false);
            Config.SetFilename(CaptureFilename);
            if (Config.SetResolution(1280, 720, 1.f))
            {
                bCaptureRequested = GEngine->GameViewport->Viewport->TakeHighResScreenShot();
                CaptureRequestTime = Elapsed;
                UE_LOG(LogTemp, Display, TEXT("BorderTown capture requested: %s"), *CaptureFilename);
            }
        }
        if (bExitAfterCapture && bCaptureRequested && Elapsed > CaptureRequestTime + 4.f && IFileManager::Get().FileSize(*CaptureFilename) > 0)
        {
            FPlatformMisc::RequestExit(false);
        }
        return;
    }
    if (!PromptWidget)
    {
        PromptWidget = CreateWidget<UBorderTownDoorPrompt>(Controller, UBorderTownDoorPrompt::StaticClass());
        if (PromptWidget) PromptWidget->AddToViewport(20);
    }
    ABorderTownDoor* Target = FindFocusedDoor(Controller);
    FString Prompt;
    if (Target)
    {
        if (Controller->WasInputKeyJustPressed(EKeys::E)) TryInteract();
        if (Target->IsObstructed()) Prompt = TEXT("Door obstructed - step aside, then press E");
        else Prompt = FString::Printf(TEXT("E  %s  |  %s"), Target->IsOpen() ? TEXT("Close") : TEXT("Open"), *Target->DoorName);
    }
    if (CVarBorderTownHidePrompts.GetValueOnGameThread() != 0) Prompt.Reset();
    if (PromptWidget) PromptWidget->SetPrompt(Prompt);
}

ABorderTownDoor* ABorderTownDoorInteraction::FindFocusedDoor(APlayerController* Controller) const
{
    if (!Controller || !Controller->IsLocalController()) return nullptr;
    FVector ViewLocation;
    FRotator ViewRotation;
    Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
    ABorderTownDoor* Target = nullptr;
    float BestScore = TNumericLimits<float>::Max();
    for (const TWeakObjectPtr<ABorderTownDoor>& WeakDoor : Doors)
    {
        ABorderTownDoor* Door = WeakDoor.Get();
        if (!Door) continue;
        const FVector ToDoor = Door->GetInteractionPoint() - ViewLocation;
        const float Distance = ToDoor.Size();
        const float Facing = FVector::DotProduct(ViewRotation.Vector(), ToDoor.GetSafeNormal());
        if (Distance > Reach || Facing < .35f) continue;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(BorderTownDoorFocus), true, Controller->GetPawn());
        FHitResult Hit;
        if (GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, Door->GetInteractionPoint(), ECC_Visibility, Params) && Hit.GetActor() != Door) continue;
        const float Score = Distance - Facing*140.f;
        if (Score < BestScore) { BestScore = Score; Target = Door; }
    }
    return Target;
}

bool ABorderTownDoorInteraction::TryInteract()
{
    APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0);
    if (ABorderTownDoor* Target = FindFocusedDoor(Controller))
    {
        Target->ToggleDoor();
        return true;
    }
    return false;
}
