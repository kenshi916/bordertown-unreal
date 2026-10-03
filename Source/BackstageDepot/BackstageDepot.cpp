#include "BackstageDepot.h"
#include "DepotPackPreview.h"
#include "Modules/ModuleManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimationAsset.h"
#include "Misc/PackageName.h"
#include "Components/SkeletalMeshComponent.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Images/SImage.h"
#include "TimerManager.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Brushes/SlateColorBrush.h"

IMPLEMENT_GAME_MODULE(FDefaultGameModuleImpl, BackstageDepot);

#include "DepotChrome.h"

class SDepotWeaponInspector : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SDepotWeaponInspector){}
        SLATE_ARGUMENT(ABackstageDepotPlayerController*, Owner)
        SLATE_DEFAULT_SLOT(FArguments, Content)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args)
    {
        Owner = Args._Owner;
        SetCursor(EMouseCursor::GrabHand);
        ChildSlot[Args._Content.Widget];
    }

    virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent& Event) override
    {
        if (Event.GetEffectingButton() == EKeys::LeftMouseButton && Owner.IsValid())
            return FReply::Handled().CaptureMouse(SharedThis(this));
        return FReply::Unhandled();
    }

    virtual FReply OnMouseMove(const FGeometry&, const FPointerEvent& Event) override
    {
        if (HasMouseCapture() && Owner.IsValid())
        {
            Owner->RotateCosmeticPreview(Event.GetCursorDelta().X * 0.35f);
            return FReply::Handled();
        }
        return FReply::Unhandled();
    }

    virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent& Event) override
    {
        if (Event.GetEffectingButton() == EKeys::LeftMouseButton && HasMouseCapture())
            return FReply::Handled().ReleaseMouseCapture();
        return FReply::Unhandled();
    }

private:
    TWeakObjectPtr<ABackstageDepotPlayerController> Owner;
};

class SBackstageDepotScreen : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SBackstageDepotScreen){} SLATE_ARGUMENT(ABackstageDepotPlayerController*,Owner) SLATE_END_ARGS()
    void Construct(const FArguments& A)
    {
        Owner=A._Owner;
        ChildSlot[SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::Both)
            [SNew(SBox).WidthOverride(1600).HeightOverride(900)
                [SNew(SOverlay)+SOverlay::Slot()[SNew(SDepotShade)]+SOverlay::Slot()[SAssignNew(Root,SVerticalBox)]]]];
        Refresh();
    }
    void SetPage(const FString& NewPage)
    {
        if(Owner.IsValid()&&Page==TEXT("ARMORY")&&NewPage!=Page)Owner->HidePackWeaponPreview();
        if(Owner.IsValid()&&Page==TEXT("OPERATORS")&&!Owner->GetSelectedOperator().IsNone())Owner->PreviewOperator(Owner->GetSelectedOperator());
        Page=NewPage;Selected=NAME_None;Search.Empty();
        if(Owner.IsValid()&&Page==TEXT("OPERATORS"))
        {
            PreviewOperatorId=Owner->GetSelectedOperator();
            const TArray<FDepotOperator> Operators=Owner->GetInventorySubsystem()->GetOperators();
            if(PreviewOperatorId.IsNone()&&!Operators.IsEmpty())PreviewOperatorId=Operators[0].Id;
        }
        if(Owner.IsValid()&&Page==TEXT("ARMORY"))
        {
            const FDepotItem* Primary=Owner->FindItem(Owner->GetEquippedPrimary());
            ArmoryWeapon=Primary&&Primary->bPackWeapon?Primary->Id:FName(TEXT("pack_ak"));
            SetArmoryWeapon(ArmoryWeapon);
        }
        Refresh();
    }
    void Escape(){if(!Selected.IsNone()){Selected=NAME_None;Refresh();}else SetPage(TEXT("HUB"));}
    virtual FReply OnPreviewKeyDown(const FGeometry&,const FKeyEvent& Event)override
    {
        if(Event.GetKey()==EKeys::Escape){Escape();return FReply::Handled();}
        if(Event.GetKey()==EKeys::Tab){SetPage(TEXT("LOADOUT"));return FReply::Handled();}
        if(Event.GetKey()==EKeys::Left){Owner->RotateCosmeticPreview(-15.f);return FReply::Handled();}
        if(Event.GetKey()==EKeys::Right){Owner->RotateCosmeticPreview(15.f);return FReply::Handled();}
        return FReply::Unhandled();
    }
    void Refresh()
    {
        if(!Owner.IsValid()||!Root.IsValid())return;
        Root->ClearChildren();Root->AddSlot().AutoHeight()[Header()];Root->AddSlot().FillHeight(1)[Body()];Root->AddSlot().AutoHeight()[Footer()];
    }
private:
    TWeakObjectPtr<ABackstageDepotPlayerController> Owner;
    TSharedPtr<SVerticalBox> Root;
    TMap<FName,TSharedPtr<FSlateBrush>> Brushes;
    FString Page=TEXT("HUB"),Filter=TEXT("ALL"),Search;
    FName Selected;
    FName PreviewOperatorId,ArmoryWeapon=TEXT("pack_ak"),PreviewFinishId;
    bool bWeaponPreviewReady=false;
    using FClick=TFunction<FReply()>;
    FString OperatorName()const
    {
        if(UBackstageDepotInventorySubsystem* State=Owner->GetInventorySubsystem())
            for(const FDepotOperator& Operator:State->GetOperators())if(Operator.Id==Owner->GetSelectedOperator())return Operator.DisplayName.ToUpper();
        return TEXT("OPERATOR");
    }
    TSharedRef<SWidget> Paragraph(const FString& Value,int32 Size=12,FLinearColor Color=DepotStyle::Muted)
    {return SNew(STextBlock).Text(FText::FromString(Value)).Font(DepotStyle::Font(Size)).ColorAndOpacity(Color).AutoWrapText(true);}
    void SetArmoryWeapon(FName WeaponId)
    {
        ArmoryWeapon=WeaponId;PreviewFinishId=Owner->GetWeaponFinish(WeaponId);
        const TArray<FDepotWeaponFinish> Finishes=Owner->GetInventorySubsystem()->GetWeaponFinishesForWeapon(WeaponId);
        if(PreviewFinishId.IsNone()&&!Finishes.IsEmpty())PreviewFinishId=Finishes[0].Id;
        bWeaponPreviewReady=Owner->SetPackWeaponPreview(ArmoryWeapon,PreviewFinishId);
    }
    TSharedRef<SWidget> Button(const FString& Label,FClick Click,bool Primary=false,float Width=0,float Height=0)
    {
        TSharedRef<SWidget> B=SNew(SBox).HeightOverride(Height>0?Height:(Primary?56:44))
            [SNew(SDepotPlate).Primary(Primary).Padding(FMargin(20,8)).OnClicked_Lambda([Click=MoveTemp(Click)](){return Click();})
                [SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                    [DepotStyle::Text(Label,Primary?18:12,Primary?DepotStyle::ActionText:DepotStyle::Cream,true)]
                    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14,0,0,0)
                    [SNew(SDepotSymbol).Kind(TEXT("chevron")).Tint(Primary?DepotStyle::ActionText:DepotStyle::Muted).Size(22)]]];
        return Width>0?StaticCastSharedRef<SWidget>(SNew(SBox).WidthOverride(Width)[B]):B;
    }
    TSharedRef<SWidget> Icon(FName Id,float W,float H,bool Cover=false)
    {
        UTexture2D* Texture=Owner->Icon(Id);
        if(!Texture)return SNew(SBox).WidthOverride(W).HeightOverride(H)[SNew(SDepotSymbol).Kind(TEXT("inventory")).Tint(DepotStyle::Muted).Size(FMath::Min(W,H))];
        TSharedPtr<FSlateBrush>& Brush=Brushes.FindOrAdd(Id);
        if(!Brush.IsValid()){Brush=MakeShared<FSlateBrush>();Brush->SetResourceObject(Texture);const FIntPoint ImportedSize=Texture->GetImportedSize();Brush->ImageSize=FVector2D(FMath::Max(1,ImportedSize.X),FMath::Max(1,ImportedSize.Y));Brush->DrawAs=ESlateBrushDrawType::Image;}
        return SNew(SBox).WidthOverride(W).HeightOverride(H).Clipping(EWidgetClipping::ClipToBounds)
            [SNew(SScaleBox).Stretch(Cover?EStretch::ScaleToFill:EStretch::ScaleToFit)[SNew(SImage).Image(Brush.Get())]];
    }
    TSharedRef<SWidget> Header()
    {
        TSharedRef<SHorizontalBox> Nav=SNew(SHorizontalBox);
        for(const FString& Name:{TEXT("HUB"),TEXT("OPERATORS"),TEXT("LOADOUT"),TEXT("WORKBENCH"),TEXT("CONTRACTS")})
        {
            const bool Active=Page==Name||(Name==TEXT("HUB")&&Page==TEXT("RAID"))||(Name==TEXT("WORKBENCH")&&Page==TEXT("ARMORY"));
            Nav->AddSlot().AutoWidth().Padding(0,0,7,0)
                [SNew(SBox).HeightOverride(44)
                    [SNew(SDepotPlate).Active(Active).Marks(Active).Corner(6).Fill(DepotStyle::Ink.CopyWithNewOpacity(.44f)).Padding(FMargin(17,9))
                        .OnClicked_Lambda([this,Name](){SetPage(Name);return FReply::Handled();})
                        [DepotStyle::Text(Name==TEXT("HUB")?TEXT("DEPOT"):Name,14,Active?DepotStyle::ActionText:DepotStyle::Cream,true)]]];
        }
        return SNew(SBox).HeightOverride(100)
            [SNew(SOverlay)
                +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(48,24,0,0)
                    [SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().Padding(0,0,14,0)[SNew(SDepotMark)]
                        +SHorizontalBox::Slot().AutoWidth()[SNew(SVerticalBox)
                            +SVerticalBox::Slot().AutoHeight()[DepotStyle::Text(TEXT("BACKSTAGE"),24,DepotStyle::Cream,true)]
                            +SVerticalBox::Slot().AutoHeight().Padding(1,4,0,0)[DepotStyle::Text(TEXT("D E P O T  /  0 4"),9,DepotStyle::Muted)]]]
                +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0,28,0,0)[Nav]
                +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0,27,48,0)
                    [SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,15,0)
                        [SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)[DepotStyle::Text(OperatorName(),14,DepotStyle::Cream,true)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,3,0,0)[DepotStyle::Text(TEXT("CREW ID / 04"),10,DepotStyle::Muted)]]
                        +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(44).HeightOverride(44)
                            [SNew(SDepotPlate).Active(Page==TEXT("SETTINGS")).Marks(false).Padding(10).Corner(6)
                                .OnClicked_Lambda([this](){SetPage(TEXT("SETTINGS"));return FReply::Handled();})
                                [SNew(SDepotSymbol).Kind(TEXT("settings")).Tint(Page==TEXT("SETTINGS")?DepotStyle::ActionText:DepotStyle::Cream).Size(24)]]]]
                +SOverlay::Slot().VAlign(VAlign_Bottom).Padding(48,0,48,8)[DepotStyle::Rule(DepotStyle::Line.CopyWithNewOpacity(.17f))]];
    }
    TSharedRef<SWidget> Footer()
    {
        auto Key=[](const FString& K,const FString& Name)->TSharedRef<SWidget>
        {
            return SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth()
                [SNew(SDepotPlate).Marks(false).Corner(3).Padding(FMargin(5,2))[DepotStyle::Text(K,9,DepotStyle::Cream)]]
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(7,0,0,0)[DepotStyle::Text(Name,10,DepotStyle::Muted)];
        };
        return SNew(SBox).HeightOverride(52)
            [SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight().Padding(48,0,48,0)[DepotStyle::Rule(DepotStyle::Line.CopyWithNewOpacity(.14f))]
                +SVerticalBox::Slot().FillHeight(1).Padding(48,0)
                    [SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[DepotStyle::Text(TEXT("BACKSTAGE  /  PERSONAL QUARTERS"),10,DepotStyle::Muted)]
                        +SHorizontalBox::Slot().FillWidth(1).HAlign(HAlign_Center).VAlign(VAlign_Center)[DepotStyle::Text(Owner->Feedback,10,DepotStyle::Accent)]
                        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,18,0)[Key(TEXT("TAB"),TEXT("Loadout"))]
                        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,18,0)[Key(TEXT("ESC"),TEXT("Back"))]
                        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Key(TEXT("< >"),TEXT("Rotate"))]]];
    }
    TSharedRef<SWidget> Body()
    {
        if(Page==TEXT("LOADOUT"))return Loadout();if(Page==TEXT("WORKBENCH"))return Workshop();
        if(Page==TEXT("OPERATORS"))return Operators();if(Page==TEXT("ARMORY"))return Armory();
        if(Page==TEXT("SETTINGS"))return Settings();if(Page==TEXT("CONTRACTS"))return Contracts();if(Page==TEXT("RAID"))return Raid();return Hub();
    }
    TSharedRef<SWidget> ConditionBar()
    {
        TSharedRef<SHorizontalBox> Bars=SNew(SHorizontalBox);
        for(int32 I=0;I<10;I++)
        {
            const float Amount=FMath::Clamp(Owner->Inventory()->Health/10.f-I,0.f,1.f);
            Bars->AddSlot().AutoWidth().Padding(0,0,I<9?3:0,0)
                [SNew(SBox).WidthOverride(18).HeightOverride(5)[SNew(SOverlay)
                    +SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(DepotStyle::Line).Padding(0)]
                    +SOverlay::Slot().HAlign(HAlign_Left)[SNew(SBox).WidthOverride(18*Amount)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(DepotStyle::Accent).Padding(0)]]]];
        }
        return Bars;
    }
    TSharedRef<SWidget> Hub()
    {
        const FDepotItem* Primary=Owner->FindItem(Owner->Inventory()->Primary);
        const FName DestinationId=Owner->Inventory()->SelectedMap;
        const TArray<FDepotDestination> Destinations=Owner->GetInventorySubsystem()->GetDestinations();
        const FDepotDestination* SelectedDestination=Destinations.FindByPredicate([DestinationId](const FDepotDestination& Destination){return Destination.Id==DestinationId;});
        const FString DestinationName=SelectedDestination?SelectedDestination->DisplayName:TEXT("CHOOSE MAP");
        TSharedRef<SWidget> DestinationArtwork=SNew(SBox).WidthOverride(344).HeightOverride(94);
        if(DestinationId==TEXT("Carnival")) DestinationArtwork=Icon(TEXT("destination"),344,94,true);
        else if(DestinationId==TEXT("BorderTown")) DestinationArtwork=Icon(TEXT("town_destination"),344,94,true);
        auto Station=[this](const FString& Label,const FString& Kind,const FString& Bay,const FString& Detail,const FString& Target)->TSharedRef<SWidget>
        {
            return SNew(SBox).WidthOverride(350).HeightOverride(84)
                [SNew(SDepotPlate).Padding(FMargin(20,14)).OnClicked_Lambda([this,Target](){SetPage(Target);return FReply::Handled();})
                    [SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,16,0)[SNew(SDepotSymbol).Kind(Kind).Tint(DepotStyle::Cream).Size(38)]
                        +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(SVerticalBox)
                            +SVerticalBox::Slot().AutoHeight()[DepotStyle::Text(Label,17,DepotStyle::Cream,true)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,5,0,0)[DepotStyle::Text(Bay+TEXT("  /  ")+Detail,10,DepotStyle::Muted)]]
                        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SDepotSymbol).Kind(TEXT("chevron")).Tint(DepotStyle::Muted).Size(20)]]];
        };
        TSharedRef<SVerticalBox> PrimaryContent=SNew(SVerticalBox);
        PrimaryContent->AddSlot().AutoHeight()[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[DepotStyle::Text(TEXT("PRIMARY LOADOUT"),11,DepotStyle::Muted,true)]
            +SHorizontalBox::Slot().AutoWidth()[SNew(SDepotSymbol).Kind(TEXT("inventory")).Tint(DepotStyle::Accent).Size(18)]];
        PrimaryContent->AddSlot().AutoHeight().Padding(0,5)[Primary?Icon(Primary->Id,310,70):StaticCastSharedRef<SWidget>(SNew(SBox).HeightOverride(70)[DepotStyle::Text(TEXT("NO PRIMARY EQUIPPED"),12,DepotStyle::Muted)])];
        PrimaryContent->AddSlot().AutoHeight()[DepotStyle::Text(Primary?Primary->Name:TEXT("EMPTY WEAPON SLOT"),16,DepotStyle::Cream,true)];
        PrimaryContent->AddSlot().AutoHeight().Padding(0,8,0,0)[DepotStyle::Rule()];
        PrimaryContent->AddSlot().AutoHeight().Padding(0,8,0,0)[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1)[DepotStyle::Text(FString::Printf(TEXT("FIELD KIT  /  %02d ITEMS"),Owner->Inventory()->FieldKit.Num()),10,DepotStyle::Muted)]
            +SHorizontalBox::Slot().AutoWidth()[DepotStyle::Text(TEXT("MANAGE  >"),10,DepotStyle::Accent)]];
        return SNew(SOverlay)
            +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(48,20,0,0)
                [SNew(SBox).WidthOverride(240).HeightOverride(112)[SNew(SDepotPlate).Padding(16)
                    [SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
                        +SHorizontalBox::Slot().FillWidth(1)[DepotStyle::Text(TEXT("FIELD CONDITION"),10,DepotStyle::Muted,true)]
                        +SHorizontalBox::Slot().AutoWidth()[DepotStyle::Text(TEXT("04"),10,DepotStyle::Accent,true)]]
                        +SVerticalBox::Slot().AutoHeight().Padding(0,11,0,10)[SNew(SHorizontalBox)
                            +SHorizontalBox::Slot().FillWidth(1)[DepotStyle::Text(FString::Printf(TEXT("%.0f"),Owner->Inventory()->Health),23,DepotStyle::Cream,true)]
                            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(0,0,0,3)[DepotStyle::Text(TEXT("HEALTH / 100"),10,DepotStyle::Muted)]]
                        +SVerticalBox::Slot().AutoHeight()[ConditionBar()]]]]
            +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0,20,48,0)
                [SNew(SBox).WidthOverride(350).HeightOverride(190)[SNew(SDepotPlate).Padding(20).OnClicked_Lambda([this](){SetPage(TEXT("LOADOUT"));return FReply::Handled();})[PrimaryContent]]]
            +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(48,0,0,24)
                [SNew(SBox).WidthOverride(360)[SNew(SVerticalBox)
                    +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
                        [SNew(SBox).HeightOverride(110)[SNew(SDepotPlate).Padding(8).OnClicked_Lambda([this](){SetPage(TEXT("RAID"));return FReply::Handled();})
                            [SNew(SOverlay)+SOverlay::Slot()[DestinationArtwork]
                                +SOverlay::Slot()[SNew(SDepotShade).Destination(true)]
                                +SOverlay::Slot().Padding(12,12)[SNew(SVerticalBox)
                                    +SVerticalBox::Slot().AutoHeight()[DepotStyle::Text(TEXT("SELECTED DESTINATION"),9,DepotStyle::Accent,true)]
                                    +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,5)[DepotStyle::Text(DestinationName,18,DepotStyle::Cream,true)]
                                    +SVerticalBox::Slot().AutoHeight()[DepotStyle::Text(TEXT("LOCAL PLAYTEST  /  CHOOSE MAP"),10,DepotStyle::Muted)]]]]]
                    +SVerticalBox::Slot().AutoHeight()[Button(TEXT("CHOOSE MAP"),[this](){SetPage(TEXT("RAID"));return FReply::Handled();},true,360,64)]]]
            +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0,0,48,24)
                [SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[Station(TEXT("STASH"),TEXT("stash"),TEXT("BAY 02"),FString::Printf(TEXT("%d ITEMS"),Owner->Inventory()->Stash.Num()),TEXT("LOADOUT"))]
                    +SVerticalBox::Slot().AutoHeight().Padding(0,12,0,0)[Station(TEXT("WORKBENCH"),TEXT("workbench"),TEXT("BAY 03"),TEXT("LOADOUT PRESETS"),TEXT("WORKBENCH"))]]
            +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0,0,0,18)
                [SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,10,0)[SNew(SDepotSymbol).Kind(TEXT("operator")).Tint(DepotStyle::Accent).Size(22)]
                    +SHorizontalBox::Slot().AutoWidth()[SNew(SDepotPlate).Marks(false).Padding(FMargin(10,6)).OnClicked_Lambda([this](){SetPage(TEXT("OPERATORS"));return FReply::Handled();})[DepotStyle::Text(OperatorName()+TEXT("  /  CHANGE OPERATOR"),11,DepotStyle::Cream,true)]]];
    }

    TSharedRef<SWidget> ItemTile(const FDepotItem& Item)
    {
        const bool Active=Selected==Item.Id;
        const int32 Quantity=Owner->Inventory()->Quantities.FindRef(Item.Id);
        FLinearColor Edge=Active?DepotStyle::Accent:DepotStyle::Line;
        Edge.A=Active?.92f:.32f;
        FLinearColor Fill=DepotStyle::Panel;
        Fill.A=.91f;
        return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Edge).Padding(1)
            [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Fill).Padding(0)
                [SNew(SButton).ButtonStyle(&DepotStyle::QuietButton()).ContentPadding(7)
                    .ToolTipText(FText::FromString(Item.Name))
                    .OnClicked_Lambda([this,Id=Item.Id](){Selected=Id;Refresh();return FReply::Handled();})
                    [SNew(SOverlay)
                        +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
                            [Icon(Item.Id,Item.Width*64.f-24.f,Item.Height*54.f-24.f)]
                        +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)
                            [DepotStyle::Text(DepotStyle::TileLabel(Item.Id),Item.Width>1?10:9,DepotStyle::Cream)]
                        +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom)
                            [DepotStyle::Text(Quantity>1?FString::Printf(TEXT("%d"),Quantity):TEXT(""),10,DepotStyle::Muted)]]]];
    }

    TSharedRef<SWidget> InventoryGrid()
    {
        constexpr int32 Columns=8, Rows=16;
        constexpr float CellWidth=64.f, CellHeight=54.f;
        TSharedRef<SGridPanel> Grid=SNew(SGridPanel);
        FLinearColor GridLine=DepotStyle::Line; GridLine.A=.33f;
        const FLinearColor EmptyFill=DepotStyle::SRGB(23,22,20,.98f);
        for(int32 X=0;X<Columns;X++) Grid->SetColumnFill(X,1.f);
        for(int32 Y=0;Y<Rows;Y++)
        {
            Grid->SetRowFill(Y,1.f);
            for(int32 X=0;X<Columns;X++)
                Grid->AddSlot(X,Y).Layer(0)
                    [SNew(SBox).WidthOverride(CellWidth).HeightOverride(CellHeight).Visibility(EVisibility::HitTestInvisible)
                        [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(GridLine).Padding(1.f)
                            [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(EmptyFill).Padding(0)]]];
        }
        bool Used[Rows][Columns]={};
        for(const FDepotItem& Item:Owner->Items())
        {
            if(!Owner->Inventory()->Stash.Contains(Item.Id))continue;
            if(Filter!=TEXT("ALL")&&Filter!=Item.Category)continue;
            if(!Search.IsEmpty()&&!Item.Name.Contains(Search,ESearchCase::IgnoreCase))continue;
            int32 GX=-1,GY=-1;
            for(int32 Y=0;Y<=Rows-Item.Height&&GY<0;Y++)
                for(int32 X=0;X<=Columns-Item.Width;X++)
                {
                    bool Fits=true;
                    for(int32 DY=0;DY<Item.Height;DY++)for(int32 DX=0;DX<Item.Width;DX++)if(Used[Y+DY][X+DX])Fits=false;
                    if(Fits){GX=X;GY=Y;break;}
                }
            if(GY<0)continue;
            for(int32 DY=0;DY<Item.Height;DY++)for(int32 DX=0;DX<Item.Width;DX++)Used[GY+DY][GX+DX]=true;
            Grid->AddSlot(GX,GY).ColumnSpan(Item.Width).RowSpan(Item.Height).Layer(1).Padding(2)
                [SNew(SBox).WidthOverride(Item.Width*CellWidth-4.f).HeightOverride(Item.Height*CellHeight-4.f)[ItemTile(Item)]];
        }
        return SNew(SBox).WidthOverride(Columns*CellWidth+8).HeightOverride(8*CellHeight)
            [SNew(SScrollBox).ScrollBarThickness(FVector2D(6,6)).ScrollBarPadding(FMargin(1,0))
                +SScrollBox::Slot()[SNew(SBox).WidthOverride(Columns*CellWidth).HeightOverride(Rows*CellHeight)[Grid]]];
    }

    TSharedRef<SWidget> Loadout()
    {
        TSharedRef<SVerticalBox> Left=SNew(SVerticalBox);
        Left->AddSlot().AutoHeight()[DepotStyle::Text(TEXT("OPERATOR / 04"),10,DepotStyle::Accent,true)];
        Left->AddSlot().AutoHeight().Padding(0,7,0,17)[DepotStyle::Text(TEXT("LOADOUT"),24,DepotStyle::Cream,true)];
        Left->AddSlot().AutoHeight()[DepotStyle::Rule()];
        Left->AddSlot().AutoHeight().Padding(0,17,0,7)[DepotStyle::Text(TEXT("PRIMARY WEAPON"),11,DepotStyle::Muted)];
        const FDepotItem* Primary=Owner->FindItem(Owner->Inventory()->Primary);
        if(Primary)
        {
            Left->AddSlot().AutoHeight()[SNew(SButton).ButtonStyle(&DepotStyle::QuietButton()).ContentPadding(FMargin(0,4))
                .OnClicked_Lambda([this,Id=Primary->Id](){Selected=Id;Refresh();return FReply::Handled();})[Icon(Primary->Id,256,108)]];
            Left->AddSlot().AutoHeight().Padding(0,10,0,5)[DepotStyle::Text(Primary->Name,17,DepotStyle::Cream,true)];
            Left->AddSlot().AutoHeight()[DepotStyle::Text(TEXT("Equipped / select to inspect"),11,DepotStyle::Muted)];
        }
        else Left->AddSlot().AutoHeight().Padding(0,35)[DepotStyle::Text(TEXT("EMPTY PRIMARY SLOT"),13,DepotStyle::Muted)];
        Left->AddSlot().AutoHeight().Padding(0,22,0,17)[DepotStyle::Rule()];
        Left->AddSlot().AutoHeight().Padding(0,0,0,12)
            [SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[DepotStyle::Text(TEXT("FIELD KIT"),12,DepotStyle::Cream,true)]
                +SHorizontalBox::Slot().AutoWidth()[DepotStyle::Text(FString::Printf(TEXT("%02d"),Owner->Inventory()->FieldKit.Num()),12,DepotStyle::Accent,true)]];
        if(Owner->Inventory()->FieldKit.IsEmpty())
            Left->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("Add supplies from your stash"))).Font(DepotStyle::Font(12)).ColorAndOpacity(DepotStyle::Muted).AutoWrapText(true)];
        else
        {
            TSharedRef<SScrollBox> Kit=SNew(SScrollBox);
            for(FName Id:Owner->Inventory()->FieldKit)if(const FDepotItem* Item=Owner->FindItem(Id))
                Kit->AddSlot().Padding(0,2)[SNew(SButton).ButtonStyle(&DepotStyle::QuietButton()).ContentPadding(FMargin(0,4))
                    .OnClicked_Lambda([this,Id](){Selected=Id;Refresh();return FReply::Handled();})
                    [SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().Padding(0,0,9,0)[Icon(Id,28,24)]
                        +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[DepotStyle::Text(DepotStyle::TileLabel(Id),11,DepotStyle::Cream)]
                        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[DepotStyle::Text(FString::Printf(TEXT("%d"),Owner->Inventory()->Quantities.FindRef(Id)),11,DepotStyle::Muted)]]];
            Left->AddSlot().AutoHeight()[SNew(SBox).HeightOverride(FMath::Min(172.f,Owner->Inventory()->FieldKit.Num()*38.f))[Kit]];
        }
        Left->AddSlot().AutoHeight().Padding(0,20,0,16)[DepotStyle::Rule()];
        Left->AddSlot().AutoHeight()[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[DepotStyle::Text(TEXT("HEALTH"),11,DepotStyle::Muted)]
            +SHorizontalBox::Slot().AutoWidth()[DepotStyle::Text(FString::Printf(TEXT("%.0f / 100"),Owner->Inventory()->Health),17,DepotStyle::Cream,true)]];
        Left->AddSlot().AutoHeight().Padding(0,12,0,0)[ConditionBar()];
        Left->AddSlot().FillHeight(1)[SNew(SSpacer)];
        Left->AddSlot().AutoHeight().Padding(0,16,0,0)[Button(TEXT("RETURN TO DEPOT"),[this](){SetPage(TEXT("HUB"));return FReply::Handled();})];

        TSharedRef<SVerticalBox> Stash=SNew(SVerticalBox);
        Stash->AddSlot().AutoHeight()[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,12,0)[SNew(SDepotSymbol).Kind(TEXT("stash")).Tint(DepotStyle::Cream).Size(28)]
            +SHorizontalBox::Slot().FillWidth(1)[DepotStyle::Text(TEXT("PERSONAL STASH"),24,DepotStyle::Cream,true)]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[DepotStyle::Text(FString::Printf(TEXT("%02d ITEMS"),Owner->Inventory()->Stash.Num()),11,DepotStyle::Accent)]];
        Stash->AddSlot().AutoHeight().Padding(0,17,0,10)
            [SNew(SDepotPlate).Marks(false).Corner(4).Padding(1).Fill(DepotStyle::Line)
                [SNew(SBox).HeightOverride(34)[SNew(SEditableTextBox).Style(&DepotStyle::SearchStyle()).HintText(FText::FromString(TEXT("Search equipment / Enter"))).Text(FText::FromString(Search)).Font(DepotStyle::Font(12))
                    .OnTextCommitted_Lambda([this](const FText& Text,ETextCommit::Type Commit){if(Commit==ETextCommit::OnEnter){Search=Text.ToString();Refresh();}})]]];
        TSharedRef<SHorizontalBox> Filters=SNew(SHorizontalBox);
        for(const FString& Name:{TEXT("ALL"),TEXT("WEAPONS"),TEXT("GEAR"),TEXT("MEDICAL"),TEXT("SALVAGE")})
        {
            const bool Active=Filter==Name;
            Filters->AddSlot().FillWidth(1).Padding(0,0,4,0)
                [SNew(SBox).HeightOverride(32)[SNew(SDepotPlate).Active(Active).Marks(false).Corner(4).Padding(FMargin(6,7))
                    .OnClicked_Lambda([this,Name](){Filter=Name;Refresh();return FReply::Handled();})
                    [SNew(STextBlock).Text(FText::FromString(Name)).Font(DepotStyle::Font(11,true)).ColorAndOpacity(Active?DepotStyle::ActionText:DepotStyle::Muted).Justification(ETextJustify::Center)]]];
        }
        Stash->AddSlot().AutoHeight()[Filters];
        Stash->AddSlot().AutoHeight().Padding(0,14,0,0).HAlign(HAlign_Center)[InventoryGrid()];
        Stash->AddSlot().FillHeight(1)[SNew(SSpacer)];
        Stash->AddSlot().AutoHeight().Padding(0,15,0,0)[DepotStyle::Rule()];
        Stash->AddSlot().AutoHeight().Padding(0,12,0,0)[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1)[DepotStyle::Text(TEXT("SELECT TO INSPECT / EQUIP / USE"),10,DepotStyle::Muted)]
            +SHorizontalBox::Slot().AutoWidth()[DepotStyle::Text(TEXT("SCROLL / 8 COLUMNS"),10,DepotStyle::Muted)]];
        TSharedRef<SOverlay> Layout=SNew(SOverlay);
        Layout->AddSlot().HAlign(HAlign_Left).Padding(48,16,0,20)[SNew(SBox).WidthOverride(300)[SNew(SDepotPlate).Padding(22)[Left]]];
        Layout->AddSlot().HAlign(HAlign_Right).Padding(0,16,48,20)[SNew(SBox).WidthOverride(568)[SNew(SDepotPlate).Padding(24)[Stash]]];
        if(!Selected.IsNone())Layout->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Center)[Inspect()];
        return Layout;
    }

    TSharedRef<SWidget> Inspect()
    {
        const FDepotItem* Item=Owner->FindItem(Selected);if(!Item)return SNew(SSpacer);
        const bool InStash=Owner->Inventory()->Stash.Contains(Selected);
        TSharedRef<SVerticalBox> V=SNew(SVerticalBox);
        V->AddSlot().AutoHeight()[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[DepotStyle::Text(Item->Category,11,DepotStyle::Accent,true)]
            +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(26).HeightOverride(26)
                [SNew(SDepotPlate).Marks(false).Padding(4).Corner(3).OnClicked_Lambda([this](){Selected=NAME_None;Refresh();return FReply::Handled();})
                    [SNew(STextBlock).Text(FText::FromString(TEXT("X"))).Font(DepotStyle::Heading(11)).ColorAndOpacity(DepotStyle::Muted).Justification(ETextJustify::Center)]]]];
        V->AddSlot().AutoHeight().Padding(0,18)[Icon(Item->Id,350,145)];
        V->AddSlot().AutoHeight()[DepotStyle::Text(Item->Name,20,DepotStyle::Cream,true)];
        V->AddSlot().AutoHeight().Padding(0,14)[SNew(STextBlock).Text(FText::FromString(Item->Description)).Font(DepotStyle::Font(12)).ColorAndOpacity(DepotStyle::Muted).AutoWrapText(true)];
        V->AddSlot().AutoHeight().Padding(0,6,0,20)[DepotStyle::Text(Item->bPackWeapon?FString::Printf(TEXT("QUANTITY %d"),Owner->Inventory()->Quantities.FindRef(Item->Id)):FString::Printf(TEXT("%.1f KG   /   QUANTITY %d"),Item->Weight,Owner->Inventory()->Quantities.FindRef(Item->Id)),11,DepotStyle::Muted)];
        V->AddSlot().AutoHeight()[Button(InStash?(Item->bFirearm?TEXT("EQUIP PRIMARY"):TEXT("ADD TO FIELD KIT")):TEXT("RETURN TO STASH"),[this,Id=Item->Id,InStash](){Selected=NAME_None;if(InStash)Owner->EquipItem(Id);else Owner->ReturnItem(Id);return FReply::Handled();},true)];
        if(Item->bPackWeapon)V->AddSlot().AutoHeight().Padding(0,8,0,0)[Button(TEXT("CUSTOMIZE FINISH"),[this,Id=Item->Id](){SetPage(TEXT("ARMORY"));SetArmoryWeapon(Id);Refresh();return FReply::Handled();})];
        if(Item->bMedical)V->AddSlot().AutoHeight().Padding(0,8,0,0)[Button(TEXT("USE / RESTORE HEALTH"),[this,Id=Item->Id](){Selected=NAME_None;Owner->UseItem(Id);return FReply::Handled();})];
        return SNew(SBox).WidthOverride(402)[SNew(SDepotPlate).Fill(DepotStyle::Ink.CopyWithNewOpacity(1)).Padding(26)[V]];
    }
    TSharedRef<SWidget> Framed(TSharedRef<SWidget> Content,float Width=640)
    {return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SBox).WidthOverride(Width)[SNew(SDepotPlate).Padding(30)[Content]]];}
    TSharedRef<SWidget> PageTitle(const FString& Name,const FString& Kind,const FString& Eyebrow)
    {
        return SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,17,0)[SNew(SDepotSymbol).Kind(Kind).Tint(DepotStyle::Cream).Size(36)]
            +SHorizontalBox::Slot().FillWidth(1)[SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[DepotStyle::Text(Eyebrow,10,DepotStyle::Accent,true)]
                +SVerticalBox::Slot().AutoHeight().Padding(0,5,0,0)[DepotStyle::Text(Name,24,DepotStyle::Cream,true)]];
    }
    TSharedRef<SWidget> Operators()
    {
        UBackstageDepotInventorySubsystem* State=Owner->GetInventorySubsystem();
        if(!State)return SNew(SSpacer);
        const TArray<FDepotOperator> Catalog=State->GetOperators();
        if(Catalog.IsEmpty())return Framed(SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight()[PageTitle(TEXT("OPERATOR SELECTION"),TEXT("operator"),TEXT("CREW ROSTER"))]
            +SVerticalBox::Slot().AutoHeight().Padding(0,24)[DepotStyle::Rule()]
            +SVerticalBox::Slot().AutoHeight()[DepotStyle::Text(TEXT("CHARACTER ROSTER BEING UPDATED"),18,DepotStyle::Cream,true)]
            +SVerticalBox::Slot().AutoHeight().Padding(0,12,0,28)[Paragraph(TEXT("New operators are being selected for BorderTown"),14)]
            +SVerticalBox::Slot().AutoHeight()[Button(TEXT("RETURN TO DEPOT"),[this](){SetPage(TEXT("HUB"));return FReply::Handled();})],640);
        const FDepotOperator* Preview=Catalog.FindByPredicate([this](const FDepotOperator& Entry){return Entry.Id==PreviewOperatorId;});
        if(!Preview)return SNew(SSpacer);
        TSharedRef<SVerticalBox> Roster=SNew(SVerticalBox);
        Roster->AddSlot().AutoHeight()[PageTitle(TEXT("OPERATORS"),TEXT("operator"),TEXT("CREW ROSTER"))];
        Roster->AddSlot().AutoHeight().Padding(0,16,0,20)[Paragraph(TEXT("Choose your field operator"))];
        for(int32 Index=0;Index<Catalog.Num();++Index)
        {
            const FDepotOperator& Entry=Catalog[Index];
            const bool bPreview=Entry.Id==PreviewOperatorId;
            const bool bEquipped=Entry.Id==State->GetSelectedOperator();
            const FLinearColor TextColor=bPreview?DepotStyle::ActionText:DepotStyle::Cream;
            Roster->AddSlot().AutoHeight().Padding(0,0,0,10)[SNew(SBox).HeightOverride(90)
                [SNew(SDepotPlate).Active(bPreview).Marks(bPreview).Padding(FMargin(17,14))
                    .OnClicked_Lambda([this,Id=Entry.Id](){if(Owner->PreviewOperator(Id))PreviewOperatorId=Id;Refresh();return FReply::Handled();})
                    [SNew(SHorizontalBox)
                        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,14,0)[DepotStyle::Text(FString::Printf(TEXT("%02d"),Index+1),24,bPreview?DepotStyle::ActionText:DepotStyle::Accent,true)]
                        +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(SVerticalBox)
                            +SVerticalBox::Slot().AutoHeight()[DepotStyle::Text(Entry.DisplayName.ToUpper(),19,TextColor,true)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,6,0,0)[DepotStyle::Text(bEquipped?TEXT("ACTIVE OPERATOR"):(bPreview?TEXT("PREVIEWING"):TEXT("VIEW OPERATOR")),10,bPreview?DepotStyle::ActionText:DepotStyle::Muted)]]]]];
        }
        Roster->AddSlot().AutoHeight().Padding(0,18,0,0)[Button(TEXT("RETURN TO DEPOT"),[this](){SetPage(TEXT("HUB"));return FReply::Handled();})];
        const bool bEquipped=Preview->Id==State->GetSelectedOperator();
        TSharedRef<SVerticalBox> Detail=SNew(SVerticalBox);
        Detail->AddSlot().AutoHeight()[DepotStyle::Text(TEXT("OPERATOR APPEARANCE"),11,DepotStyle::Accent,true)];
        Detail->AddSlot().AutoHeight().Padding(0,12,0,8)[DepotStyle::Text(Preview->DisplayName.ToUpper(),32,DepotStyle::Cream,true)];
        Detail->AddSlot().AutoHeight()[Paragraph(Preview->Description,14)];
        Detail->AddSlot().AutoHeight().Padding(0,24)[DepotStyle::Rule()];
        Detail->AddSlot().AutoHeight()[DepotStyle::Text(TEXT("APPEARANCE / UNIFORM"),11,DepotStyle::Accent,true)];
        Detail->AddSlot().AutoHeight().Padding(0,9,0,22)[Paragraph(TEXT("Your equipment and field kit stay with you"))];
        Detail->AddSlot().AutoHeight()[Button(bEquipped?TEXT("ACTIVE OPERATOR"):TEXT("USE OPERATOR"),[this](){Owner->SelectOperator(PreviewOperatorId);return FReply::Handled();},!bEquipped)];
        Detail->AddSlot().AutoHeight().Padding(0,12,0,0)[Paragraph(bEquipped?TEXT("Saved to your crew profile"):TEXT("Preview only / Select Use Operator to save this choice"),11)];
        return SNew(SOverlay)
            +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(48,24,0,0)[SNew(SBox).WidthOverride(332)[SNew(SDepotPlate).Padding(22)[Roster]]]
            +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0,0,48,30)[SNew(SBox).WidthOverride(338)[SNew(SDepotPlate).Padding(26)[Detail]]]
            +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0,0,0,22)
                [SNew(SDepotPlate).Marks(false).Padding(FMargin(18,10))[DepotStyle::Text(TEXT("<  >   ROTATE OPERATOR"),11,DepotStyle::Muted)]];
    }
    TSharedRef<SWidget> PreviewImage(UTexture* Texture,FName Key,float Width,float Height,FLinearColor Tint=FLinearColor::White)
    {
        if(!Texture)return SNew(SBox).WidthOverride(Width).HeightOverride(Height);
        TSharedPtr<FSlateBrush>& Brush=Brushes.FindOrAdd(Key);
        if(!Brush.IsValid())Brush=MakeShared<FSlateBrush>();
        Brush->SetResourceObject(Texture);Brush->ImageSize=FVector2D(Width,Height);Brush->DrawAs=ESlateBrushDrawType::Image;
        if(UTexture2D* StaticTexture=Cast<UTexture2D>(Texture)){const FIntPoint Size=StaticTexture->GetImportedSize();Brush->ImageSize=FVector2D(Size.X,Size.Y);}
        else if(UTextureRenderTarget2D* Target=Cast<UTextureRenderTarget2D>(Texture))Brush->ImageSize=FVector2D(Target->SizeX,Target->SizeY);
        return SNew(SBox).WidthOverride(Width).HeightOverride(Height).Clipping(EWidgetClipping::ClipToBounds)
            [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SImage).Image(Brush.Get()).ColorAndOpacity(Tint)]];
    }
    TSharedRef<SWidget> StockFinishIcon(const FDepotWeaponFinish& Finish,float Width,float Height)
    {
        UTexture2D* Texture=Owner->AssetIcon(Finish.IconPath);
        if(!Texture)return SNew(SBox).WidthOverride(Width).HeightOverride(Height).HAlign(HAlign_Center).VAlign(VAlign_Center)
            [DepotStyle::Text(Finish.DisplayName.ToUpper(),11,DepotStyle::Muted,true)];
        return PreviewImage(Texture,FName(*Finish.IconPath),Width,Height);
    }
    TSharedRef<SWidget> Armory()
    {
        UBackstageDepotInventorySubsystem* State=Owner->GetInventorySubsystem();
        if(!State)return SNew(SSpacer);
        const FDepotItem* Weapon=Owner->FindItem(ArmoryWeapon);
        const TArray<FDepotWeaponFinish> Finishes=State->GetWeaponFinishesForWeapon(ArmoryWeapon);
        if(!Weapon||Finishes.IsEmpty())return Framed(SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight()[PageTitle(TEXT("ARMORY"),TEXT("workbench"),TEXT("FPS CONTROLLER 2 / WEAPON FINISHES"))]
            +SVerticalBox::Slot().AutoHeight().Padding(0,24)[DepotStyle::Rule()]
            +SVerticalBox::Slot().AutoHeight()[DepotStyle::Text(TEXT("CONNECTING WEAPON CATALOG"),18,DepotStyle::Cream,true)]
            +SVerticalBox::Slot().AutoHeight().Padding(0,12,0,28)[Paragraph(TEXT("FPS Multiplayer Controller 2 source found / Weapon and finish integration in progress"),14)]
            +SVerticalBox::Slot().AutoHeight()[Button(TEXT("BACK TO LOADOUT"),[this](){SetPage(TEXT("LOADOUT"));return FReply::Handled();})],680);
        const FDepotWeaponFinish* Preview=Finishes.FindByPredicate([this](const FDepotWeaponFinish& Entry){return Entry.Id==PreviewFinishId;});
        if(!Preview){Preview=&Finishes[0];PreviewFinishId=Preview->Id;}
        const FName SavedFinish=Owner->GetWeaponFinish(ArmoryWeapon);
        const bool bOwned=Owner->GetItemQuantity(ArmoryWeapon)>0;
        const bool bEquipped=bOwned&&SavedFinish==PreviewFinishId;
        TSharedRef<SVerticalBox> WeaponList=SNew(SVerticalBox);
        WeaponList->AddSlot().AutoHeight()[PageTitle(TEXT("ARMORY"),TEXT("workbench"),TEXT("STOCK WEAPON COLLECTION"))];
        WeaponList->AddSlot().AutoHeight().Padding(0,12,0,15)[DepotStyle::Rule()];
        for(const FDepotItem& Entry:Owner->Items())
        {
            if(!Entry.bPackWeapon||!State->IsPackWeaponAvailable(Entry.Id))continue;
            const bool bCurrent=Entry.Id==ArmoryWeapon;
            WeaponList->AddSlot().AutoHeight().Padding(0,0,0,6)[SNew(SBox).HeightOverride(85)
                [SNew(SDepotPlate).Active(bCurrent).Marks(bCurrent).Padding(FMargin(13,9))
                    .OnClicked_Lambda([this,Id=Entry.Id](){SetArmoryWeapon(Id);Refresh();return FReply::Handled();})
                    [SNew(SVerticalBox)
                        +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[PreviewImage(Owner->AssetIcon(Entry.IconPath),FName(*Entry.IconPath),190,43,bCurrent?DepotStyle::ActionText:DepotStyle::Cream)]
                        +SVerticalBox::Slot().AutoHeight().Padding(0,5,0,0)[SNew(SHorizontalBox)
                            +SHorizontalBox::Slot().FillWidth(1)[DepotStyle::Text(Entry.Name,13,bCurrent?DepotStyle::ActionText:DepotStyle::Cream,true)]
                            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[DepotStyle::Text(Owner->GetItemQuantity(Entry.Id)>0?TEXT("OWNED"):TEXT("PREVIEW"),9,bCurrent?DepotStyle::ActionText:DepotStyle::Muted)]]]]];
        }
        if(!State->HasPackStarterKit())WeaponList->AddSlot().AutoHeight().Padding(0,7,0,0)
            [SNew(SBox).HeightOverride(48)[SNew(SDepotPlate).Primary(true).Padding(FMargin(12,10))
                .OnClicked_Lambda([this](){Owner->AcquirePackStarterKit();SetArmoryWeapon(ArmoryWeapon);Refresh();return FReply::Handled();})
                [SNew(STextBlock).Text(FText::FromString(TEXT("ADD STARTER KIT"))).Font(DepotStyle::Heading(12)).ColorAndOpacity(DepotStyle::ActionText).Justification(ETextJustify::Center)]]];
        WeaponList->AddSlot().AutoHeight().Padding(0,8,0,0)[Button(TEXT("BACK TO LOADOUT"),[this](){SetPage(TEXT("LOADOUT"));return FReply::Handled();},false,0,38)];
        TSharedRef<SVerticalBox> WorkArea=SNew(SVerticalBox);
        WorkArea->AddSlot().AutoHeight()[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1)[SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight()[DepotStyle::Text(TEXT("WEAPON COLLECTION"),10,DepotStyle::Accent,true)]
                +SVerticalBox::Slot().AutoHeight().Padding(0,7,0,0)[DepotStyle::Text(Weapon->Name,27,DepotStyle::Cream,true)]]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[DepotStyle::Text(TEXT("INSPECT WEAPON"),10,DepotStyle::Muted)]];
        WorkArea->AddSlot().AutoHeight().Padding(0,15,0,14)[DepotStyle::Rule()];
        TSharedRef<SWidget> LivePreview=bWeaponPreviewReady&&Owner->PackWeaponPreviewTexture()
            ?StaticCastSharedRef<SWidget>(SNew(SDepotWeaponInspector).Owner(Owner.Get())
                [PreviewImage(Owner->PackWeaponPreviewTexture(),TEXT("LivePackWeapon"),665,300)])
            :StaticCastSharedRef<SWidget>(SNew(SBox).WidthOverride(665).HeightOverride(300).HAlign(HAlign_Center).VAlign(VAlign_Center)
                [DepotStyle::Text(TEXT("WEAPON PREVIEW COULD NOT LOAD"),13,DepotStyle::Muted,true)]);
        TSharedRef<SVerticalBox> Detail=SNew(SVerticalBox);
        Detail->AddSlot().AutoHeight()[DepotStyle::Text(TEXT("SELECTED FINISH"),10,DepotStyle::Accent,true)];
        Detail->AddSlot().AutoHeight().Padding(0,9,0,9)[DepotStyle::Text(Preview->DisplayName.ToUpper(),23,DepotStyle::Cream,true)];
        Detail->AddSlot().AutoHeight()[Paragraph(TEXT("WEAPON FINISH"),13)];
        if(bOwned)
        {
            Detail->AddSlot().AutoHeight().Padding(0,20,0,8)[Button(bEquipped?TEXT("FINISH SELECTED"):TEXT("APPLY FINISH"),[this](){Owner->SelectWeaponFinish(ArmoryWeapon,PreviewFinishId);return FReply::Handled();},!bEquipped)];
            Detail->AddSlot().AutoHeight()[Paragraph(bEquipped?TEXT("Selected for this weapon"):TEXT("Preview only / Apply to save this finish"),11)];
            if(Owner->GetEquippedPrimary()!=ArmoryWeapon)Detail->AddSlot().AutoHeight().Padding(0,10,0,0)
                [Button(TEXT("EQUIP PRIMARY"),[this](){Owner->EquipItem(ArmoryWeapon);return FReply::Handled();})];
        }
        else Detail->AddSlot().AutoHeight().Padding(0,20,0,0)[Paragraph(TEXT("Add the starter kit to equip this weapon and save its finish"),12)];
        WorkArea->AddSlot().AutoHeight()[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1).HAlign(HAlign_Center).VAlign(VAlign_Center)[LivePreview]
            +SHorizontalBox::Slot().AutoWidth().Padding(18,12,0,0)[SNew(SBox).WidthOverride(265)[Detail]]];
        WorkArea->AddSlot().AutoHeight().Padding(0,16,0,12)[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1)[DepotStyle::Text(FString::Printf(TEXT("FINISHES  /  %02d"),Finishes.Num()),11,DepotStyle::Muted,true)]
            +SHorizontalBox::Slot().AutoWidth()[DepotStyle::Text(TEXT("DRAG TO ROTATE  /  <  >"),10,DepotStyle::Muted)]];
        TSharedRef<SScrollBox> Cards=SNew(SScrollBox).Orientation(Orient_Horizontal);
        for(const FDepotWeaponFinish& Finish:Finishes)
        {
            const bool bPreview=Finish.Id==PreviewFinishId;
            const bool bSaved=bOwned&&Finish.Id==SavedFinish;
            Cards->AddSlot().Padding(0,0,9,0)[SNew(SBox).WidthOverride(184).HeightOverride(137)
                [SNew(SDepotPlate).Active(bPreview).Marks(bPreview).Padding(12)
                    .OnClicked_Lambda([this,Id=Finish.Id](){PreviewFinishId=Id;bWeaponPreviewReady=Owner->SetPackWeaponPreview(ArmoryWeapon,Id);Refresh();return FReply::Handled();})
                    [SNew(SVerticalBox)
                        +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[StockFinishIcon(Finish,154,67)]
                        +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,5)[DepotStyle::Text(Finish.DisplayName.ToUpper(),13,bPreview?DepotStyle::ActionText:DepotStyle::Cream,true)]
                        +SVerticalBox::Slot().AutoHeight()[DepotStyle::Text(bSaved?TEXT("SELECTED"):(bPreview?TEXT("PREVIEWING"):TEXT("PREVIEW")),9,bPreview?DepotStyle::ActionText:DepotStyle::Muted)]]]];
        }
        WorkArea->AddSlot().AutoHeight()[SNew(SBox).HeightOverride(153)[Cards]];
        WorkArea->AddSlot().AutoHeight().Padding(0,13,0,0)[Paragraph(TEXT("Customize your loadout / Deployment currently unavailable"),11)];
        return SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().Padding(48,20,18,20)[SNew(SBox).WidthOverride(290)[SNew(SDepotPlate).Padding(20)[WeaponList]]]
            +SHorizontalBox::Slot().FillWidth(1).Padding(0,20,48,20)[SNew(SDepotPlate).Padding(24)[WorkArea]];
    }
    TSharedRef<SWidget> Workshop()
    {
        TSharedRef<SVerticalBox> V=SNew(SVerticalBox);
        V->AddSlot().AutoHeight()[PageTitle(TEXT("WORKBENCH"),TEXT("workbench"),TEXT("BAY 03 / LOADOUT PRESETS"))];
        if(const FDepotItem* P=Owner->FindItem(Owner->Inventory()->Primary))
        {V->AddSlot().AutoHeight().Padding(0,16,0,6)[Icon(P->Id,580,125)];V->AddSlot().AutoHeight()[DepotStyle::Text(P->Name,18,DepotStyle::Cream,true)];}
        V->AddSlot().AutoHeight().Padding(0,17,0,14)[DepotStyle::Rule()];
        for(const FName Name:{FName(TEXT("Balanced")),FName(TEXT("Close Quarters")),FName(TEXT("Scout"))})
        {
            const bool Active=Owner->Inventory()->BuildPreset==Name;
            V->AddSlot().AutoHeight().Padding(0,0,0,8)[SNew(SBox).HeightOverride(50)
                [SNew(SDepotPlate).Active(Active).Marks(Active).Padding(FMargin(18,12)).OnClicked_Lambda([this,Name](){Owner->SelectPreset(Name);return FReply::Handled();})
                    [SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[DepotStyle::Text(Name.ToString().ToUpper(),14,Active?DepotStyle::ActionText:DepotStyle::Cream,true)]
                        +SHorizontalBox::Slot().AutoWidth()[DepotStyle::Text(Active?TEXT("SELECTED"):TEXT("SELECT"),10,Active?DepotStyle::ActionText:DepotStyle::Muted)]]]];
        }
        V->AddSlot().AutoHeight().Padding(0,9,0,18)[SNew(STextBlock).Text(FText::FromString(TEXT("Presets save your preparation choice / Weapon tuning is a configuration preview"))).Font(DepotStyle::Font(12)).ColorAndOpacity(DepotStyle::Muted).AutoWrapText(true)];
        V->AddSlot().AutoHeight().Padding(0,0,0,8)[Button(TEXT("WEAPON FINISHES"),[this](){SetPage(TEXT("ARMORY"));return FReply::Handled();},true)];
        V->AddSlot().AutoHeight()[Button(TEXT("RETURN TO DEPOT"),[this](){SetPage(TEXT("HUB"));return FReply::Handled();})];return Framed(V);
    }
    TSharedRef<SWidget> Settings()
    {
        TSharedRef<SVerticalBox> V=SNew(SVerticalBox);V->AddSlot().AutoHeight()[PageTitle(TEXT("SETTINGS"),TEXT("settings"),TEXT("CREW TERMINAL / PREFERENCES"))];
        V->AddSlot().AutoHeight().Padding(0,24,0,8)[Button(Owner->Inventory()->bReducedMotion?TEXT("BACKGROUND MOTION / OFF"):TEXT("BACKGROUND MOTION / ON"),[this](){Owner->ToggleMotion();return FReply::Handled();})];
        V->AddSlot().AutoHeight().Padding(0,0,0,8)[Button(TEXT("WINDOWED / 1600 x 900"),[this](){Owner->ConsoleCommand(TEXT("r.SetRes 1600x900w"));return FReply::Handled();})];
        V->AddSlot().AutoHeight()[Button(TEXT("FULLSCREEN"),[this](){Owner->ConsoleCommand(TEXT("r.SetRes 1920x1080f"));return FReply::Handled();})];
        V->AddSlot().AutoHeight().Padding(0,20)[DepotStyle::Rule()];
        V->AddSlot().AutoHeight()[DepotStyle::Text(TEXT("Inventory and preferences are saved on this computer"),12,DepotStyle::Muted)];
        V->AddSlot().AutoHeight().Padding(0,18,0,8)[Button(TEXT("RESTORE STARTER INVENTORY"),[this](){Owner->ResetInventory();return FReply::Handled();})];
        V->AddSlot().AutoHeight()[Button(TEXT("RETURN TO DEPOT"),[this](){SetPage(TEXT("HUB"));return FReply::Handled();})];return Framed(V,620);
    }
    TSharedRef<SWidget> Contracts()
    {
        return Framed(SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[PageTitle(TEXT("CONTRACT BOARD"),TEXT("contracts"),TEXT("CREW TERMINAL / ASSIGNMENTS"))]
            +SVerticalBox::Slot().AutoHeight().Padding(0,25)[DepotStyle::Rule()]
            +SVerticalBox::Slot().AutoHeight()[DepotStyle::Text(TEXT("NO ACTIVE CONTRACTS"),16,DepotStyle::Cream,true)]
            +SVerticalBox::Slot().AutoHeight().Padding(0,12,0,26)[DepotStyle::Text(TEXT("Contract assignments are not connected in this build"),12,DepotStyle::Muted)]
            +SVerticalBox::Slot().AutoHeight()[Button(TEXT("RETURN TO DEPOT"),[this](){SetPage(TEXT("HUB"));return FReply::Handled();})]);
    }
    TSharedRef<SWidget> Raid()
    {
        UBackstageDepotInventorySubsystem* State=Owner->GetInventorySubsystem();
        if(!State)return SNew(SSpacer);
        const FDepotItem* Primary=Owner->FindItem(Owner->Inventory()->Primary);
        TSharedRef<SVerticalBox> V=SNew(SVerticalBox);
        V->AddSlot().AutoHeight()[PageTitle(TEXT("SELECT DESTINATION"),TEXT("deployment"),TEXT("DEPLOYMENT / LOCAL PLAYTEST"))];
        V->AddSlot().AutoHeight().Padding(0,18,0,15)[DepotStyle::Rule()];
        for(const FDepotDestination& Map:State->GetDestinations())
        {
            const bool Active=State->GetSelectedDestination()==Map.Id;
            V->AddSlot().AutoHeight().Padding(0,0,0,10)
                [SNew(SDepotPlate).Active(Active).Marks(Active).Padding(FMargin(18,13))
                    .OnClicked_Lambda([this,Id=Map.Id](){Owner->SelectDestination(Id);return FReply::Handled();})
                    [SNew(SVerticalBox)
                        +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
                            +SHorizontalBox::Slot().FillWidth(1)[DepotStyle::Text(Map.DisplayName,18,Active?DepotStyle::ActionText:DepotStyle::Cream,true)]
                            +SHorizontalBox::Slot().AutoWidth()[DepotStyle::Text(Map.bMapInstalled?TEXT("INSTALLED"):TEXT("NOT INSTALLED"),10,Active?DepotStyle::ActionText:DepotStyle::Muted)]]
                        +SVerticalBox::Slot().AutoHeight().Padding(0,6,0,0)[DepotStyle::Text(Map.Description,11,Active?DepotStyle::ActionText:DepotStyle::Muted)]]];
        }
        V->AddSlot().AutoHeight().Padding(0,10,0,15)[DepotStyle::Rule()];
        if(State->GetSelectedDestination()==TEXT("Mexico"))
        {
            V->AddSlot().AutoHeight()[DepotStyle::Text(TEXT("LOCAL PLAYTEST"),13,DepotStyle::Accent,true)];
            V->AddSlot().AutoHeight().Padding(0,8,0,18)[Paragraph(TEXT("Explore the village / Choose your weapons in the map's loadout screen"),12)];
            V->AddSlot().AutoHeight()[Button(TEXT("EXPLORE MAP"),[this](){Owner->ExploreMexicoMap();return FReply::Handled();},true)];
        }
        else
        {
            V->AddSlot().AutoHeight()[DepotStyle::Text(TEXT("PRIMARY  /  ")+(Primary?Primary->Name:TEXT("EMPTY")),13,DepotStyle::Cream,true)];
            V->AddSlot().AutoHeight().Padding(0,7,0,0)[DepotStyle::Text(FString::Printf(TEXT("FIELD KIT  /  %d ITEMS"),Owner->Inventory()->FieldKit.Num()),12,DepotStyle::Muted)];
            FString Reason;const bool Ready=State->CanDeploy(State->GetSelectedDestination(),Reason);
            V->AddSlot().AutoHeight().Padding(0,18)[SNew(STextBlock).Text(FText::FromString(Reason)).Font(DepotStyle::Font(12)).ColorAndOpacity(Ready?DepotStyle::Accent:DepotStyle::Muted).AutoWrapText(true)];
            if(Ready)V->AddSlot().AutoHeight()[Button(TEXT("DEPLOY"),[this](){Owner->DeploySelectedMap();return FReply::Handled();},true)];
            else V->AddSlot().AutoHeight()[Button(TEXT("CHECK DEPLOYMENT"),[this](){Owner->DeploySelectedMap();return FReply::Handled();})];
            V->AddSlot().AutoHeight().Padding(0,8,0,0)[Button(TEXT("EDIT LOADOUT"),[this](){SetPage(TEXT("LOADOUT"));return FReply::Handled();})];
        }
        V->AddSlot().AutoHeight().Padding(0,8,0,0)[Button(TEXT("RETURN TO DEPOT"),[this](){SetPage(TEXT("HUB"));return FReply::Handled();})];
        return Framed(V,700);
    }

};

ABackstageDepotGameMode::ABackstageDepotGameMode()
{
    PlayerControllerClass=ABackstageDepotPlayerController::StaticClass();
    DefaultPawnClass=nullptr;HUDClass=nullptr;
}
ABackstageDepotPlayerController::ABackstageDepotPlayerController()
{
    PrimaryActorTick.bCanEverTick=true;bShowMouseCursor=true;bEnableClickEvents=true;bEnableMouseOverEvents=true;
}
void ABackstageDepotPlayerController::BeginPlay()
{
    Super::BeginPlay();if(!IsLocalController())return;
    InventoryState=GetGameInstance()?GetGameInstance()->GetSubsystem<UBackstageDepotInventorySubsystem>():nullptr;
    if(!InventoryState || !InventoryState->Inventory())
    {
        UE_LOG(LogTemp,Error,TEXT("Depot inventory subsystem is unavailable"));
        return;
    }
    InventoryState->CancelPendingDeployment();
    Feedback=InventoryState->GetStatus();
    for(const FDepotItem& Item:Items())
    {
        if(Item.bPackWeapon){if(UTexture2D* T=AssetIcon(Item.IconPath))Icons.Add(Item.Id,T);continue;}
        const FString Path=FString::Printf(TEXT("/Game/BackstageDepot/UI/Icons/T_%s.T_%s"),*Item.Id.ToString(),*Item.Id.ToString());
        if(UTexture2D* T=LoadObject<UTexture2D>(nullptr,*Path))Icons.Add(Item.Id,T);
    }
    if(UTexture2D* Destination=LoadObject<UTexture2D>(nullptr,TEXT("/Game/BackstageDepot/UI/Icons/T_carnival_card.T_carnival_card")))Icons.Add(TEXT("destination"),Destination);
    if(UTexture2D* Town=LoadObject<UTexture2D>(nullptr,TEXT("/Game/BorderTown/UI/T_MenuBackground.T_MenuBackground")))Icons.Add(TEXT("town_destination"),Town);
    for(TActorIterator<AActor> It(GetWorld());It;++It)
    {
        if(It->ActorHasTag(TEXT("HubFan")))Fans.Add(*It);
        if(It->ActorHasTag(TEXT("HubWeaponDisplay")))
        {
            TInlineComponentArray<USkeletalMeshComponent*> DisplayParts(*It);
            for(USkeletalMeshComponent* Part:DisplayParts)
            {
                TArray<FName> Bones;Part->GetBoneNames(Bones);
                for(const FName Bone:Bones)
                {
                    const FString Name=Bone.ToString().ToLower();
                    if(Name.Contains(TEXT("bullet"))||Name.Contains(TEXT("shell"))||Name==TEXT("mag_02"))
                        Part->HideBoneByName(Bone,EPhysBodyOp::PBO_None);
                }
                Part->SetForcedLOD(1);Part->PrestreamTextures(30.f,false);
            }
        }
        if(It->ActorHasTag(TEXT("HubOperator")))
        {
            OperatorActor=*It;It->SetActorHiddenInGame(true);It->SetActorEnableCollision(false);
        }
    }
    ApplyMotionPreference();
    FocusHubCamera();GetWorldTimerManager().SetTimerForNextTick(this,&ABackstageDepotPlayerController::FocusHubCamera);
    if(GEngine&&GEngine->GameViewport){SAssignNew(Screen,SBackstageDepotScreen).Owner(this);ViewportWidget=Screen;GEngine->GameViewport->AddViewportWidgetContent(ViewportWidget.ToSharedRef(),10);}
    FInputModeGameAndUI Mode;Mode.SetHideCursorDuringCapture(false);Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);SetInputMode(Mode);bShowMouseCursor=true;
}
void ABackstageDepotPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
    // Shared inventory is saved by its subsystem, never by a departing controller.
    if(GEngine&&GEngine->GameViewport&&ViewportWidget.IsValid())GEngine->GameViewport->RemoveViewportWidgetContent(ViewportWidget.ToSharedRef());
    Screen.Reset();ViewportWidget.Reset();if(PackPreviewActor)PackPreviewActor->Destroy();PackPreviewActor=nullptr;InventoryState=nullptr;Super::EndPlay(Reason);
}
void ABackstageDepotPlayerController::FocusHubCamera()
{
    for(TActorIterator<ACameraActor> It(GetWorld());It;++It)if(It->ActorHasTag(TEXT("HubCamera"))){SetViewTargetWithBlend(*It,0.f);return;}
}
void ABackstageDepotPlayerController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);Runtime+=DeltaSeconds;if(!Inventory()||Inventory()->bReducedMotion)return;
    for(auto& Fan:Fans)if(Fan.IsValid())Fan->AddActorLocalRotation(FRotator(DeltaSeconds*38,0,0));
}
void ABackstageDepotPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();InputComponent->BindKey(EKeys::Escape,IE_Pressed,this,&ABackstageDepotPlayerController::EscapePressed);
    InputComponent->BindKey(EKeys::Tab,IE_Pressed,this,&ABackstageDepotPlayerController::LoadoutPressed);
    InputComponent->BindKey(EKeys::Left,IE_Pressed,this,&ABackstageDepotPlayerController::RotateLeft);
    InputComponent->BindKey(EKeys::Right,IE_Pressed,this,&ABackstageDepotPlayerController::RotateRight);
}
const TArray<FDepotItem>& ABackstageDepotPlayerController::Items()const
{
    static const TArray<FDepotItem> Empty;
    return InventoryState?InventoryState->Items():Empty;
}
UBackstageDepotSaveGame* ABackstageDepotPlayerController::Inventory()const{return InventoryState?InventoryState->Inventory():nullptr;}
const FDepotItem* ABackstageDepotPlayerController::FindItem(FName Id)const{return InventoryState?InventoryState->FindItem(Id):nullptr;}
UTexture2D* ABackstageDepotPlayerController::Icon(FName Id)const{const auto* Found=Icons.Find(Id);return Found?Found->Get():nullptr;}
UTexture2D* ABackstageDepotPlayerController::AssetIcon(const FString& Path)
{
    if(!Path.StartsWith(TEXT("/Game/FPS_Controller/")))return nullptr;
    const FName Key(*Path);if(const auto* Found=Icons.Find(Key))return Found->Get();
    if(!FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(Path)))return nullptr;
    UTexture2D* Texture=LoadObject<UTexture2D>(nullptr,*Path);
    if(Texture)Icons.Add(Key,Texture);
    return Texture;
}
UTexture* ABackstageDepotPlayerController::PackWeaponPreviewTexture()const
{
    return PackPreviewActor&&bPackPreviewActive?PackPreviewActor->GetPreviewTexture():nullptr;
}
bool ABackstageDepotPlayerController::SetPackWeaponPreview(FName WeaponId,FName FinishId)
{
    if(!InventoryState)return false;
    const TArray<FDepotWeaponFinish> Finishes=InventoryState->GetWeaponFinishesForWeapon(WeaponId);
    const FDepotWeaponFinish* Finish=Finishes.FindByPredicate([FinishId](const FDepotWeaponFinish& Entry){return Entry.Id==FinishId;});
    if(!Finish){HidePackWeaponPreview();return false;}
    if(!PackPreviewActor)PackPreviewActor=GetWorld()->SpawnActor<ADepotPackPreview>();
    bPackPreviewActive=PackPreviewActor&&PackPreviewActor->ConfigureWeapon(WeaponId,Finish->SkinDataAssetPath);
    if(PackPreviewActor)PackPreviewActor->SetPreviewActive(bPackPreviewActive);
    if(!bPackPreviewActive)Feedback=TEXT("WEAPON PREVIEW COULD NOT LOAD");
    return bPackPreviewActive;
}
void ABackstageDepotPlayerController::HidePackWeaponPreview()
{
    bPackPreviewActive=false;if(PackPreviewActor)PackPreviewActor->SetPreviewActive(false);
}
bool ABackstageDepotPlayerController::AcquirePackStarterKit()
{
    if(!InventoryState)return false;
    const bool bAcquired=InventoryState->AcquirePackStarterKit();Feedback=InventoryState->GetStatus();RefreshScreen();return bAcquired;
}
bool ABackstageDepotPlayerController::ApplyOperatorMesh(FName Id)
{
    if(!InventoryState||!OperatorActor.IsValid()){Feedback=TEXT("OPERATOR PREVIEW UNAVAILABLE");return false;}
    const TArray<FDepotOperator> Catalog=InventoryState->GetOperators();
    const FDepotOperator* Entry=Catalog.FindByPredicate([Id](const FDepotOperator& Candidate){return Candidate.Id==Id;});
    if(!Entry){Feedback=TEXT("UNKNOWN OPERATOR KIT");return false;}
    const FString Package=FPackageName::ObjectPathToPackageName(Entry->MeshPath);
    if(!FPackageName::DoesPackageExist(Package)){Feedback=TEXT("OPERATOR KIT IS NOT INSTALLED");return false;}
    USkeletalMesh* Mesh=LoadObject<USkeletalMesh>(nullptr,*Entry->MeshPath);
    USkeletalMeshComponent* Component=OperatorActor->FindComponentByClass<USkeletalMeshComponent>();
    if(!Mesh||!Component){Feedback=TEXT("OPERATOR PREVIEW COULD NOT LOAD");return false;}
    if(Component->GetSkeletalMeshAsset()==Mesh)return true;
    UAnimSingleNodeInstance* SingleNode=Component->GetSingleNodeInstance();
    UAnimationAsset* Idle=SingleNode?SingleNode->GetAnimationAsset():nullptr;
    const float Position=SingleNode?SingleNode->GetCurrentTime():0.f;
    // A validated pack mesh must also support the existing preview pose
    if(Idle&&Idle->GetSkeleton()!=Mesh->GetSkeleton())
    {
        Feedback=TEXT("OPERATOR KIT DOES NOT SUPPORT THIS PREVIEW POSE");return false;
    }
    Component->SetSkeletalMesh(Mesh,true);
    Component->EmptyOverrideMaterials();
    if(Idle){Component->PlayAnimation(Idle,true);Component->SetPosition(Position,false);}
    ApplyMotionPreference();
    return true;
}
bool ABackstageDepotPlayerController::PreviewOperator(FName Id)
{
    const bool bApplied=ApplyOperatorMesh(Id);
    if(bApplied)Feedback=Id==GetSelectedOperator()?TEXT("ACTIVE OPERATOR KIT"):TEXT("PREVIEW ONLY / SELECT USE OPERATOR TO SAVE");
    return bApplied;
}
bool ABackstageDepotPlayerController::SelectOperator(FName Id)
{
    if(!InventoryState)return false;
    if(!ApplyOperatorMesh(Id)){RefreshScreen();return false;}
    const bool bSaved=InventoryState->SelectOperator(Id);
    if(!bSaved)ApplyOperatorMesh(InventoryState->GetSelectedOperator());
    Feedback=InventoryState->GetStatus();RefreshScreen();return bSaved;
}
FName ABackstageDepotPlayerController::GetSelectedOperator()const{return InventoryState?InventoryState->GetSelectedOperator():NAME_None;}
bool ABackstageDepotPlayerController::SelectWeaponFinish(FName WeaponId,FName FinishId)
{
    if(!InventoryState)return false;
    const bool bSaved=InventoryState->SelectWeaponFinish(WeaponId,FinishId);
    Feedback=InventoryState->GetStatus();RefreshScreen();return bSaved;
}
FName ABackstageDepotPlayerController::GetWeaponFinish(FName WeaponId)const{return InventoryState?InventoryState->GetWeaponFinish(WeaponId):NAME_None;}
bool ABackstageDepotPlayerController::SaveInventory(){return InventoryState&&InventoryState->SaveInventory();}
void ABackstageDepotPlayerController::RefreshScreen(){if(Screen.IsValid())Screen->Refresh();}
void ABackstageDepotPlayerController::EquipItem(FName Id)
{
    if(!InventoryState)return;InventoryState->EquipItem(Id);Feedback=InventoryState->GetStatus();RefreshScreen();
}
void ABackstageDepotPlayerController::ReturnItem(FName Id)
{
    if(!InventoryState)return;InventoryState->ReturnItem(Id);Feedback=InventoryState->GetStatus();RefreshScreen();
}
void ABackstageDepotPlayerController::UseItem(FName Id)
{
    if(!InventoryState)return;InventoryState->UseItem(Id);Feedback=InventoryState->GetStatus();RefreshScreen();
}
void ABackstageDepotPlayerController::SelectPreset(FName Preset)
{
    if(!InventoryState)return;InventoryState->SelectPreset(Preset);Feedback=InventoryState->GetStatus();RefreshScreen();
}
void ABackstageDepotPlayerController::ApplyMotionPreference()
{
    if(!Inventory()||!OperatorActor.IsValid())return;
    TInlineComponentArray<USkeletalMeshComponent*> Components(OperatorActor.Get());
    for(USkeletalMeshComponent* Component:Components)Component->GlobalAnimRateScale=Inventory()->bReducedMotion?0.f:1.f;
}
void ABackstageDepotPlayerController::ToggleMotion()
{
    if(!InventoryState)return;InventoryState->ToggleMotion();ApplyMotionPreference();Feedback=InventoryState->GetStatus();RefreshScreen();
}
void ABackstageDepotPlayerController::ResetInventory()
{
    if(!InventoryState)return;InventoryState->ResetInventory();ApplyOperatorMesh(GetSelectedOperator());ApplyMotionPreference();Feedback=InventoryState->GetStatus();RefreshScreen();
}
void ABackstageDepotPlayerController::SelectDestination(FName Id)
{
    if(!InventoryState)return;InventoryState->SelectDestination(Id);Feedback=InventoryState->GetStatus();RefreshScreen();
}
bool ABackstageDepotPlayerController::DeploySelectedMap()
{
    if(!InventoryState)return false;
    const bool bStarting=InventoryState->TryDeploy(InventoryState->GetSelectedDestination());
    Feedback=bStarting?TEXT("DEPLOYING"):TEXT("DEPLOYMENT NOT READY");
    if(bStarting){bShowMouseCursor=false;SetInputMode(FInputModeGameOnly());}
    else RefreshScreen();
    return bStarting;
}
bool ABackstageDepotPlayerController::ExploreMexicoMap()
{
    if(!InventoryState || InventoryState->GetSelectedDestination()!=TEXT("Mexico"))return false;
    if(InventoryState->HasPendingDeployment())
    {Feedback=TEXT("DEPLOYMENT IS ALREADY STARTING");RefreshScreen();return false;}
    if(!GetWorld() || !GetWorld()->IsGameWorld() || !IsLocalController())return false;
    const TCHAR* MapPackage=TEXT("/Game/BorderTown/Maps/Mexico_Playtest");
    const TCHAR* GameModePath=TEXT("/Game/BorderTownWeapons/Arsenal/BP_ArsenalGameMode.BP_ArsenalGameMode_C");
    if(!FPackageName::DoesPackageExist(MapPackage))
    {Feedback=TEXT("MEXICO PLAYTEST IS NOT INSTALLED");RefreshScreen();return false;}
    if(!FSoftClassPath(GameModePath).TryLoadClass<AGameModeBase>())
    {Feedback=TEXT("MEXICO PLAYTEST GAME MODE IS UNAVAILABLE");RefreshScreen();return false;}
    // The native loadout screen handles this preview; no Depot inventory is issued or consumed.
    Feedback=TEXT("OPENING MEXICO / LOCAL PLAYTEST");
    bShowMouseCursor=false;SetInputMode(FInputModeGameOnly());
    UGameplayStatics::OpenLevel(this,FName(MapPackage),true,FString(TEXT("game="))+GameModePath);
    return true;
}
TArray<FName> ABackstageDepotPlayerController::GetStashItems()const{return Inventory()?Inventory()->Stash:TArray<FName>();}
TArray<FName> ABackstageDepotPlayerController::GetFieldKitItems()const{return Inventory()?Inventory()->FieldKit:TArray<FName>();}
FName ABackstageDepotPlayerController::GetEquippedPrimary()const{return Inventory()?Inventory()->Primary:NAME_None;}
int32 ABackstageDepotPlayerController::GetItemQuantity(FName Id)const{return Inventory()?Inventory()->Quantities.FindRef(Id):0;}
float ABackstageDepotPlayerController::GetOperatorHealth()const{return Inventory()?Inventory()->Health:0.f;}
void ABackstageDepotPlayerController::BackstagePanel(const FString& Name)
{
    const FString Value=Name.ToUpper();
    if(Value==TEXT("HUB")||Value==TEXT("LOADOUT")||Value==TEXT("WORKBENCH")||Value==TEXT("OPERATORS")||Value==TEXT("ARMORY")||Value==TEXT("CONTRACTS")||Value==TEXT("SETTINGS")||Value==TEXT("RAID"))if(Screen.IsValid())Screen->SetPage(Value);
}
void ABackstageDepotPlayerController::BackstageShot(const FString& FileName)
{
    const FString Output=FileName.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("Screenshots/BackstageDepot.png"):FileName;
    FTimerHandle CaptureTimer;
    GetWorldTimerManager().SetTimer(CaptureTimer,FTimerDelegate::CreateLambda([Output](){FScreenshotRequest::RequestScreenshot(Output,true,false,false,FIntRect(),true);}),12.f,false);
}
void ABackstageDepotPlayerController::EscapePressed(){if(Screen.IsValid())Screen->Escape();}
void ABackstageDepotPlayerController::BackstageCleanShot(const FString& FileName)
{
    const FString Output=FileName.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("Screenshots/BackstageDepot-world.png"):FileName;
    FTimerHandle CaptureTimer;
    GetWorldTimerManager().SetTimer(CaptureTimer,FTimerDelegate::CreateLambda([Output](){FScreenshotRequest::RequestScreenshot(Output,false,false,false,FIntRect(),true);}),12.f,false);
}
void ABackstageDepotPlayerController::LoadoutPressed(){if(Screen.IsValid())Screen->SetPage(TEXT("LOADOUT"));}
void ABackstageDepotPlayerController::RotateCosmeticPreview(float Degrees){if(bPackPreviewActive&&PackPreviewActor)PackPreviewActor->RotatePreview(Degrees);else if(OperatorActor.IsValid())OperatorActor->AddActorWorldRotation(FRotator(0,Degrees,0));}
void ABackstageDepotPlayerController::RotateLeft(){RotateCosmeticPreview(-15.f);}
void ABackstageDepotPlayerController::RotateRight(){RotateCosmeticPreview(15.f);}
