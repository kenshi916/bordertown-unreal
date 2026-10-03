#include "BorderTownCombatHUD.h"
#include "BorderTownCombatVitals.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Engine/World.h"
#include "Fonts/FontMeasure.h"
#include "Fonts/CompositeFont.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#if WITH_EDITOR
#include "Editor.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#endif

namespace CombatHUD
{
    const FLinearColor White = FLinearColor::FromSRGBColor(FColor(237,233,222));
    const FLinearColor Cyan = FLinearColor::FromSRGBColor(FColor(114,204,220));
    const FLinearColor Track = FLinearColor::FromSRGBColor(FColor(61,67,70,210));
    const FLinearColor Sand = FLinearColor::FromSRGBColor(FColor(166,148,117));

    struct FPainter
    {
        const FGeometry& G; FSlateWindowElementList& E; int32 Layer;
        FVector2D Origin; float Scale;
        FVector2D P(double X, double Y) const { return Origin+FVector2D(X,Y)*Scale; }
        void Box(double X,double Y,double W,double H,FLinearColor Color) const
        {
            FSlateDrawElement::MakeBox(E,Layer,G.ToPaintGeometry(FVector2D(W,H)*Scale,FSlateLayoutTransform(P(X,Y))),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,Color);
        }
        void Lines(const TArray<FVector2D>& Points,FLinearColor Color,float Width=1.4f) const
        {
            TArray<FVector2D> Transformed;for(const FVector2D& V:Points)Transformed.Add(P(V.X,V.Y));
            FSlateDrawElement::MakeLines(E,Layer+1,G.ToPaintGeometry(),Transformed,ESlateDrawEffect::None,Color,true,Width*Scale);
        }
        void Polygon(const TArray<FVector2D>& Points,FLinearColor Color) const
        {
            TArray<FSlateVertex> Vertices;TArray<SlateIndex> Indices;
            for(const FVector2D& V:Points)Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(),FVector2f(P(V.X,V.Y)),FVector2f(.5f,.5f),Color.ToFColor(true)));
            for(int32 I=1;I+1<Points.Num();++I){Indices.Add(0);Indices.Add(I);Indices.Add(I+1);}
            const auto Handle=FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush("WhiteBrush"));
            FSlateDrawElement::MakeCustomVerts(E,Layer,Handle,Vertices,Indices,nullptr,0,0);
        }
        void Bar(double X,double Y,double W,double H,float Amount,FLinearColor Color) const
        {
            Polygon({{X,Y},{X+W-5,Y},{X+W,Y+5},{X+W,Y+H},{X,Y+H}},Track);
            const double Fill=W*FMath::Clamp(Amount,0.f,1.f);
            if(Fill<=0)return;
            const double Cut=FMath::Min(5.,Fill);
            Polygon({{X,Y},{X+Fill-Cut,Y},{X+Fill,Y+Cut},{X+Fill,Y+H},{X,Y+H}},Color);
        }
        FSlateFontInfo FontFor(int32 Size,bool Label=false) const
        {
            FSlateFontInfo Font=FCoreStyle::GetDefaultFontStyle(Label?TEXT("Bold"):TEXT("Regular"),FMath::Max(10,FMath::RoundToInt(Size*Scale)));
            static const TSharedPtr<const FCompositeFont> Numerals=MakeShared<FCompositeFont>(FName("Regular"),FPaths::ProjectContentDir()/TEXT("UI/Fonts/ChakraPetch-Regular.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
            if(!Label)Font=FSlateFontInfo(Numerals,FMath::Max(10,FMath::RoundToInt(Size*Scale)));
            Font.LetterSpacing=Label?65:85;
            return Font;
        }
        void Text(const FString& Value,double X,double Y,int32 Size,FLinearColor Color,bool Right=false,bool Label=false) const
        {
            const FSlateFontInfo Font=FontFor(Size,Label);
            FVector2D Pos=P(X,Y);
            const FVector2D TextSize=FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Value,Font);
            if(Right)Pos.X-=TextSize.X;
            FSlateDrawElement::MakeText(E,Layer+2,G.ToPaintGeometry(TextSize,FSlateLayoutTransform(Pos)),Value,Font,ESlateDrawEffect::None,Color);
        }
        void Key(const FString& Value,double X,double Y) const
        {
            const FSlateRoundedBoxBrush Outline(FLinearColor::Transparent,4.f*Scale,White.CopyWithNewOpacity(.65f),1.5f*Scale);
            FSlateDrawElement::MakeBox(E,Layer+1,G.ToPaintGeometry(FVector2D(30,30)*Scale,FSlateLayoutTransform(P(X,Y))),&Outline,ESlateDrawEffect::None,FLinearColor::Transparent);
            const FSlateFontInfo Font=FontFor(17);
            const FVector2D TextSize=FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Value,Font);
            const FVector2D Pos=P(X+15,Y+15)-TextSize*.5;
            FSlateDrawElement::MakeText(E,Layer+2,G.ToPaintGeometry(TextSize,FSlateLayoutTransform(Pos)),Value,Font,ESlateDrawEffect::None,White);
        }
    };
}

TSharedRef<SWidget> UBorderTownCombatWidget::RebuildWidget()
{
    if(WidgetTree&&!WidgetTree->RootWidget)WidgetTree->RootWidget=WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("CombatCanvas"));
    return Super::RebuildWidget();
}

int32 UBorderTownCombatWidget::NativePaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Culling,FSlateWindowElementList& E,int32 Layer,const FWidgetStyle& Style,bool bEnabled) const
{
    Layer=Super::NativePaint(Args,G,Culling,E,Layer,Style,bEnabled);
    if(!IsValid(Vitals)||Vitals->GetHealth()<=0)return Layer;
    const APlayerController* PC=GetOwningPlayer();
    if(!PC||!PC->GetPawn()||PC->bShowMouseCursor)return Layer;
    using namespace CombatHUD;
    const FVector2D Size=G.GetLocalSize();
    const float Scale=FMath::Clamp(float(FMath::Min(Size.X/1920.,Size.Y/1080.)),.7f,1.5f);
    FPainter P{G,E,Layer+1,FVector2D(48*Scale,Size.Y-48*Scale-168*Scale),Scale};
    // Transparent layout: only icons, typography and the functional bar tracks.
    P.Lines({{3,8},{9,7},{14,5},{18,2},{22,5},{27,7},{33,8},{33,16},{32,24},{30,29},{27,33},{23,37},{18,41},{13,37},{9,33},{6,29},{4,24},{3,16},{3,8}},Cyan,2.4f);
    P.Text(TEXT("SHIELD"),52,0,16,White,false,true);
    P.Text(FString::Printf(TEXT("%d / %d"),FMath::CeilToInt(Vitals->Shield),FMath::RoundToInt(Vitals->MaxShield)),388,0,17,White,true);
    const float ShieldFraction=Vitals->MaxShield>0?FMath::Clamp(Vitals->Shield/Vitals->MaxShield,0.f,1.f):0.f;
    for(int32 I=0;I<4;++I)P.Bar(52+I*85.,36,81,12,FMath::Clamp(ShieldFraction*4-I,0.f,1.f),Cyan);
    P.Box(13,70,10,32,White);P.Box(2,81,11,10,White);P.Box(23,81,11,10,White);
    P.Text(TEXT("HEALTH"),52,68,16,White,false,true);
    P.Text(FString::FromInt(FMath::CeilToInt(Vitals->GetHealth())),388,68,17,White,true);
    const float HealthFraction=Vitals->GetMaxHealth()>0?Vitals->GetHealth()/Vitals->GetMaxHealth():0.f;
    const FLinearColor HealthColor=HealthFraction<.25f?FLinearColor::FromSRGBColor(FColor(237,101,92)):White;
    P.Bar(52,104,336,12,HealthFraction,HealthColor);
    // A closed syringe barrel, distinct needle, flange and plunger.
    P.Lines({{10,143},{23,130},{31,138},{18,151},{10,143}},White,2.f);
    P.Lines({{14,147},{4,157}},White,2.f);
    P.Lines({{21,128},{33,140}},White,2.f);
    P.Lines({{27,134},{34,127}},White,2.f);
    P.Lines({{30,123},{38,131}},White,2.f);
    P.Lines({{17,137},{21,141}},White,1.6f);P.Lines({{21,133},{25,137}},White,1.6f);
    P.Text(FString::FromInt(Vitals->InjectorCount),46,124,23,White);P.Key(Vitals->GetInjectorKeyLabel(),77,127);
    P.Box(128,124,1,33,Sand);
    P.Lines({{160,128},{176,128},{178,130},{178,156},{176,158},{160,158},{158,156},{158,130},{160,128}},Cyan,2.f);
    P.Lines({{164,128},{164,122},{172,122},{172,128}},Cyan,2.f);
    P.Polygon({{171,132},{163,144},{168,144},{172,140}},Cyan);
    P.Polygon({{168,140},{165,154},{174,140}},Cyan);
    P.Text(FString::FromInt(Vitals->CellCount),194,124,23,White);P.Key(Vitals->GetCellKeyLabel(),225,127);
    const FString Use=Vitals->GetUseLabel();
    if(!Use.IsEmpty()){P.Text(Use,52,-39,13,White);P.Bar(52,-16,336,5,Vitals->GetUseProgress(),Cyan);}
    // Server-confirmed damage feedback. The center stays open over the target.
    const uint8 HitKind=Vitals->GetHitKind();const float Age=Vitals->GetHitAge();
    const float Duration=HitKind==3?.36f:.22f;
    if(HitKind>0&&Age>=0.f&&Age<Duration)
    {
        const float Alpha=1.f-FMath::Clamp((Age-Duration*.55f)/(Duration*.45f),0.f,1.f);
        const float Gap=FMath::Lerp(9.f,5.f,FMath::Clamp(Age/.055f,0.f,1.f));
        const float Length=HitKind==3?7.5f:6.f;
        FLinearColor Color=HitKind==3?FLinearColor::FromSRGBColor(FColor(238,72,61)):HitKind==2?Cyan:White;Color.A=Alpha;
        FPainter Hit{G,E,Layer+6,Size*.5,Scale};
        for(float SX:{-1.f,1.f})for(float SY:{-1.f,1.f})
        {
            const TArray<FVector2D> Stroke{{SX*Gap,SY*Gap},{SX*(Gap+Length),SY*(Gap+Length)}};
            Hit.Lines(Stroke,FLinearColor(0,0,0,.45f*Alpha),3.4f);
            Hit.Lines(Stroke,Color,HitKind==3?2.2f:1.8f);
        }
        if(HitKind==2)Hit.Lines({{-4,-26},{0,-28},{4,-26},{4,-22},{2,-19},{0,-17},{-2,-19},{-4,-22},{-4,-26}},Color,1.5f);
    }
    return Layer+9;
}

ABorderTownCombatHUD::ABorderTownCombatHUD(){PrimaryActorTick.bCanEverTick=true;bAutoTraining=FParse::Param(FCommandLine::Get(),TEXT("CombatTrainingBots"));}
bool ABorderTownCombatHUD::RequestWindowedValidation(int32 Width,int32 Height)
{
#if WITH_EDITOR
    if(!GEditor||GEditor->PlayWorld)return false;
    ULevelEditorPlaySettings* Settings=GetMutableDefault<ULevelEditorPlaySettings>();
    // This process is a disposable QA profile. Do not persist these settings.
    Settings->NewWindowWidth=FMath::Clamp(Width,640,3840);
    Settings->NewWindowHeight=FMath::Clamp(Height,360,2160);
    FRequestPlaySessionParams Params;
    Params.WorldType=EPlaySessionWorldType::PlayInEditor;
    Params.EditorPlaySettings=Settings;
    Params.bAllowOnlineSubsystem=false;
    GEditor->RequestPlaySession(Params);
    return true;
#else
    return false;
#endif
}
void ABorderTownCombatHUD::BeginPlay()
{
    Super::BeginPlay();
    if(PlayerOwner&&PlayerOwner->IsLocalController())
    {
        CombatWidget=CreateWidget<UBorderTownCombatWidget>(PlayerOwner);
        if(CombatWidget){CombatWidget->ForceVolatile(true);CombatWidget->SetVisibility(ESlateVisibility::HitTestInvisible);CombatWidget->AddToViewport(8);}
        StockHealthWidgetClass=LoadClass<UUserWidget>(nullptr,TEXT("/Game/FPS_Controller/UI/HealthBar/WB_Healthbar.WB_Healthbar_C"));
        StockHitWidgetClass=LoadClass<UUserWidget>(nullptr,TEXT("/Game/FPS_Controller/UI/Gameplay/WB_KillMarker.WB_KillMarker_C"));
    }
}
void ABorderTownCombatHUD::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(!CombatWidget||!PlayerOwner)return;
    APawn* Pawn=PlayerOwner->GetPawn();
    CombatWidget->Vitals=Pawn?Pawn->FindComponentByClass<UBorderTownCombatVitals>():nullptr;
    if(bAutoTraining&&AutoTrainingAttempts<3&&CombatWidget->Vitals&&!PlayerOwner->bShowMouseCursor)
    {
        AutoTrainingDelay-=DeltaSeconds;
        if(AutoTrainingDelay<=0.f)
        {
            ++AutoTrainingAttempts;AutoTrainingDelay=1.5f;
            if(CombatWidget->Vitals->SpawnTrainingTargets().Num()==2)bAutoTraining=false;
        }
    }
    WidgetScanTime-=DeltaSeconds;
    if(WidgetScanTime<=0&&StockHealthWidgetClass&&CombatWidget->Vitals)
    {
        WidgetScanTime=.25f;TArray<UUserWidget*> Widgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this,Widgets,StockHealthWidgetClass,false);
        for(UUserWidget* Widget:Widgets)if(Widget&&Widget->GetOwningPlayer()==PlayerOwner)Widget->SetRenderOpacity(0.f);
        Widgets.Reset();
        if(StockHitWidgetClass)UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this,Widgets,StockHitWidgetClass,false);
        for(UUserWidget* Widget:Widgets)if(Widget&&Widget->GetOwningPlayer()==PlayerOwner)Widget->SetRenderOpacity(0.f);
    }
}
void ABorderTownCombatHUD::EndPlay(const EEndPlayReason::Type Reason)
{
    if(CombatWidget)CombatWidget->RemoveFromParent();
    Super::EndPlay(Reason);
}
