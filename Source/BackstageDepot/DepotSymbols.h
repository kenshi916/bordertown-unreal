#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include <initializer_list>

/** Small, texture-free control symbols drawn directly by Slate. */
class SDepotSymbol final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SDepotSymbol)
        : _Kind(TEXT("stash")), _Tint(FLinearColor::White), _Size(36.f)
    {}
        SLATE_ARGUMENT(FString, Kind)
        SLATE_ARGUMENT(FLinearColor, Tint)
        SLATE_ARGUMENT(float, Size)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        SymbolKind=InArgs._Kind.TrimStartAndEnd().ToLower();
        SymbolTint=InArgs._Tint;
        SymbolSize=FMath::Max(1.f,InArgs._Size);
        SetCanTick(false);
        SetVisibility(EVisibility::HitTestInvisible);
        BuildPaths();
    }

private:
    FString SymbolKind;
    FLinearColor SymbolTint=FLinearColor::White;
    float SymbolSize=36.f;
    TArray<TArray<FVector2f>> Paths;

    virtual FVector2D ComputeDesiredSize(float) const override
    {
        return FVector2D(SymbolSize,SymbolSize);
    }

    virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,FSlateWindowElementList& OutDrawElements,
        int32 LayerId,const FWidgetStyle& InWidgetStyle,bool bParentEnabled) const override
    {
        const FVector2D Local=AllottedGeometry.GetLocalSize();
        const float Extent=static_cast<float>(FMath::Min(Local.X,Local.Y));
        if(Extent<=0.f||Paths.IsEmpty())return LayerId;
        const float Scale=Extent/36.f;
        const FVector2f Origin(static_cast<float>(Local.X-Extent)*.5f,static_cast<float>(Local.Y-Extent)*.5f);
        const FLinearColor DrawTint=SymbolTint*InWidgetStyle.GetColorAndOpacityTint();
        if(DrawTint.A<=UE_SMALL_NUMBER)return LayerId;
        const ESlateDrawEffect Effects=ShouldBeEnabled(bParentEnabled)?ESlateDrawEffect::None:ESlateDrawEffect::DisabledEffect;
        const FPaintGeometry PaintGeometry=AllottedGeometry.ToPaintGeometry();
        // Geometry and strokes share the same local scale; Slate handles viewport DPI.
        const float Stroke=1.5f*Scale;
        for(const TArray<FVector2f>& Source:Paths)
        {
            TArray<FVector2f> Points;
            Points.Reserve(Source.Num());
            for(const FVector2f& P:Source)Points.Add(Origin+P*Scale);
            FSlateDrawElement::MakeLines(OutDrawElements,LayerId,PaintGeometry,MoveTemp(Points),Effects,DrawTint,true,Stroke);
        }
        return LayerId;
    }

    void Path(std::initializer_list<FVector2f> Points,bool bClosed=false)
    {
        TArray<FVector2f>& P=Paths.AddDefaulted_GetRef();
        P.Reserve(static_cast<int32>(Points.size())+(bClosed?1:0));
        for(const FVector2f& V:Points)P.Add(V);
        if(bClosed&&P.Num()>2){const FVector2f First=P[0];P.Add(First);}
    }

    void Circle(float X,float Y,float Radius,int32 Segments=24)
    {
        TArray<FVector2f>& P=Paths.AddDefaulted_GetRef();
        P.Reserve(Segments+1);
        for(int32 I=0;I<=Segments;I++)
        {
            const float Angle=2.f*PI*static_cast<float>(I)/static_cast<float>(Segments);
            P.Emplace(X+FMath::Cos(Angle)*Radius,Y+FMath::Sin(Angle)*Radius);
        }
    }

    void BuildPaths()
    {
        Paths.Reset();
        if(SymbolKind==TEXT("stash")||SymbolKind==TEXT("crate"))
        {
            Path({{6,12},{30,12},{30,28},{28,30},{8,30},{6,28}},true);
            Path({{5,8},{31,8},{31,12},{5,12}},true);
            Path({{13,8},{13,5},{23,5},{23,8}});
            Path({{10,11},{10,16},{13,16},{13,11}});
            Path({{23,11},{23,16},{26,16},{26,11}});
            Path({{16,18},{20,18},{20,22},{16,22}},true);
            Path({{10,26},{14,26}});
            Path({{22,26},{26,26}});
        }
        else if(SymbolKind==TEXT("workbench")||SymbolKind==TEXT("tools"))
        {
            // Open-jaw spanner runs southwest to northeast.
            Path({{6,27},{9,30},{22,17},{26,17},{30,13},{31,8},{28,5},
                  {28,10},{25,13},{22,10},{25,7},{20,7},{17,11},{17,15}},true);
            Circle(8.6f,27.4f,1.15f,12);
            // Screwdriver behind the spanner; the crossing has a deliberate gap.
            Path({{6,5},{10,6},{14,10},{11,13},{7,9}},true);
            Path({{12,12},{16,16}});
            Path({{21,21},{28,28},{30,29},{29,27},{22,20}});
        }
        else if(SymbolKind==TEXT("deployment")||SymbolKind==TEXT("compass")||SymbolKind==TEXT("raid"))
        {
            Circle(18,18,13,32);
            Path({{27,9},{21,25},{17,19},{11,15}},true);
            Path({{17,19},{27,9}});
            Path({{18,3},{18,5}});
            Path({{31,18},{33,18}});
            Path({{18,31},{18,33}});
            Path({{3,18},{5,18}});
        }
        else if(SymbolKind==TEXT("operator")||SymbolKind==TEXT("badge"))
        {
            Path({{18,3},{29,7},{28,20},{24,27},{18,32},{12,27},{8,20},{7,7}},true);
            Circle(18,12,3.4f,20);
            Path({{11,23},{12,20},{15,18},{21,18},{24,20},{25,23}});
            Path({{14,26},{22,26}});
        }
        else if(SymbolKind==TEXT("settings")||SymbolKind==TEXT("sliders"))
        {
            Path({{5,9},{8,9}}); Path({{14,9},{31,9}});
            Path({{8,5.5f},{14,5.5f},{14,12.5f},{8,12.5f}},true);
            Path({{5,18},{22,18}}); Path({{28,18},{31,18}});
            Path({{22,14.5f},{28,14.5f},{28,21.5f},{22,21.5f}},true);
            Path({{5,27},{13,27}}); Path({{19,27},{31,27}});
            Path({{13,23.5f},{19,23.5f},{19,30.5f},{13,30.5f}},true);
        }
        else if(SymbolKind==TEXT("contracts")||SymbolKind==TEXT("clipboard"))
        {
            Path({{13,7},{8,7},{8,30},{10,32},{26,32},{28,30},{28,7},{23,7}});
            Path({{13,4},{23,4},{23,10},{13,10}},true);
            Path({{11,16},{12.5f,17.5f},{15,14}});
            Path({{18,16},{24,16}});
            Path({{11,23},{12.5f,24.5f},{15,21}});
            Path({{18,23},{24,23}});
            Path({{11,28},{24,28}});
        }
        else if(SymbolKind==TEXT("inventory")||SymbolKind==TEXT("backpack")||SymbolKind==TEXT("loadout"))
        {
            Path({{14,8},{14,4},{22,4},{22,8}});
            Path({{11,8},{25,8},{29,13},{29,29},{27,32},{9,32},{7,29},{7,13}},true);
            Path({{11,19},{25,19},{25,28},{11,28}},true);
            Path({{11,15},{25,15}});
            Path({{21,15},{21,17}});
            Path({{7,17},{4,19},{4,27},{7,28}});
            Path({{29,17},{32,19},{32,27},{29,28}});
            Path({{17,22},{19,22}});
        }
        else // Chevron is also a safe fallback for an unknown symbol name.
        {
            Path({{13,7},{24,18},{13,29}});
        }
    }
};
