#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Rendering/DrawElements.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/CompositeFont.h"
#include "Misc/Paths.h"
#include "DepotSymbols.h"

namespace DepotStyle
{
    inline FLinearColor SRGB(uint8 R,uint8 G,uint8 B,float A=1.f)
    { FLinearColor C=FLinearColor::FromSRGBColor(FColor(R,G,B));C.A=A;return C; }
    const FLinearColor Ink=SRGB(20,20,19,.94f);
    const FLinearColor Panel=SRGB(31,30,28,.96f);
    const FLinearColor ActionText=SRGB(22,21,19);
    const FLinearColor Cream=SRGB(232,225,210);
    const FLinearColor Muted=SRGB(170,164,151);
    const FLinearColor Accent=SRGB(195,166,122);
    const FLinearColor Red=SRGB(167,73,62);
    const FLinearColor Line=SRGB(168,157,137,.30f);
    inline FSlateFontInfo Font(int32 Size,bool Bold=false)
    {
        static const TSharedPtr<const FCompositeFont> Regular=MakeShared<FCompositeFont>(NAME_None,FPaths::ProjectContentDir()/TEXT("UI/Fonts/Barlow/Barlow-Regular.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
        static const TSharedPtr<const FCompositeFont> Medium=MakeShared<FCompositeFont>(NAME_None,FPaths::ProjectContentDir()/TEXT("UI/Fonts/Barlow/Barlow-Medium.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
        FSlateFontInfo F(Bold?Medium:Regular,Size);F.LetterSpacing=Bold?25:10;return F;
    }
    inline FSlateFontInfo Heading(int32 Size,bool Bold=false)
    {
        static const TSharedPtr<const FCompositeFont> Semi=MakeShared<FCompositeFont>(NAME_None,FPaths::ProjectContentDir()/TEXT("UI/Fonts/Oxanium/Oxanium-SemiBold.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
        static const TSharedPtr<const FCompositeFont> Heavy=MakeShared<FCompositeFont>(NAME_None,FPaths::ProjectContentDir()/TEXT("UI/Fonts/Oxanium/Oxanium-Bold.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
        FSlateFontInfo F(Bold?Heavy:Semi,Size);F.LetterSpacing=25;return F;
    }
    inline TSharedRef<STextBlock> Text(const FString& Value,int32 Size=12,FLinearColor Color=Cream,bool Head=false)
    { return SNew(STextBlock).Text(FText::FromString(Value)).Font(Head?Heading(Size):Font(Size)).ColorAndOpacity(Color); }
    inline const FButtonStyle& BareButton()
    {
        static const FButtonStyle S=FButtonStyle().SetNormal(FSlateNoResource()).SetHovered(FSlateNoResource()).SetPressed(FSlateNoResource()).SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));return S;
    }
    inline const FButtonStyle& QuietButton()
    {
        static const FButtonStyle S=FButtonStyle().SetNormal(FSlateColorBrush(FLinearColor::Transparent)).SetHovered(FSlateColorBrush(Accent.CopyWithNewOpacity(.10f))).SetPressed(FSlateColorBrush(Accent.CopyWithNewOpacity(.18f))).SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));return S;
    }
    inline const FEditableTextBoxStyle& SearchStyle()
    {
        static const FEditableTextBoxStyle S=FEditableTextBoxStyle()
            .SetBackgroundImageNormal(FSlateColorBrush(SRGB(15,15,14,.8f)))
            .SetBackgroundImageHovered(FSlateColorBrush(SRGB(40,37,32,.85f)))
            .SetBackgroundImageFocused(FSlateColorBrush(SRGB(48,43,35,.9f)))
            .SetBackgroundImageReadOnly(FSlateColorBrush(Ink))
            .SetForegroundColor(Cream).SetFocusedForegroundColor(Cream).SetPadding(FMargin(12,7));return S;
    }
    inline TSharedRef<SWidget> Rule(FLinearColor C=Line)
    { return SNew(SBox).HeightOverride(1)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(C).Padding(0)]; }
    inline FString TileLabel(FName Id)
    {
        static const TMap<FName,FString> Labels={
            {TEXT("rifle"),TEXT("M4A1")},{TEXT("smg"),TEXT("MP5")},{TEXT("pistol"),TEXT("USP")},
            {TEXT("pack_ak"),TEXT("AK")},{TEXT("pack_m14"),TEXT("M14")},{TEXT("pack_mac10"),TEXT("MAC-10")},
            {TEXT("pack_x24"),TEXT("X24")},{TEXT("pack_shotgun"),TEXT("SHOTGUN")},
            {TEXT("magazine"),TEXT("MAG")},{TEXT("ammo"),TEXT("5.56")},{TEXT("armor"),TEXT("CARRIER")},
            {TEXT("medkit"),TEXT("MEDKIT")},{TEXT("bandage"),TEXT("BAND")},{TEXT("backpack"),TEXT("PACK")},
            {TEXT("keycard"),TEXT("ACCESS")},{TEXT("radio"),TEXT("RADIO")},{TEXT("wrench"),TEXT("WRENCH")},
            {TEXT("electronics"),TEXT("MODULE")},{TEXT("fuel"),TEXT("FUEL")}};
        const FString* L=Labels.Find(Id);return L?*L:Id.ToString().ToUpper();
    }
}

/** Chamfered crew-terminal surface. Input stays with the enclosed Slate button. */
class SDepotPlate : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SDepotPlate):_Padding(18),_Primary(false),_Active(false),_Marks(true),_Corner(8.f),_Fill(DepotStyle::Ink){}
        SLATE_DEFAULT_SLOT(FArguments,Content)
        SLATE_ARGUMENT(FMargin,Padding)
        SLATE_ARGUMENT(bool,Primary)
        SLATE_ARGUMENT(bool,Active)
        SLATE_ARGUMENT(bool,Marks)
        SLATE_ARGUMENT(float,Corner)
        SLATE_ARGUMENT(FLinearColor,Fill)
        SLATE_EVENT(FOnClicked,OnClicked)
    SLATE_END_ARGS()
    void Construct(const FArguments& A)
    {
        bPrimary=A._Primary;bActive=A._Active;bMarks=A._Marks;Corner=A._Corner;Fill=A._Fill;bInteractive=A._OnClicked.IsBound();
        if(bInteractive)
            ChildSlot[SNew(SButton).ButtonStyle(&DepotStyle::BareButton()).ContentPadding(A._Padding).HAlign(HAlign_Fill).VAlign(VAlign_Fill).Cursor(EMouseCursor::Hand)
                .OnClicked(A._OnClicked).OnPressed_Lambda([this](){bPressed=true;Invalidate(EInvalidateWidgetReason::Paint);})
                .OnReleased_Lambda([this](){bPressed=false;Invalidate(EInvalidateWidgetReason::Paint);})[A._Content.Widget]];
        else ChildSlot.Padding(A._Padding)[A._Content.Widget];
    }
    virtual void OnMouseEnter(const FGeometry& G,const FPointerEvent& P) override
    { SCompoundWidget::OnMouseEnter(G,P);Invalidate(EInvalidateWidgetReason::Paint); }
    virtual void OnMouseLeave(const FPointerEvent& P) override
    { SCompoundWidget::OnMouseLeave(P);bPressed=false;Invalidate(EInvalidateWidgetReason::Paint); }
    virtual int32 OnPaint(const FPaintArgs& A,const FGeometry& G,const FSlateRect& Clip,FSlateWindowElementList& E,int32 L,const FWidgetStyle& Style,bool Enabled)const override
    {
        const FVector2D Size=G.GetLocalSize();const float W=static_cast<float>(Size.X),H=static_cast<float>(Size.Y);
        if(W<=1||H<=1)return L;
        const bool Hover=bInteractive&&IsHovered();
        FLinearColor Surface=(bPrimary||bActive)?DepotStyle::Cream:Fill;
        if(Hover)Surface=(bPrimary||bActive)?DepotStyle::SRGB(248,242,228):DepotStyle::SRGB(43,39,33,.98f);
        if(bPressed)Surface=(bPrimary||bActive)?DepotStyle::SRGB(206,195,176):DepotStyle::SRGB(29,27,23);
        Surface*=Style.GetColorAndOpacityTint();
        const float C=FMath::Min(Corner,FMath::Min(W,H)*.25f);
        const TArray<FVector2f> P={{.5f,.5f},{W-C,.5f},{W-.5f,C},{W-.5f,H-.5f},{C,H-.5f},{.5f,H-C}};
        TArray<FSlateVertex> V;TArray<SlateIndex> Indices;
        for(const FVector2f& Pos:P)V.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(),Pos,FVector2f(.5f,.5f),Surface.ToFColor(true)));
        for(int32 I=1;I<P.Num()-1;I++){Indices.Add(0);Indices.Add(static_cast<SlateIndex>(I));Indices.Add(static_cast<SlateIndex>(I+1));}
        const FSlateResourceHandle Handle=FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush("WhiteBrush"));
        FSlateDrawElement::MakeCustomVerts(E,L,Handle,V,Indices,nullptr,0,0);
        TArray<FVector2f> Edge=P;Edge.Add(P[0]);
        const FLinearColor Border=(Hover||bActive)?DepotStyle::Accent:DepotStyle::Line;
        FSlateDrawElement::MakeLines(E,L+1,G.ToPaintGeometry(),MoveTemp(Edge),ESlateDrawEffect::None,Border,true,1.f);
        if(bMarks)
        {
            const FLinearColor Mark=bPrimary?DepotStyle::Red:DepotStyle::Accent.CopyWithNewOpacity(Hover||bActive?1.f:.76f);
            FSlateDrawElement::MakeLines(E,L+2,G.ToPaintGeometry(),TArray<FVector2f>{{1,13},{1,1},{15,1}},ESlateDrawEffect::None,Mark,true,bPrimary?3.f:2.f);
            FSlateDrawElement::MakeLines(E,L+2,G.ToPaintGeometry(),TArray<FVector2f>{{W-15,H-1},{W-1,H-1},{W-1,H-13}},ESlateDrawEffect::None,Hover?DepotStyle::Accent:Mark,true,2.f);
        }
        return SCompoundWidget::OnPaint(A,G,Clip,E,L+3,Style,Enabled);
    }
private:
    bool bPrimary=false,bActive=false,bMarks=true,bInteractive=false,bPressed=false;
    float Corner=8.f;FLinearColor Fill;
};

class SDepotShade : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SDepotShade):_Destination(false){} SLATE_ARGUMENT(bool,Destination) SLATE_END_ARGS()
    void Construct(const FArguments& A){bDestination=A._Destination;SetVisibility(EVisibility::HitTestInvisible);}
    virtual FVector2D ComputeDesiredSize(float)const override{return FVector2D(1,1);}
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& E,int32 L,const FWidgetStyle&,bool)const override
    {
        const FVector2D S=G.GetLocalSize();
        auto Stop=[](float X,float Y,float Alpha){return FSlateGradientStop(FVector2f(X,Y),FLinearColor(.007f,.006f,.004f,Alpha));};
        if(bDestination)
            FSlateDrawElement::MakeGradient(E,L,G.ToPaintGeometry(),{Stop(0,0,.94f),Stop(S.X*.40f,0,.60f),Stop(S.X,0,.04f)},Orient_Vertical);
        else
        {
            FSlateDrawElement::MakeGradient(E,L,G.ToPaintGeometry(),{Stop(0,0,.35f),Stop(S.X*.24f,0,0),Stop(S.X*.69f,0,0),Stop(S.X,0,.40f)},Orient_Vertical);
            FSlateDrawElement::MakeGradient(E,L+1,G.ToPaintGeometry(),{Stop(0,0,.57f),Stop(0,105,.13f),Stop(0,250,0),Stop(0,S.Y*.72f,0),Stop(0,S.Y,.43f)},Orient_Horizontal);
        }
        return bDestination?L:L+1;
    }
private:bool bDestination=false;
};

class SDepotMark : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SDepotMark){} SLATE_END_ARGS()
    void Construct(const FArguments&){SetVisibility(EVisibility::HitTestInvisible);}
    virtual FVector2D ComputeDesiredSize(float)const override{return FVector2D(42,42);}
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& E,int32 L,const FWidgetStyle&,bool)const override
    {
        FSlateDrawElement::MakeLines(E,L,G.ToPaintGeometry(),TArray<FVector2f>{{3,34},{3,3},{29,3},{39,13},{39,39},{12,39}},ESlateDrawEffect::None,DepotStyle::Cream,true,1.5f);
        for(int32 I=0;I<3;I++)FSlateDrawElement::MakeLines(E,L,G.ToPaintGeometry(),TArray<FVector2f>{{9.f+I*7,29},{17.f+I*7,13}},ESlateDrawEffect::None,DepotStyle::Cream,true,3.f);
        FSlateDrawElement::MakeLines(E,L,G.ToPaintGeometry(),TArray<FVector2f>{{3,34},{3,39},{9,39}},ESlateDrawEffect::None,DepotStyle::Accent,true,2.f);
        return L;
    }
};
