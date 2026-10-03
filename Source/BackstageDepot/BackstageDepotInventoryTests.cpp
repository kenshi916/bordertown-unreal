#if WITH_DEV_AUTOMATION_TESTS
#include "BackstageDepot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

namespace
{
struct FDepotInventoryFixture
{
    FString Slot=TEXT("DepotAutomation_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UGameInstance* Game=NewObject<UGameInstance>();
    UBackstageDepotInventorySubsystem* State=nullptr;
    FDepotInventoryFixture()
    {
        Game->AddToRoot();
        State=NewObject<UBackstageDepotInventorySubsystem>(Game);State->AddToRoot();
        State->InitializeInventory(Slot);
    }
    ~FDepotInventoryFixture()
    {
        UGameplayStatics::DeleteGameInSlot(Slot,0);
        State->RemoveFromRoot();Game->RemoveFromRoot();
    }
    UBackstageDepotInventorySubsystem* Reload() const
    {
        UBackstageDepotInventorySubsystem* Result=NewObject<UBackstageDepotInventorySubsystem>(Game);
        Result->InitializeInventory(Slot);return Result;
    }
};
constexpr EAutomationTestFlags DepotTestFlags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDepotSaveReloadTest,"BorderTown.Depot.Inventory.SaveReloadAndCarriedSnapshot",DepotTestFlags)
bool FDepotSaveReloadTest::RunTest(const FString&)
{
    FDepotInventoryFixture F;
    TestTrue(TEXT("Equip selected firearm"),F.State->EquipItem(TEXT("smg")));
    TestTrue(TEXT("Previous primary returns to stash"),F.State->Inventory()->Stash.Contains(TEXT("rifle")));
    TestTrue(TEXT("Pack bandages"),F.State->EquipItem(TEXT("bandage")));
    TestTrue(TEXT("Consume one bandage"),F.State->UseItem(TEXT("bandage")));
    TestTrue(TEXT("Persist Carnival selection"),F.State->SelectDestination(TEXT("Carnival")));
    UBackstageDepotInventorySubsystem* Reloaded=F.Reload();
    TestEqual(TEXT("Primary survives new session"),Reloaded->Inventory()->Primary,FName(TEXT("smg")));
    TestEqual(TEXT("Medical quantity survives new session"),Reloaded->Inventory()->Quantities.FindRef(TEXT("bandage")),3);
    TestEqual(TEXT("Health survives new session"),Reloaded->Inventory()->Health,97.f);
    TestEqual(TEXT("Destination survives new session"),Reloaded->GetSelectedDestination(),FName(TEXT("Carnival")));
    TestTrue(TEXT("Field kit survives new session"),Reloaded->Inventory()->FieldKit.Contains(TEXT("bandage")));
    const FDepotLoadout Snapshot=Reloaded->CaptureLoadout();
    TestTrue(TEXT("Snapshot carries equipped primary"),Snapshot.Quantities.Contains(TEXT("smg")));
    TestTrue(TEXT("Snapshot carries packed supplies"),Snapshot.Quantities.Contains(TEXT("bandage")));
    TestFalse(TEXT("Snapshot cannot issue stash rifle"),Snapshot.Quantities.Contains(TEXT("rifle")));
    TestFalse(TEXT("Snapshot cannot issue unpacked ammunition"),Snapshot.Quantities.Contains(TEXT("ammo")));
    UBackstageDepotSaveGame* OriginalState=F.State->Inventory();
    F.State->InitializeInventory(TEXT("SHOULD_NOT_REPLACE_LIVE_STATE"));
    TestTrue(TEXT("Initialization preserves the live state object"),OriginalState==F.State->Inventory());
    TestEqual(TEXT("Initialization cannot reset live state"),F.State->Inventory()->Primary,FName(TEXT("smg")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDepotRejectedDeployTest,"BorderTown.Depot.Deployment.InvalidDestinationAndMissingAdapterPreserveInventory",DepotTestFlags)
bool FDepotRejectedDeployTest::RunTest(const FString&)
{
    FDepotInventoryFixture F;
    F.State->EquipItem(TEXT("pistol"));F.State->EquipItem(TEXT("medkit"));
    TArray<uint8> Before,After;
    UGameplayStatics::SaveGameToMemory(F.State->Inventory(),Before);
    TestFalse(TEXT("Reject arbitrary map ID"),F.State->TryDeploy(TEXT("../../NotAMap")));
    TestFalse(TEXT("Reject pack/adapter that has not been verified"),F.State->TryDeploy(TEXT("BorderTown")));
    TestFalse(TEXT("Rejected deployment cannot enter pending state"),F.State->HasPendingDeployment());
    UGameplayStatics::SaveGameToMemory(F.State->Inventory(),After);
    TestTrue(TEXT("Rejected deployment leaves serialized inventory byte-identical"),Before==After);
    UBackstageDepotInventorySubsystem* Reloaded=F.Reload();
    TestEqual(TEXT("Persisted primary unchanged"),Reloaded->Inventory()->Primary,FName(TEXT("pistol")));
    TestTrue(TEXT("Persisted medical supply unchanged"),Reloaded->Inventory()->FieldKit.Contains(TEXT("medkit")));
    FString Reason;
    TestFalse(TEXT("No pawn can apply without pending deployment"),F.State->ApplyPendingLoadout(nullptr,Reason));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDepotControllerLifetimeTest,"BorderTown.Depot.Inventory.ControllerReplacementCannotOverwriteSharedState",DepotTestFlags)
bool FDepotControllerLifetimeTest::RunTest(const FString&)
{
    FDepotInventoryFixture F;
    const UWorld::InitializationValues WorldOptions=UWorld::InitializationValues()
        .AllowAudioPlayback(false).CreatePhysicsScene(false).ShouldSimulatePhysics(false)
        .CreateNavigation(false).CreateAISystem(false);
    // CreateWorld initializes the persistent level and WorldSettings itself.
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&WorldOptions);
    if (!TestNotNull(TEXT("Transient test world"),World)) return false;
    ABackstageDepotPlayerController* OldController=World->SpawnActor<ABackstageDepotPlayerController>();
    ABackstageDepotPlayerController* NewController=World->SpawnActor<ABackstageDepotPlayerController>();
    FObjectPropertyBase* StateProperty=FindFProperty<FObjectPropertyBase>(ABackstageDepotPlayerController::StaticClass(),TEXT("InventoryState"));
    if (!TestNotNull(TEXT("First controller"),OldController)||!TestNotNull(TEXT("Replacement controller"),NewController)||!TestNotNull(TEXT("Shared state binding"),StateProperty))
    {World->DestroyWorld(false);return false;}
    StateProperty->SetObjectPropertyValue_InContainer(OldController,F.State);
    StateProperty->SetObjectPropertyValue_InContainer(NewController,F.State);
    OldController->EquipItem(TEXT("smg"));
    NewController->EquipItem(TEXT("pistol"));
    TestTrue(TEXT("Controllers see same state across world presentation lifecycle"),OldController->Inventory()==NewController->Inventory());
    OldController->EndPlay(EEndPlayReason::LevelTransition);
    TestNull(TEXT("Departing controller releases state reference"),OldController->Inventory());
    TestFalse(TEXT("Departing controller cannot write a stale save"),OldController->SaveInventory());
    TestEqual(TEXT("Replacement still holds selected primary"),NewController->GetEquippedPrimary(),FName(TEXT("pistol")));
    UBackstageDepotInventorySubsystem* Reloaded=F.Reload();
    TestEqual(TEXT("Teardown does not overwrite persisted newer selection"),Reloaded->Inventory()->Primary,FName(TEXT("pistol")));
    NewController->EndPlay(EEndPlayReason::LevelTransition);
    TestEqual(TEXT("Shared state survives both controller teardowns"),F.State->Inventory()->Primary,FName(TEXT("pistol")));
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDepotInvalidSlotTest,"BorderTown.Depot.Inventory.InvalidSlotNeverFallsBackToUserProfile",DepotTestFlags)
bool FDepotInvalidSlotTest::RunTest(const FString&)
{
    UGameInstance* Game=NewObject<UGameInstance>();
    UBackstageDepotInventorySubsystem* State=NewObject<UBackstageDepotInventorySubsystem>(Game);
    State->InitializeInventory(TEXT("../BackstageDepot_Inventory_v1"));
    TestFalse(TEXT("Invalid override cannot save"),State->SaveInventory());
    TestFalse(TEXT("Invalid override cannot equip"),State->EquipItem(TEXT("smg")));
    TestFalse(TEXT("Invalid override cannot reset real user slot"),State->ResetInventory());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDepotPackCatalogTest,"BorderTown.Depot.Cosmetics.AuthenticPackCatalogAndExplicitKit",DepotTestFlags)
bool FDepotPackCatalogTest::RunTest(const FString&)
{
    FDepotInventoryFixture F;
    const TMap<FName,int32> BeforeQuantities=F.State->Inventory()->Quantities;
    TestFalse(TEXT("Loading existing inventory cannot auto claim pack kit"),F.State->HasPackStarterKit());
    for (const FDepotItem& Item:F.State->Items())
    {
        if (!Item.bPackWeapon) continue;
        TestFalse(TEXT("Pack catalog cannot auto grant guns"),F.State->Inventory()->Quantities.Contains(Item.Id));
        TestTrue(TEXT("Pack gun uses the verified donor namespace"),Item.SourceAssetPath.StartsWith(TEXT("/Game/FPS_Controller/Blueprints/DataAssets/WeaponsData/")));
        TestTrue(TEXT("Pack gun uses authored weapon icon"),Item.IconPath.StartsWith(TEXT("/Game/FPS_Controller/UI/Textures/WeaponIcons/")));
    }
    TestTrue(TEXT("No rejected character appears in the operator catalog"),F.State->GetOperators().IsEmpty());
    TestFalse(TEXT("Rejected Oskar cannot be selected"),F.State->SelectOperator(TEXT("Oskar")));
    F.State->Inventory()->SelectedOperator=TEXT("Oskar");
    TestTrue(TEXT("Preserve an earlier Oskar save for compatibility testing"),F.State->SaveInventory());
    UBackstageDepotInventorySubsystem* RestoredOperator=F.Reload();
    TestEqual(TEXT("Earlier raw operator choice is preserved"),RestoredOperator->Inventory()->SelectedOperator,FName(TEXT("Oskar")));
    TestTrue(TEXT("Earlier rejected operator is not exposed as selected"),RestoredOperator->GetSelectedOperator().IsNone());
    TestTrue(TEXT("Rejected operator does not enter a deployment snapshot"),RestoredOperator->CaptureLoadout().SelectedOperator.IsNone());
    TestTrue(TEXT("Old gun IDs cannot borrow real pack finishes"),F.State->GetWeaponFinishesForWeapon(TEXT("rifle")).IsEmpty());
    for (const FDepotWeaponFinish& Finish:F.State->GetWeaponFinishes())
    {
        TestTrue(TEXT("Every available finish belongs to a pack weapon"),Finish.WeaponId.ToString().StartsWith(TEXT("pack_")));
        TestTrue(TEXT("Every available finish resolves an authored skin DataAsset"),Finish.SkinDataAssetPath.StartsWith(TEXT("/Game/FPS_Controller/Blueprints/DataAssets/SkinsData/")));
        TestTrue(TEXT("No invented finish IDs remain"),Finish.Id!=TEXT("Factory")&&Finish.Id!=TEXT("Sandstorm")&&Finish.Id!=TEXT("Nightwatch")&&Finish.Id!=TEXT("Midway"));
    }
    if (!F.State->CanAcquirePackStarterKit())
    {
        TestFalse(TEXT("Missing installed content blocks kit claim"),F.State->AcquirePackStarterKit());
        TestTrue(TEXT("Unavailable kit preserves all quantities"),BeforeQuantities.OrderIndependentCompareEqual(F.State->Inventory()->Quantities));
        AddInfo(TEXT("Pack content unavailable in this test environment; installed-kit path is covered when donor content is present"));
        return true;
    }
    TestTrue(TEXT("Explicit action grants pack starter kit"),F.State->AcquirePackStarterKit());
    TestTrue(TEXT("Explicit kit claim flag is set"),F.State->HasPackStarterKit());
    TestEqual(TEXT("Kit does not silently remap legacy primary"),F.State->Inventory()->Primary,FName(TEXT("rifle")));
    for (const auto& Entry:BeforeQuantities)
        TestEqual(TEXT("Kit preserves legacy item quantity"),F.State->Inventory()->Quantities.FindRef(Entry.Key),Entry.Value);
    int32 PackCount=0;
    for (const FDepotItem& Item:F.State->Items())
    {
        if (!Item.bPackWeapon) continue;
        ++PackCount;
        TestEqual(TEXT("One of each real gun is added"),F.State->Inventory()->Quantities.FindRef(Item.Id),1);
        TestTrue(TEXT("Pack gun enters stash"),F.State->Inventory()->Stash.Contains(Item.Id));
        TestEqual(TEXT("Pack gun gets its actual default skin"),F.State->GetWeaponFinish(Item.Id),FName(TEXT("Default")));
    }
    TestEqual(TEXT("Exactly five stock firearms registered"),PackCount,5);
    TArray<uint8> BeforeRepeat,AfterRepeat;
    UGameplayStatics::SaveGameToMemory(F.State->Inventory(),BeforeRepeat);
    TestFalse(TEXT("Kit cannot be claimed twice"),F.State->AcquirePackStarterKit());
    UGameplayStatics::SaveGameToMemory(F.State->Inventory(),AfterRepeat);
    TestTrue(TEXT("Repeated kit claim leaves serialized inventory unchanged"),BeforeRepeat==AfterRepeat);
    UBackstageDepotInventorySubsystem* Reloaded=F.Reload();
    TestTrue(TEXT("Kit flag persists across sessions"),Reloaded->HasPackStarterKit());
    TestFalse(TEXT("New session cannot duplicate starter kit"),Reloaded->AcquirePackStarterKit());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDepotPackFinishPersistenceTest,"BorderTown.Depot.Cosmetics.PackFinishesStayWeaponSpecific",DepotTestFlags)
bool FDepotPackFinishPersistenceTest::RunTest(const FString&)
{
    FDepotInventoryFixture F;
    if (!F.State->CanAcquirePackStarterKit())
    {
        TestFalse(TEXT("Absent pack cannot accept a fabricated skin"),F.State->SelectWeaponFinish(TEXT("pack_ak"),TEXT("Gold")));
        AddInfo(TEXT("Installed stock-skin persistence requires donor content"));
        return true;
    }
    TestTrue(TEXT("Acquire owned pack kit explicitly"),F.State->AcquirePackStarterKit());
    TestTrue(TEXT("Save stock AK Gold"),F.State->SelectWeaponFinish(TEXT("pack_ak"),TEXT("Gold")));
    TestTrue(TEXT("Save stock MAC-10 Urban"),F.State->SelectWeaponFinish(TEXT("pack_mac10"),TEXT("Urban")));
    TestTrue(TEXT("Save stock X24 Noir"),F.State->SelectWeaponFinish(TEXT("pack_x24"),TEXT("Noir")));
    TestFalse(TEXT("AK cannot borrow X24 Noir"),F.State->SelectWeaponFinish(TEXT("pack_ak"),TEXT("Noir")));
    TestFalse(TEXT("M14 cannot borrow AK Gold"),F.State->SelectWeaponFinish(TEXT("pack_m14"),TEXT("Gold")));
    TestTrue(TEXT("Equip exact stock AK identity"),F.State->EquipItem(TEXT("pack_ak")));
    UBackstageDepotInventorySubsystem* Reloaded=F.Reload();
    TestEqual(TEXT("AK skin persists"),Reloaded->GetWeaponFinish(TEXT("pack_ak")),FName(TEXT("Gold")));
    TestEqual(TEXT("MAC-10 skin remains independent"),Reloaded->GetWeaponFinish(TEXT("pack_mac10")),FName(TEXT("Urban")));
    TestEqual(TEXT("X24 skin remains independent"),Reloaded->GetWeaponFinish(TEXT("pack_x24")),FName(TEXT("Noir")));
    const FDepotLoadout Snapshot=Reloaded->CaptureLoadout();
    TestTrue(TEXT("No operator is silently substituted"),Snapshot.SelectedOperator.IsNone());
    TestEqual(TEXT("Only carried weapon skin enters snapshot"),Snapshot.WeaponFinishes.Num(),1);
    TestEqual(TEXT("Snapshot contains exact stock AK skin"),Snapshot.WeaponFinishes.FindRef(TEXT("pack_ak")),FName(TEXT("Gold")));
    TestFalse(TEXT("Snapshot cannot issue stash MAC-10 skin"),Snapshot.WeaponFinishes.Contains(TEXT("pack_mac10")));
    TestFalse(TEXT("Snapshot cannot issue stash X24 skin"),Snapshot.WeaponFinishes.Contains(TEXT("pack_x24")));
    TestFalse(TEXT("Stock cosmetics cannot bypass the verified gameplay adapter"),Reloaded->TryDeploy(TEXT("BorderTown")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDepotLegacyCosmeticPreservationTest,"BorderTown.Depot.Cosmetics.LegacyChoicesPreservedButNotExposed",DepotTestFlags)
bool FDepotLegacyCosmeticPreservationTest::RunTest(const FString&)
{
    FDepotInventoryFixture F;
    TestTrue(TEXT("New inventory makes no unverified operator claim"),F.State->GetSelectedOperator().IsNone());
    TestTrue(TEXT("New inventory makes no unverified finish claim"),F.State->GetWeaponFinish(TEXT("rifle")).IsNone());
    F.State->EquipItem(TEXT("smg"));F.State->EquipItem(TEXT("bandage"));F.State->SelectDestination(TEXT("Carnival"));
    F.State->Inventory()->SelectedOperator=TEXT("Nomad");
    F.State->Inventory()->WeaponFinishes.Add(TEXT("rifle"),TEXT("Sandstorm"));
    F.State->Inventory()->WeaponFinishes.Add(TEXT("smg"),TEXT("Nightwatch"));
    const TMap<FName,int32> Quantities=F.State->Inventory()->Quantities;
    TestTrue(TEXT("Persist earlier v1 cosmetics for compatibility test"),F.State->SaveInventory());
    UBackstageDepotInventorySubsystem* Reloaded=F.Reload();
    TestEqual(TEXT("Save remains version one"),Reloaded->Inventory()->Version,1);
    TestEqual(TEXT("Earlier raw operator value is preserved"),Reloaded->Inventory()->SelectedOperator,FName(TEXT("Nomad")));
    TestEqual(TEXT("Earlier raw finish value is preserved"),Reloaded->Inventory()->WeaponFinishes.FindRef(TEXT("rifle")),FName(TEXT("Sandstorm")));
    TestTrue(TEXT("Unverified operator is never exposed as available"),Reloaded->GetSelectedOperator().IsNone());
    TestTrue(TEXT("Unverified finish is never exposed as available"),Reloaded->GetWeaponFinish(TEXT("rifle")).IsNone());
    TestEqual(TEXT("Selected legacy gun is preserved without remapping"),Reloaded->Inventory()->Primary,FName(TEXT("smg")));
    TestEqual(TEXT("Destination is preserved"),Reloaded->GetSelectedDestination(),FName(TEXT("Carnival")));
    TestTrue(TEXT("Gear quantities are preserved"),Reloaded->Inventory()->Quantities.OrderIndependentCompareEqual(Quantities));
    TestTrue(TEXT("Legacy invented skins cannot enter deployment snapshot"),Reloaded->CaptureLoadout().WeaponFinishes.IsEmpty());
    TArray<uint8> Before,After;
    UGameplayStatics::SaveGameToMemory(Reloaded->Inventory(),Before);
    TestFalse(TEXT("Earlier unrelated operator is rejected"),Reloaded->SelectOperator(TEXT("Nomad")));
    TestFalse(TEXT("Earlier invented finish is rejected"),Reloaded->SelectWeaponFinish(TEXT("rifle"),TEXT("Sandstorm")));
    TestFalse(TEXT("Unknown operator is rejected"),Reloaded->SelectOperator(TEXT("UnknownOperator")));
    TestFalse(TEXT("Unknown weapon is rejected"),Reloaded->SelectWeaponFinish(TEXT("UnknownWeapon"),TEXT("Default")));
    TestFalse(TEXT("Nonfirearm cannot receive a pack skin"),Reloaded->SelectWeaponFinish(TEXT("wrench"),TEXT("Default")));
    TestFalse(TEXT("Unowned pack weapon cannot receive a skin"),Reloaded->SelectWeaponFinish(TEXT("pack_ak"),TEXT("Gold")));
    UGameplayStatics::SaveGameToMemory(Reloaded->Inventory(),After);
    TestTrue(TEXT("Rejected choices leave serialized profile unchanged"),Before==After);
    return true;
}
#endif
