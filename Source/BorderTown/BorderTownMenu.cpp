#include "BorderTownMenu.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/FileManager.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
const FLinearColor Ivory(.87f,.83f,.76f,1.f);
const FLinearColor Title(.93f,.90f,.85f,1.f);
const FLinearColor Muted(.57f,.60f,.60f,1.f);
const FLinearColor Accent(.62f,.46f,.29f,1.f);
const FLinearColor Ink(.012f,.018f,.020f,1.f);

void Place(UCanvasPanel* Canvas, UWidget* Widget, FVector2D Position, FVector2D Size)
{
    UCanvasPanelSlot* Slot=Canvas->AddChildToCanvas(Widget);
    Slot->SetPosition(Position);
    Slot->SetSize(Size);
}
FSlateBrush Solid(FLinearColor Color)
{
    FSlateBrush Brush;
    Brush.DrawAs=ESlateBrushDrawType::Box;
    Brush.TintColor=FSlateColor(Color);
    return Brush;
}
}

ABorderTownMenuScene::ABorderTownMenuScene()
{
    PrimaryActorTick.bCanEverTick=false;
    SetActorEnableCollision(false);
}

void UBorderTownMenuWidget::SetBackground(UTexture2D* Texture)
{
    BackgroundTexture=Texture;
    if (BackgroundImage && Texture) BackgroundImage->SetBrushFromTexture(Texture,true);
}

UTextBlock* UBorderTownMenuWidget::Text(UCanvasPanel* Canvas, const TCHAR* Name,
    const FString& Content, FVector2D Position, FVector2D Size, int32 FontSize,
    FLinearColor Color, bool bBold)
{
    UTextBlock* Label=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),FName(Name));
    Label->SetText(FText::FromString(Content));
    Label->SetColorAndOpacity(FSlateColor(Color));
    FSlateFontInfo Font=Label->GetFont();
    Font.Size=FontSize;
    Font.TypefaceFontName=bBold?FName("Bold"):FName("Regular");
    Label->SetFont(Font);
    Label->SetVisibility(ESlateVisibility::HitTestInvisible);
    Place(Canvas,Label,Position,Size);
    return Label;
}

UButton* UBorderTownMenuWidget::Button(UCanvasPanel* Canvas, const TCHAR* Name,
    const FString& Label, FVector2D Position, FVector2D Size, bool bPrimary)
{
    UButton* Result=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),FName(Name));
    FButtonStyle Style;
    Style.SetNormal(Solid(bPrimary?Ivory:FLinearColor(.022f,.030f,.034f,.91f)));
    Style.SetHovered(Solid(bPrimary?FLinearColor(.98f,.94f,.86f,1):FLinearColor(.12f,.14f,.15f,.97f)));
    Style.SetPressed(Solid(bPrimary?Accent:FLinearColor(.18f,.19f,.18f,1)));
    Style.SetDisabled(Solid(FLinearColor(.035f,.043f,.048f,.9f)));
    Style.SetNormalPadding(FMargin(26,10));
    Style.SetPressedPadding(FMargin(26,11,26,9));
    Result->SetStyle(Style);
    UTextBlock* Caption=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),FName(FString(Name)+TEXT("Text")));
    Caption->SetText(FText::FromString(Label));
    Caption->SetColorAndOpacity(FSlateColor(bPrimary?Ink:Title));
    FSlateFontInfo Font=Caption->GetFont();Font.Size=bPrimary?22:19;Font.TypefaceFontName=FName("Bold");Caption->SetFont(Font);
    Caption->SetJustification(ETextJustify::Left);
    Result->AddChild(Caption);
    Place(Canvas,Result,Position,Size);
    return Result;
}

TSharedRef<SWidget> UBorderTownMenuWidget::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        UOverlay* Root=WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(),TEXT("TitleRoot"));
        WidgetTree->RootWidget=Root;
        UBorder* Base=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("DarkFallback"));
        Base->SetBrushColor(Ink);
        auto BaseSlot=Root->AddChildToOverlay(Base);BaseSlot->SetHorizontalAlignment(HAlign_Fill);BaseSlot->SetVerticalAlignment(VAlign_Fill);
        UScaleBox* ArtScale=WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(),TEXT("ArtScale"));
        ArtScale->SetStretch(EStretch::ScaleToFit);
        auto ArtSlot=Root->AddChildToOverlay(ArtScale);ArtSlot->SetHorizontalAlignment(HAlign_Fill);ArtSlot->SetVerticalAlignment(VAlign_Fill);
        BackgroundImage=WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),TEXT("BackgroundArt"));
        if (BackgroundTexture) BackgroundImage->SetBrushFromTexture(BackgroundTexture,true);
        BackgroundImage->SetVisibility(ESlateVisibility::HitTestInvisible);
        ArtScale->AddChild(BackgroundImage);

        UScaleBox* LayoutScale=WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(),TEXT("LayoutScale"));
        LayoutScale->SetStretch(EStretch::ScaleToFit);
        auto LayoutSlot=Root->AddChildToOverlay(LayoutScale);LayoutSlot->SetHorizontalAlignment(HAlign_Fill);LayoutSlot->SetVerticalAlignment(VAlign_Fill);
        USizeBox* DesignSize=WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),TEXT("Design1920x1080"));
        DesignSize->SetWidthOverride(1920);DesignSize->SetHeightOverride(1080);LayoutScale->AddChild(DesignSize);
        UOverlay* Pages=WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(),TEXT("Pages"));DesignSize->AddChild(Pages);
        MainPage=WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("MainPage"));
        auto MainSlot=Pages->AddChildToOverlay(MainPage);MainSlot->SetHorizontalAlignment(HAlign_Fill);MainSlot->SetVerticalAlignment(VAlign_Fill);
        SettingsPage=WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("SettingsPage"));
        auto SettingsSlot=Pages->AddChildToOverlay(SettingsPage);SettingsSlot->SetHorizontalAlignment(HAlign_Fill);SettingsSlot->SetVerticalAlignment(VAlign_Fill);

        Text(MainPage,TEXT("Location"),TEXT("S A N   P A L O M A"),{112,112},{600,35},19,Accent,true);
        Text(MainPage,TEXT("TitleLineOne"),TEXT("BORDER"),{104,155},{850,116},96,Title,true);
        Text(MainPage,TEXT("TitleLineTwo"),TEXT("TOWN"),{104,263},{750,116},96,Title,true);
        Text(MainPage,TEXT("Subtitle"),TEXT("Prepare your kit. Choose your destination."),{112,413},{660,38},21,Muted);
        UButton* Enter=Button(MainPage,TEXT("EnterTownButton"),TEXT("ENTER SAFEHOUSE"),{112,516},{392,88},true);
        UButton* Settings=Button(MainPage,TEXT("SettingsButton"),TEXT("SETTINGS"),{112,628},{392,64});
        UButton* Exit=Button(MainPage,TEXT("ExitButton"),TEXT("EXIT"),{112,704},{392,64});
        MainButtons={Enter,Settings,Exit};
        Enter->OnClicked.AddDynamic(this,&UBorderTownMenuWidget::EnterTown);
        Settings->OnClicked.AddDynamic(this,&UBorderTownMenuWidget::ShowSettings);
        Exit->OnClicked.AddDynamic(this,&UBorderTownMenuWidget::ExitGame);
        Text(MainPage,TEXT("Build"),TEXT("LOCAL INTEGRATION BUILD"),{112,956},{820,26},14,Muted);
        Text(MainPage,TEXT("Navigation"),TEXT("ARROWS  Navigate     ENTER  Select"),{112,991},{850,28},14,Muted);

        UBorder* SettingsBacking=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("SettingsBacking"));
        SettingsBacking->SetBrushColor(FLinearColor(.008f,.012f,.015f,.96f));
        Place(SettingsPage,SettingsBacking,{80,92},{744,872});
        Text(SettingsPage,TEXT("SettingsLocation"),TEXT("S A N   P A L O M A"),{112,126},{650,35},17,Accent,true);
        Text(SettingsPage,TEXT("SettingsTitle"),TEXT("SETTINGS"),{108,180},{700,100},62,Title,true);
        Text(SettingsPage,TEXT("GraphicsLabel"),TEXT("GRAPHICS PRESET"),{112,327},{650,32},18,Muted,true);
        UButton* Low=Button(SettingsPage,TEXT("LowButton"),TEXT("LOW"),{112,377},{194,64});
        UButton* Medium=Button(SettingsPage,TEXT("MediumButton"),TEXT("MEDIUM"),{322,377},{220,64});
        UButton* High=Button(SettingsPage,TEXT("HighButton"),TEXT("HIGH"),{558,377},{194,64});
        PresetStatus=Text(SettingsPage,TEXT("PresetStatus"),TEXT(""),{112,457},{640,32},17,Title);
        Text(SettingsPage,TEXT("DisplayLabel"),TEXT("DISPLAY MODE"),{112,542},{650,32},18,Muted,true);
        UButton* Display=Button(SettingsPage,TEXT("DisplayButton"),TEXT("SWITCH DISPLAY MODE"),{112,590},{640,64});
        DisplayStatus=Text(SettingsPage,TEXT("DisplayStatus"),TEXT(""),{112,670},{640,32},17,Title);
        UButton* Back=Button(SettingsPage,TEXT("BackButton"),TEXT("BACK"),{112,781},{392,64},true);
        Text(SettingsPage,TEXT("SettingsFootnote"),TEXT("Changes save automatically.    ESC  Back"),{112,878},{680,32},16,Muted);
        SettingsButtons={Low,Medium,High,Display,Back};
        Low->OnClicked.AddDynamic(this,&UBorderTownMenuWidget::SetLow);
        Medium->OnClicked.AddDynamic(this,&UBorderTownMenuWidget::SetMedium);
        High->OnClicked.AddDynamic(this,&UBorderTownMenuWidget::SetHigh);
        Display->OnClicked.AddDynamic(this,&UBorderTownMenuWidget::ToggleDisplayMode);
        Back->OnClicked.AddDynamic(this,&UBorderTownMenuWidget::ShowMain);
        SettingsPage->SetVisibility(ESlateVisibility::Collapsed);
    }
    return Super::RebuildWidget();
}

void UBorderTownMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();SetIsFocusable(true);FocusButton(0);
}

void UBorderTownMenuWidget::FocusButton(int32 Index)
{
    auto& Buttons=bSettingsOpen?SettingsButtons:MainButtons;
    if (Buttons.IsEmpty()) return;
    FocusIndex=(Index%Buttons.Num()+Buttons.Num())%Buttons.Num();
    for (int32 I=0;I<Buttons.Num();++I)
        Buttons[I]->SetRenderOpacity(I==FocusIndex?1.f:.83f);
    Buttons[FocusIndex]->SetKeyboardFocus();
}

void UBorderTownMenuWidget::ActivateFocused()
{
    if (bSettingsOpen)
    {
        switch(FocusIndex) {case 0:SetLow();break;case 1:SetMedium();break;case 2:SetHigh();break;case 3:ToggleDisplayMode();break;default:ShowMain();}
    }
    else { switch(FocusIndex) {case 0:EnterTown();break;case 1:ShowSettings();break;default:ExitGame();} }
}

FReply UBorderTownMenuWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
    const FKey Key=Event.GetKey();
    if (Key==EKeys::Escape && bSettingsOpen) {ShowMain();return FReply::Handled();}
    if (Key==EKeys::Down || Key==EKeys::Right || (Key==EKeys::Tab && !Event.IsShiftDown())) {FocusButton(FocusIndex+1);return FReply::Handled();}
    if (Key==EKeys::Up || Key==EKeys::Left || (Key==EKeys::Tab && Event.IsShiftDown())) {FocusButton(FocusIndex-1);return FReply::Handled();}
    if (Key==EKeys::Enter || Key==EKeys::SpaceBar) {ActivateFocused();return FReply::Handled();}
    return Super::NativeOnPreviewKeyDown(Geometry,Event);
}

void UBorderTownMenuWidget::ShowSettings()
{
    bSettingsOpen=true;MainPage->SetVisibility(ESlateVisibility::Collapsed);SettingsPage->SetVisibility(ESlateVisibility::Visible);
    RefreshSettings();FocusButton(0);
}
void UBorderTownMenuWidget::ShowMain()
{
    bSettingsOpen=false;SettingsPage->SetVisibility(ESlateVisibility::Collapsed);MainPage->SetVisibility(ESlateVisibility::Visible);FocusButton(1);
}
void UBorderTownMenuWidget::ApplyPreset(int32 Level)
{
    if (UGameUserSettings* Settings=GEngine?GEngine->GetGameUserSettings():nullptr)
    {
        Settings->SetOverallScalabilityLevel(FMath::Clamp(Level,0,2));
        Settings->ApplyNonResolutionSettings();Settings->SaveSettings();
        UE_LOG(LogTemp,Display,TEXT("BorderTown menu graphics preset: %d"),Level);
    }
    RefreshSettings();
}
void UBorderTownMenuWidget::SetLow(){ApplyPreset(0);FocusButton(0);}
void UBorderTownMenuWidget::SetMedium(){ApplyPreset(1);FocusButton(1);}
void UBorderTownMenuWidget::SetHigh(){ApplyPreset(2);FocusButton(2);}

void UBorderTownMenuWidget::ToggleDisplayMode()
{
    if (UGameUserSettings* Settings=GEngine?GEngine->GetGameUserSettings():nullptr)
    {
        const bool bWindowed=Settings->GetFullscreenMode()==EWindowMode::Windowed;
        const FIntPoint Desktop=Settings->GetDesktopResolution();
        Settings->SetFullscreenMode(bWindowed?EWindowMode::WindowedFullscreen:EWindowMode::Windowed);
        if (Desktop.X>0 && Desktop.Y>0)
            Settings->SetScreenResolution(bWindowed?Desktop:FIntPoint(FMath::Min(1600,Desktop.X*85/100),FMath::Min(900,Desktop.Y*85/100)));
        Settings->ApplyResolutionSettings(false);Settings->ConfirmVideoMode();Settings->SaveSettings();
        UE_LOG(LogTemp,Display,TEXT("BorderTown menu display mode: %s"),bWindowed?TEXT("Borderless fullscreen"):TEXT("Windowed"));
    }
    RefreshSettings();FocusButton(3);
}

void UBorderTownMenuWidget::RefreshSettings()
{
    if (UGameUserSettings* Settings=GEngine?GEngine->GetGameUserSettings():nullptr)
    {
        const int32 Level=Settings->GetOverallScalabilityLevel();
        const FString Name=Level==0?TEXT("Low"):Level==1?TEXT("Medium"):Level==2?TEXT("High"):Level==3?TEXT("Epic"):Level==4?TEXT("Cinematic"):TEXT("Custom");
        if (PresetStatus) PresetStatus->SetText(FText::FromString(TEXT("Current: ")+Name));
        if (DisplayStatus) DisplayStatus->SetText(FText::FromString(Settings->GetFullscreenMode()==EWindowMode::Windowed?TEXT("Current: Windowed"):TEXT("Current: Fullscreen")));
    }
}

void UBorderTownMenuWidget::EnterTown()
{
    if (bEntering) return;bEntering=true;
    UE_LOG(LogTemp,Display,TEXT("BorderTown menu entering safehouse"));
    if (APlayerController* PC=GetOwningPlayer())
    { PC->bShowMouseCursor=false;PC->SetInputMode(FInputModeGameOnly()); }
    UGameplayStatics::OpenLevel(this,FName(TEXT("/Game/BackstageDepot/Maps/BackstageDepot")));
}
void UBorderTownMenuWidget::ExitGame()
{
    UKismetSystemLibrary::QuitGame(this,GetOwningPlayer(),EQuitPreference::Quit,false);
}

void ABorderTownMenuPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController()) return;
    MenuWidget=CreateWidget<UBorderTownMenuWidget>(this,UBorderTownMenuWidget::StaticClass());
    for (TActorIterator<ABorderTownMenuScene> It(GetWorld());It;++It) {MenuWidget->SetBackground(It->BackgroundTexture);break;}
    MenuWidget->AddToViewport(10);
    bShowMouseCursor=true;bEnableClickEvents=true;bEnableMouseOverEvents=true;
    FInputModeUIOnly Input;Input.SetWidgetToFocus(MenuWidget->TakeWidget());Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);SetInputMode(Input);
    FParse::Value(FCommandLine::Get(),TEXT("BorderTownMenuCapture="),CaptureFilename);
    bCaptureExit=FParse::Param(FCommandLine::Get(),TEXT("BorderTownMenuCaptureExit"));
    if (FParse::Param(FCommandLine::Get(),TEXT("BorderTownMenuCaptureSettings"))) MenuWidget->ShowSettings();
    UE_LOG(LogTemp,Display,TEXT("BorderTown title menu ready"));
}
void ABorderTownMenuPlayerController::PlayerTick(float DeltaSeconds)
{
    Super::PlayerTick(DeltaSeconds);
    if (CaptureFilename.IsEmpty()) return;
    CaptureElapsed+=DeltaSeconds;
    if (!bCaptureRequested && CaptureElapsed>=3.f)
    {
        CaptureFilename=FPaths::ConvertRelativePathToFull(CaptureFilename);
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(CaptureFilename),true);
        FScreenshotRequest::RequestScreenshot(CaptureFilename,true,false);
        bCaptureRequested=true;CaptureRequestTime=CaptureElapsed;
        UE_LOG(LogTemp,Display,TEXT("BorderTown title screenshot requested: %s"),*CaptureFilename);
    }
    if (bCaptureRequested && bCaptureExit && CaptureElapsed-CaptureRequestTime>2.f && IFileManager::Get().FileSize(*CaptureFilename)>0)
        UKismetSystemLibrary::QuitGame(this,this,EQuitPreference::Quit,false);
}
void ABorderTownMenuPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
    if (MenuWidget) MenuWidget->RemoveFromParent();Super::EndPlay(Reason);
}
ABorderTownMenuGameMode::ABorderTownMenuGameMode()
{
    PlayerControllerClass=ABorderTownMenuPlayerController::StaticClass();
    DefaultPawnClass=nullptr;HUDClass=nullptr;
}
