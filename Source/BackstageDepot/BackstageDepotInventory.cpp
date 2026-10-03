#include "BackstageDepotInventory.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
const TCHAR* ControllerGameMode = TEXT("/Game/FPS_Controller/Blueprints/Game/BP_GM.BP_GM_C");
bool IsSafeSlot(const FString& Name)
{
    if (Name.IsEmpty() || Name.Len()>96) return false;
    for (TCHAR Character:Name)
        if (!((Character>='a'&&Character<='z')||(Character>='A'&&Character<='Z')||(Character>='0'&&Character<='9')||Character=='_'||Character=='-')) return false;
    return true;
}
}

bool UBackstageDepotWeaponAdapter::ValidateLoadout_Implementation(const FDepotLoadout&, TSubclassOf<APawn>, FString& Reason) const
{
    Reason=TEXT("Weapon loadout integration is not ready");
    return false;
}
bool UBackstageDepotWeaponAdapter::ApplyLoadout_Implementation(APawn*, const FDepotLoadout&, const FGuid&, FString& Reason)
{
    Reason=TEXT("Weapon loadout integration is not ready");
    return false;
}

void UBackstageDepotInventorySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    FString SlotOverride;
#if !UE_BUILD_SHIPPING
    FParse::Value(FCommandLine::Get(),TEXT("DepotInventorySlot="),SlotOverride);
#endif
    InitializeInventory(SlotOverride);
    if (GEngine) TravelFailureHandle=GEngine->OnTravelFailure().AddUObject(this,&ThisClass::OnTravelFailure);
    if (!WeaponAdapterClass.IsNull())
        if (UClass* AdapterClass=WeaponAdapterClass.TryLoadClass<UBackstageDepotWeaponAdapter>()) SetWeaponAdapterClass(AdapterClass);
}

void UBackstageDepotInventorySubsystem::Deinitialize()
{
    if (GEngine) GEngine->OnTravelFailure().Remove(TravelFailureHandle);
    // Mutations save immediately. Teardown never writes an old copy over newer state.
    WeaponAdapter=nullptr;
    Super::Deinitialize();
}

void UBackstageDepotInventorySubsystem::InitializeInventory(const FString& SlotOverride)
{
    if (Saved) return;
    if (!SlotOverride.IsEmpty())
    {
        if (!IsSafeSlot(SlotOverride))
        {
            // Never silently redirect an invalid test override into the user's real save.
            bStorageReadOnly=true;
            bInvalidStorageOverride=true;
            Status=TEXT("Inventory slot override is invalid / Existing saves are untouched");
        }
        else SaveSlot=SlotOverride;
    }
    Definitions={
        {TEXT("rifle"),TEXT("M4A1 / PATROL"),TEXT("WEAPONS"),TEXT("A dependable 5.56 mm carbine prepared for field operations"),4,2,1,3.4f,true,false},
        {TEXT("smg"),TEXT("MP5 COMPACT"),TEXT("WEAPONS"),TEXT("A compact 9 mm platform for tight service corridors"),4,2,1,2.2f,true,false},
        {TEXT("pistol"),TEXT("USP SIDEARM"),TEXT("WEAPONS"),TEXT("A lightweight 9 mm sidearm / This hub supports one active weapon slot"),2,2,1,.9f,true,false},
        {TEXT("magazine"),TEXT("STANAG 30"),TEXT("WEAPONS"),TEXT("Spare rifle magazines for the field kit"),1,2,3,.5f,false,false},
        {TEXT("ammo"),TEXT("5.56 x 45"),TEXT("WEAPONS"),TEXT("A sealed rifle ammunition reserve"),2,1,120,1.4f,false,false},
        {TEXT("armor"),TEXT("FIELD CARRIER"),TEXT("GEAR"),TEXT("A reinforced plate carrier with utility pouches"),2,3,1,6.2f,false,false},
        {TEXT("medkit"),TEXT("TRAUMA KIT"),TEXT("MEDICAL"),TEXT("Use to restore up to 60 health / Consumes one trauma kit"),2,2,1,1.1f,false,true},
        {TEXT("bandage"),TEXT("FIELD DRESSING"),TEXT("MEDICAL"),TEXT("Use to restore up to 15 health / Consumes one dressing"),1,1,4,.2f,false,true},
        {TEXT("backpack"),TEXT("RANGER PACK"),TEXT("GEAR"),TEXT("A durable expedition pack with external utility straps"),2,3,1,1.8f,false,false},
        {TEXT("keycard"),TEXT("SERVICE ACCESS"),TEXT("GEAR"),TEXT("A maintenance credential for the carnival service buildings"),2,1,1,.1f,false,false},
        {TEXT("radio"),TEXT("FIELD RADIO"),TEXT("GEAR"),TEXT("A rugged shortwave radio with an extended battery"),1,2,1,.4f,false,false},
        {TEXT("wrench"),TEXT("ADJUSTABLE WRENCH"),TEXT("SALVAGE"),TEXT("Steel workshop equipment recovered from the depot"),1,2,1,.6f,false,false},
        {TEXT("electronics"),TEXT("CONTROL MODULE"),TEXT("SALVAGE"),TEXT("Salvaged attraction-control electronics with intact connectors"),2,2,2,.7f,false,false},
        {TEXT("fuel"),TEXT("RESERVE FUEL"),TEXT("SALVAGE"),TEXT("A sealed fuel can recovered from the service yard"),2,2,1,4.f,false,false}
    };
    auto AddPackWeapon=[this](const TCHAR* Id,const TCHAR* Name,const TCHAR* AssetName,const TCHAR* IconName,int32 Width,int32 Height)
    {
        FDepotItem& Item=Definitions.AddDefaulted_GetRef();
        Item.Id=Id;Item.Name=Name;Item.Category=TEXT("WEAPONS");Item.Description=TEXT("Original weapon from FPS Multiplayer Controller");
        Item.Width=Width;Item.Height=Height;Item.Quantity=0;Item.bFirearm=true;Item.bPackWeapon=true;
        Item.SourceAssetPath=FString::Printf(TEXT("/Game/FPS_Controller/Blueprints/DataAssets/WeaponsData/%s.%s"),AssetName,AssetName);
        Item.IconPath=FString::Printf(TEXT("/Game/FPS_Controller/UI/Textures/WeaponIcons/%s.%s"),IconName,IconName);
    };
    // These are separate stock item identities, never aliases for the legacy demo guns.
    AddPackWeapon(TEXT("pack_ak"),TEXT("AK"),TEXT("DA_Weapon_Ak"),TEXT("T_AK_Icon"),4,2);
    AddPackWeapon(TEXT("pack_m14"),TEXT("M14"),TEXT("DA_Weapon_M14"),TEXT("T_M14_Icon"),4,2);
    AddPackWeapon(TEXT("pack_mac10"),TEXT("MAC-10"),TEXT("DA_Weapon_Mac-10"),TEXT("T_Mac-10_Icon"),3,2);
    AddPackWeapon(TEXT("pack_x24"),TEXT("X24"),TEXT("DA_Weapon_X24"),TEXT("T_X24_Icon"),2,2);
    AddPackWeapon(TEXT("pack_shotgun"),TEXT("SHOTGUN"),TEXT("DA_Weapon_Shotgun"),TEXT("T_Shotgun_Icon"),4,2);
    if (bStorageReadOnly) {SetDefaults();return;}
    const bool bExists=UGameplayStatics::DoesSaveGameExist(SaveSlot,0);
    if (bExists) Saved=Cast<UBackstageDepotSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot,0));
    if (!Saved || Saved->Version!=1)
    {
        SetDefaults();
        // Preserve an unreadable/newer save until the user explicitly resets it.
        bStorageReadOnly=bExists;
        if (bExists) Status=TEXT("Existing inventory could not be loaded / It has been preserved");
    }
    SanitizeInventory();
}

void UBackstageDepotInventorySubsystem::SetDefaults()
{
    Saved=NewObject<UBackstageDepotSaveGame>(this);
    for (const FDepotItem& Item:Definitions)
    {
        if (Item.Quantity<=0) continue;
        Saved->Quantities.Add(Item.Id,Item.Quantity);
        if (Item.Id!=Saved->Primary) Saved->Stash.Add(Item.Id);
    }
}

void UBackstageDepotInventorySubsystem::SanitizeInventory()
{
    TSet<FName> Seen;
    for (auto It=Saved->Quantities.CreateIterator();It;++It)
    {
        if (!FindItem(It.Key()) || It.Value()<=0) It.RemoveCurrent();
    }
    const FDepotItem* Primary=FindItem(Saved->Primary);
    if (!Primary || !Primary->bFirearm || Saved->Quantities.FindRef(Saved->Primary)<=0) Saved->Primary=NAME_None;
    else Seen.Add(Saved->Primary);
    auto Clean=[this,&Seen](TArray<FName>& Values, bool bFieldKit)
    {
        Values.RemoveAll([this,&Seen,bFieldKit](FName Id)
        {
            const FDepotItem* Item=FindItem(Id);
            if (!Item || Seen.Contains(Id) || Saved->Quantities.FindRef(Id)<=0 || (bFieldKit&&Item->bFirearm)) return true;
            Seen.Add(Id);return false;
        });
    };
    Clean(Saved->FieldKit,true);Clean(Saved->Stash,false);
    // Recover owned entries omitted from both lists without creating extra quantities.
    for (const auto& Entry:Saved->Quantities) if (!Seen.Contains(Entry.Key)) Saved->Stash.Add(Entry.Key);
    Saved->Health=FMath::IsFinite(Saved->Health)?FMath::Clamp(Saved->Health,0.f,100.f):100.f;
    if (Saved->BuildPreset!=TEXT("Balanced") && Saved->BuildPreset!=TEXT("Close Quarters") && Saved->BuildPreset!=TEXT("Scout")) Saved->BuildPreset=TEXT("Balanced");
    if (!GetDestinations().ContainsByPredicate([this](const FDepotDestination& Destination){return Destination.Id==Saved->SelectedMap;})) Saved->SelectedMap=TEXT("BorderTown");
    // Retain earlier serialized cosmetic choices for save compatibility, but only
    // expose choices validated against the installed FPS pack through the getters.
    // Missing content must not erase a user's persisted data or activate a substitute.
}

const FDepotItem* UBackstageDepotInventorySubsystem::FindItem(FName Id) const
{
    return Definitions.FindByPredicate([Id](const FDepotItem& Item){return Item.Id==Id;});
}
bool UBackstageDepotInventorySubsystem::CanEdit() const { return Saved && !bTravelPending && !bStorageReadOnly; }
bool UBackstageDepotInventorySubsystem::OwnsFirearm(FName Id) const
{
    const FDepotItem* Item=FindItem(Id);
    return Saved && Item && Item->bFirearm && Saved->Quantities.FindRef(Id)>0
        && (Saved->Primary==Id || Saved->Stash.Contains(Id) || Saved->FieldKit.Contains(Id));
}
bool UBackstageDepotInventorySubsystem::SaveInventory()
{
    if (!Saved || bStorageReadOnly) {Status=TEXT("Inventory save is unavailable; the existing save is preserved");return false;}
    const bool bSuccess=UGameplayStatics::SaveGameToSlot(Saved,SaveSlot,0);
    Status=bSuccess?TEXT("LOADOUT SAVED"):TEXT("LOCAL SAVE FAILED / CHANGES REMAIN IN THIS SESSION");
    return bSuccess;
}
bool UBackstageDepotInventorySubsystem::SaveMutation(const FString& SuccessMessage)
{
    const bool bSaved=SaveInventory();if (bSaved) Status=SuccessMessage;return bSaved;
}
bool UBackstageDepotInventorySubsystem::EquipItem(FName Id)
{
    const FDepotItem* Item=FindItem(Id);
    if (!CanEdit() || !Item || !Saved->Stash.Contains(Id) || Saved->Quantities.FindRef(Id)<=0) return false;
    Saved->Stash.Remove(Id);
    if (Item->bFirearm) {if (!Saved->Primary.IsNone()) Saved->Stash.AddUnique(Saved->Primary);Saved->Primary=Id;}
    else Saved->FieldKit.AddUnique(Id);
    return SaveMutation(Item->Name+TEXT(" EQUIPPED"));
}
bool UBackstageDepotInventorySubsystem::ReturnItem(FName Id)
{
    if (!CanEdit() || !FindItem(Id) || (!Saved->FieldKit.Contains(Id)&&Saved->Primary!=Id)) return false;
    Saved->FieldKit.Remove(Id);if (Saved->Primary==Id) Saved->Primary=NAME_None;Saved->Stash.AddUnique(Id);
    return SaveMutation(TEXT("ITEM RETURNED TO STASH"));
}
bool UBackstageDepotInventorySubsystem::UseItem(FName Id)
{
    const FDepotItem* Item=FindItem(Id);
    if (!CanEdit() || !Item || !Item->bMedical || Saved->Quantities.FindRef(Id)<=0 || (!Saved->Stash.Contains(Id)&&!Saved->FieldKit.Contains(Id))) return false;
    if (Saved->Health>=100.f) {Status=TEXT("HEALTH IS ALREADY FULL");return false;}
    Saved->Health=FMath::Min(100.f,Saved->Health+(Id==TEXT("medkit")?60.f:15.f));
    int32& Quantity=Saved->Quantities.FindChecked(Id);
    if (--Quantity<=0) {Saved->Stash.Remove(Id);Saved->FieldKit.Remove(Id);Saved->Quantities.Remove(Id);}
    return SaveMutation(TEXT("MEDICAL SUPPLY USED / HEALTH RESTORED"));
}
bool UBackstageDepotInventorySubsystem::SelectPreset(FName Preset)
{
    if (!CanEdit() || (Preset!=TEXT("Balanced")&&Preset!=TEXT("Close Quarters")&&Preset!=TEXT("Scout"))) return false;
    Saved->BuildPreset=Preset;return SaveMutation(TEXT("BUILD PRESET SAVED"));
}
bool UBackstageDepotInventorySubsystem::ToggleMotion()
{
    if (!CanEdit()) return false;Saved->bReducedMotion=!Saved->bReducedMotion;
    return SaveMutation(Saved->bReducedMotion?TEXT("BACKGROUND MOTION DISABLED"):TEXT("BACKGROUND MOTION ENABLED"));
}
bool UBackstageDepotInventorySubsystem::ResetInventory()
{
    if (bTravelPending || bInvalidStorageOverride) return false;
    SetDefaults();bStorageReadOnly=false;return SaveMutation(TEXT("STARTER INVENTORY RESTORED"));
}

FDepotLoadout UBackstageDepotInventorySubsystem::CaptureLoadout() const
{
    FDepotLoadout Result;if (!Saved) return Result;
    Result.Primary=Saved->Primary;Result.FieldKit=Saved->FieldKit;Result.BuildPreset=Saved->BuildPreset;Result.Health=Saved->Health;
    Result.SelectedOperator=GetSelectedOperator();
    if (!Result.Primary.IsNone()) Result.Quantities.Add(Result.Primary,Saved->Quantities.FindRef(Result.Primary));
    for (FName Id:Result.FieldKit) Result.Quantities.Add(Id,Saved->Quantities.FindRef(Id));
    for (const auto& Entry:Result.Quantities)
    {
        const FName Finish=GetWeaponFinish(Entry.Key);
        if (!Finish.IsNone()) Result.WeaponFinishes.Add(Entry.Key,Finish);
    }
    return Result;
}
TArray<FDepotOperator> UBackstageDepotInventorySubsystem::GetOperators() const
{
    // No character is currently approved for the roster
    // Preserve saved IDs for compatibility without exposing a rejected character
    return {};
}
TArray<FDepotWeaponFinish> UBackstageDepotInventorySubsystem::GetWeaponFinishes() const
{
    TArray<FDepotWeaponFinish> Result;
    auto Add=[this,&Result](const TCHAR* WeaponId,const TCHAR* SkinName,const TCHAR* RelativeAsset)
    {
        if (!IsPackWeaponAvailable(FName(WeaponId))) return;
        const FString Package=FString(TEXT("/Game/FPS_Controller/Blueprints/DataAssets/SkinsData/"))+RelativeAsset;
        if (!FPackageName::DoesPackageExist(Package)) return;
        FDepotWeaponFinish& Item=Result.AddDefaulted_GetRef();
        Item.Id=SkinName;Item.WeaponId=WeaponId;Item.DisplayName=SkinName;
        Item.Description=TEXT("Original FPS Multiplayer Controller skin");
        Item.SkinDataAssetPath=Package+TEXT(".")+FPackageName::GetShortName(Package);
        const FString IconPackage=FString::Printf(TEXT("/Game/FPS_Controller/UI/Textures/SkinIcons/T_%sSkin_Icon"),SkinName);
        if (FPackageName::DoesPackageExist(IconPackage)) Item.IconPath=IconPackage+TEXT(".")+FPackageName::GetShortName(IconPackage);
        // UI chrome stays neutral; actual texture/material artwork supplies the skin colors.
        Item.Accent=FLinearColor(.72f,.67f,.55f);
    };
    // Stock SkinName values verified from this owned pack's DataAssets.
    Add(TEXT("pack_ak"),TEXT("Default"),TEXT("AK/DA_AK_Skin_Default"));
    Add(TEXT("pack_ak"),TEXT("Urban"),TEXT("AK/DA_AK_Skin_1"));
    Add(TEXT("pack_ak"),TEXT("Gold"),TEXT("AK/DA_AK_Skin_2"));
    Add(TEXT("pack_ak"),TEXT("Forest"),TEXT("AK/DA_AK_Skin_3"));
    Add(TEXT("pack_m14"),TEXT("Default"),TEXT("M14/DA_M14_Skin_Default"));
    Add(TEXT("pack_m14"),TEXT("Forest"),TEXT("M14/DA_M14_Skin_Forest"));
    Add(TEXT("pack_mac10"),TEXT("Default"),TEXT("Mac-10/DA_Mac_10_Default_Skin"));
    Add(TEXT("pack_mac10"),TEXT("Forest"),TEXT("Mac-10/DA_Mac_10_Forest_Skin"));
    Add(TEXT("pack_mac10"),TEXT("Gold"),TEXT("Mac-10/DA_Mac_10_Gold_Skin"));
    Add(TEXT("pack_mac10"),TEXT("Urban"),TEXT("Mac-10/DA_Mac_10_Urban_Skin"));
    Add(TEXT("pack_x24"),TEXT("Default"),TEXT("X24/DA_Pistol_Skin_Default"));
    Add(TEXT("pack_x24"),TEXT("Red"),TEXT("X24/DA_Pistol_Skin_01"));
    Add(TEXT("pack_x24"),TEXT("Forest"),TEXT("X24/DA_Pistol_Skin_02"));
    Add(TEXT("pack_x24"),TEXT("Urban"),TEXT("X24/DA_Pistol_Skin_03"));
    Add(TEXT("pack_x24"),TEXT("Noir"),TEXT("X24/DA_Pistol_Skin_04"));
    Add(TEXT("pack_shotgun"),TEXT("Default"),TEXT("Shotgun/DA_Shotgun_Skin_Default"));
    Add(TEXT("pack_shotgun"),TEXT("Forest"),TEXT("Shotgun/DA_Shotgun_Skin_Forest"));
    Add(TEXT("pack_shotgun"),TEXT("Gold"),TEXT("Shotgun/DA_Shotgun_Skin_Gold"));
    Add(TEXT("pack_shotgun"),TEXT("Urban"),TEXT("Shotgun/DA_Shotgun_Skin_Urban"));
    return Result;
}
TArray<FDepotWeaponFinish> UBackstageDepotInventorySubsystem::GetWeaponFinishesForWeapon(FName WeaponId) const
{
    return GetWeaponFinishes().FilterByPredicate([WeaponId](const FDepotWeaponFinish& Item){return Item.WeaponId==WeaponId;});
}
FName UBackstageDepotInventorySubsystem::GetSelectedOperator() const
{
    if (!Saved) return NAME_None;
    return GetOperators().ContainsByPredicate([this](const FDepotOperator& Item){return Item.Id==Saved->SelectedOperator;})?Saved->SelectedOperator:NAME_None;
}
bool UBackstageDepotInventorySubsystem::SelectOperator(FName Id)
{
    if (!CanEdit()) return false;
    if (!GetOperators().ContainsByPredicate([Id](const FDepotOperator& Item){return Item.Id==Id;})) {Status=TEXT("UNKNOWN OPERATOR");return false;}
    Saved->SelectedOperator=Id;return SaveMutation(TEXT("OPERATOR SAVED"));
}
FName UBackstageDepotInventorySubsystem::GetWeaponFinish(FName WeaponId) const
{
    if (!OwnsFirearm(WeaponId)) return NAME_None;
    const FName* Selected=Saved->WeaponFinishes.Find(WeaponId);
    if (!Selected) return NAME_None;
    return GetWeaponFinishesForWeapon(WeaponId).ContainsByPredicate([Selected](const FDepotWeaponFinish& Item){return Item.Id==*Selected;})?*Selected:NAME_None;
}
bool UBackstageDepotInventorySubsystem::SelectWeaponFinish(FName WeaponId,FName FinishId)
{
    if (!CanEdit()) return false;
    if (!OwnsFirearm(WeaponId)) {Status=TEXT("CHOOSE AN OWNED WEAPON");return false;}
    if (!GetWeaponFinishesForWeapon(WeaponId).ContainsByPredicate([FinishId](const FDepotWeaponFinish& Item){return Item.Id==FinishId;})) {Status=TEXT("WEAPON FINISH IS NOT AVAILABLE FOR THIS WEAPON");return false;}
    Saved->WeaponFinishes.Add(WeaponId,FinishId);return SaveMutation(TEXT("WEAPON FINISH SAVED"));
}
bool UBackstageDepotInventorySubsystem::IsPackWeaponAvailable(FName WeaponId) const
{
    const FDepotItem* Item=FindItem(WeaponId);
    return Item && Item->bPackWeapon && FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(Item->SourceAssetPath));
}
bool UBackstageDepotInventorySubsystem::HasPackStarterKit() const {return Saved && Saved->bFpsPackStarterKitClaimed;}
bool UBackstageDepotInventorySubsystem::CanAcquirePackStarterKit() const
{
    if (!CanEdit() || HasPackStarterKit()) return false;
    int32 Count=0;
    for (const FDepotItem& Item:Definitions)
    {
        if (!Item.bPackWeapon) continue;
        ++Count;
        if (!IsPackWeaponAvailable(Item.Id)
            || !GetWeaponFinishesForWeapon(Item.Id).ContainsByPredicate([](const FDepotWeaponFinish& Finish){return Finish.Id==TEXT("Default");})) return false;
    }
    return Count==5;
}
bool UBackstageDepotInventorySubsystem::AcquirePackStarterKit()
{
    if (!CanAcquirePackStarterKit()) {Status=TEXT("PACK STARTER KIT IS UNAVAILABLE OR ALREADY ADDED");return false;}
    for (const FDepotItem& Item:Definitions)
    {
        if (!Item.bPackWeapon) continue;
        Saved->Quantities.FindOrAdd(Item.Id)+=1;
        if (Saved->Primary!=Item.Id && !Saved->FieldKit.Contains(Item.Id)) Saved->Stash.AddUnique(Item.Id);
        if (!Saved->WeaponFinishes.Contains(Item.Id)) Saved->WeaponFinishes.Add(Item.Id,TEXT("Default"));
    }
    Saved->bFpsPackStarterKitClaimed=true;
    return SaveMutation(TEXT("PACK STARTER KIT ADDED TO STASH"));
}
TArray<FDepotDestination> UBackstageDepotInventorySubsystem::GetDestinations() const
{
    TArray<FDepotDestination> Result;
    FDepotDestination& Town=Result.AddDefaulted_GetRef();
    Town.Id=TEXT("BorderTown");Town.DisplayName=TEXT("BORDER TOWN");Town.Description=TEXT("San Paloma / Streets, shops and service alleys");Town.MapPackage=TEXT("/Game/BorderTown/Maps/BorderTown");
    FDepotDestination& Carnival=Result.AddDefaulted_GetRef();
    Carnival.Id=TEXT("Carnival");Carnival.DisplayName=TEXT("THE CARNIVAL");Carnival.Description=TEXT("Northern perimeter / Abandoned midway");Carnival.MapPackage=TEXT("/Game/Carnival/Maps/Carnival_Extraction");
    FDepotDestination& Mexico=Result.AddDefaulted_GetRef();
    Mexico.Id=TEXT("Mexico");Mexico.DisplayName=TEXT("MEXICO");Mexico.Description=TEXT("Day of the Dead / Village, plaza and cemetery");Mexico.MapPackage=TEXT("/Game/BorderTown/Maps/Mexico_Playtest");
    for (FDepotDestination& Destination:Result) Destination.bMapInstalled=FPackageName::DoesPackageExist(Destination.MapPackage);
    return Result;
}
FName UBackstageDepotInventorySubsystem::GetSelectedDestination() const {return Saved?Saved->SelectedMap:NAME_None;}
bool UBackstageDepotInventorySubsystem::SelectDestination(FName Id)
{
    if (!CanEdit()) return false;
    if (!GetDestinations().ContainsByPredicate([Id](const FDepotDestination& Item){return Item.Id==Id;})) {Status=TEXT("UNKNOWN DESTINATION");return false;}
    Saved->SelectedMap=Id;return SaveMutation(TEXT("DESTINATION SAVED"));
}
void UBackstageDepotInventorySubsystem::SetWeaponAdapterClass(TSubclassOf<UBackstageDepotWeaponAdapter> AdapterClass)
{
    if (bTravelPending) return;
    WeaponAdapter=(AdapterClass && !AdapterClass->HasAnyClassFlags(CLASS_Abstract))?NewObject<UBackstageDepotWeaponAdapter>(this,AdapterClass):nullptr;
}
bool UBackstageDepotInventorySubsystem::ResolveDeployment(FName Id, FString& Reason, FDepotDestination& Destination, UClass*& PawnClass) const
{
    PawnClass=nullptr;
    if (!Saved || bStorageReadOnly) {Reason=TEXT("Inventory is unavailable / Your existing save is preserved");return false;}
    if (bTravelPending) {Reason=TEXT("Deployment is already starting");return false;}
    const TArray<FDepotDestination> Maps=GetDestinations();
    const FDepotDestination* Map=Maps.FindByPredicate([Id](const FDepotDestination& Item){return Item.Id==Id;});
    if (!Map) {Reason=TEXT("Choose a valid destination");return false;}
    Destination=*Map;
    if (!Destination.bMapInstalled) {Reason=TEXT("This map has not been installed in this build");return false;}
    const FDepotItem* Primary=FindItem(Saved->Primary);
    if (!Primary || !Primary->bFirearm || Saved->Quantities.FindRef(Saved->Primary)<=0) {Reason=TEXT("Choose a primary weapon from your stash");return false;}
    if (!Primary->bPackWeapon) {Reason=TEXT("Choose a weapon from the FPS pack starter kit");return false;}
    if (!IsPackWeaponAvailable(Primary->Id)) {Reason=TEXT("The selected pack weapon is not installed");return false;}
    // Do not load the stock gameplay Blueprint graph while its adapter is unavailable.
    if (!WeaponAdapter) {Reason=TEXT("Playable weapon integration is not ready");return false;}
    if (!FPackageName::DoesPackageExist(TEXT("/Game/FPS_Controller/Blueprints/Game/BP_GM"))) {Reason=TEXT("The owned FPS Controller weapon pack still needs to be restored");return false;}
    UClass* ModeClass=FSoftClassPath(ControllerGameMode).TryLoadClass<AGameModeBase>();
    const AGameModeBase* Mode=ModeClass?ModeClass->GetDefaultObject<AGameModeBase>():nullptr;
    PawnClass=Mode?Mode->DefaultPawnClass.Get():nullptr;
    if (!PawnClass || !PawnClass->IsChildOf(ACharacter::StaticClass())) {Reason=TEXT("The weapon pack's playable character is not ready");return false;}
    if (!WeaponAdapter->ValidateLoadout(CaptureLoadout(),PawnClass,Reason))
    {if (Reason.IsEmpty()) Reason=TEXT("The selected loadout is not supported by the weapon integration");return false;}
    Reason=TEXT("READY / LOCAL PLAYTEST");return true;
}
bool UBackstageDepotInventorySubsystem::CanDeploy(FName Id, FString& Reason) const
{
    FDepotDestination Destination;UClass* PawnClass=nullptr;return ResolveDeployment(Id,Reason,Destination,PawnClass);
}
bool UBackstageDepotInventorySubsystem::TryDeploy(FName Id)
{
    FDepotDestination Destination;UClass* PawnClass=nullptr;
    if (!ResolveDeployment(Id,Status,Destination,PawnClass)) return false;
    UWorld* World=GetWorld();
    if (!World || !World->IsGameWorld()) {Status=TEXT("Deployment requires an active game session");return false;}
    const FName PreviousSelection=Saved->SelectedMap;Saved->SelectedMap=Id;
    if (!SaveInventory()) {Saved->SelectedMap=PreviousSelection;return false;}
    PendingLoadout=CaptureLoadout();PendingDestination=Id;PendingPawnClass=PawnClass;DeploymentId=FGuid::NewGuid();AppliedPawn.Reset();bTravelPending=true;
    // This playtest does not debit stash items or implement extraction losses.
    Status=TEXT("DEPLOYING");
    UGameplayStatics::OpenLevel(World,FName(*Destination.MapPackage),true,FString(TEXT("game="))+ControllerGameMode);
    return true;
}
bool UBackstageDepotInventorySubsystem::ApplyPendingLoadout(APawn* Pawn, FString& Reason)
{
    if (Pawn && AppliedPawn.Get()==Pawn && !bTravelPending) {Reason=TEXT("Loadout already applied");return true;}
    if (!bTravelPending || !WeaponAdapter) {Reason=TEXT("No pending deployment");return false;}
    APlayerController* Controller=Pawn?Cast<APlayerController>(Pawn->GetController()):nullptr;
    if (!Pawn || !Controller || !Controller->IsLocalController() || !Pawn->HasAuthority() || !Pawn->IsA(PendingPawnClass.Get()))
    {Reason=TEXT("Waiting for the local playable character to be possessed");return false;}
    const TArray<FDepotDestination> Maps=GetDestinations();
    const FDepotDestination* Destination=Maps.FindByPredicate([this](const FDepotDestination& Item){return Item.Id==PendingDestination;});
    if (Pawn->GetGameInstance()!=GetGameInstance() || !Destination || UGameplayStatics::GetCurrentLevelName(Pawn,true)!=FPackageName::GetShortName(Destination->MapPackage))
    {Reason=TEXT("The playable character is not in the selected map");return false;}
    if (!WeaponAdapter->ApplyLoadout(Pawn,PendingLoadout,DeploymentId,Reason)) return false;
    AppliedPawn=Pawn;bTravelPending=false;Status=TEXT("LOADOUT APPLIED / LOCAL PLAYTEST");return true;
}
void UBackstageDepotInventorySubsystem::OnTravelFailure(UWorld* World, ETravelFailure::Type, const FString& Error)
{
    if (!bTravelPending || (World && World->GetGameInstance()!=GetGameInstance())) return;
    bTravelPending=false;PendingPawnClass=nullptr;PendingDestination=NAME_None;PendingLoadout=FDepotLoadout();
    Status=TEXT("Map could not be opened / Your stash is unchanged");
    UE_LOG(LogTemp,Warning,TEXT("Depot deployment travel failed: %s"),*Error);
}
void UBackstageDepotInventorySubsystem::CancelPendingDeployment()
{
    if (!bTravelPending) return;
    bTravelPending=false;PendingPawnClass=nullptr;PendingDestination=NAME_None;PendingLoadout=FDepotLoadout();
    Status=TEXT("Returned to safehouse / Your stash is unchanged");
}
