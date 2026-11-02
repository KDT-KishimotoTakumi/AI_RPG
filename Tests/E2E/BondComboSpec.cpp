#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/RunScreen.h"
#include "Domain/ComboCatalog.h"
namespace {
class BondComboMemory : public GirlsSpire::SlotFiles,public GirlsSpire::JournalFiles {
public:
    std::array<std::array<std::optional<std::string>,2>,2> Slots{};std::optional<std::string> Journal;
    GirlsSpire::SlotBytes Read(GirlsSpire::SaveKind kind,bool backup)override{return {false,Slots[static_cast<int>(kind)-1][backup]};}
    GirlsSpire::SlotBytes ReadJournal()override{return {false,Journal};}
    bool Replace(GirlsSpire::SaveKind kind,bool backup,std::string_view bytes)override{Slots[static_cast<int>(kind)-1][backup]=std::string(bytes);return true;}
    bool ReplaceJournal(std::string_view bytes)override{Journal=std::string(bytes);return true;}
};
}
BEGIN_DEFINE_SPEC(FGirlsSpireBondComboSpec,"GirlsSpire.UI.BondCombos",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
    std::shared_ptr<BondComboMemory> Files;
    std::shared_ptr<GirlsSpire::SaveTransaction> Service;
END_DEFINE_SPEC(FGirlsSpireBondComboSpec)
void FGirlsSpireBondComboSpec::Define()
{
    BeforeEach([this]{FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();Window=SNew(SWindow).ClientSize(FVector2D(1200,1000));FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();});
    LatentBeforeEach(WaitForSlateLayout);
    for(int id=1;id<=8;++id)It(*FString::Printf(TEXT("専用連携%dを実操作で発見し中断再開する"),200+id),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this,id]{
        using namespace GirlsSpire;Profile profile;profile.Bonds[id-1]=100;
        Run run(42,Difficulty::Beginner,HeroRole::Sword,profile);auto progress=run.Checkpoint();progress.State.Day=5;progress.State.Phase=RunPhase::Event;progress.State.Active={NodeKind::Battle};progress.State.Choices.clear();progress.State.Recruited={id-1};
        BattleUnit hero;hero.At={0,1};hero.Hp=hero.MaxHp=80;hero.Attack=10;hero.Accuracy=200;hero.Speed=30;hero.Range=3;
        auto ally=hero;ally.Id=id;ally.At={2,1};ally.Hp=ally.MaxHp=40;ally.Speed=20;
        auto enemy=hero;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={1,2};enemy.Hp=enemy.MaxHp=1000;enemy.Speed=10;enemy.Attack=0;enemy.Movement=0;enemy.Evasion=0;
        Battle initial(8,8,std::vector<Tile>(64),{hero,ally,enemy},42);auto checkpoint=initial.Checkpoint();checkpoint.State.AbilityRules=AbilityRuleSet::CompleteCharacters;checkpoint.State.StartingBonds=profile.Bonds;
        CombatControl control;control.ProfileDriven=true;control.Trust[id]=100;
        Files=std::make_shared<BondComboMemory>();Service=std::make_shared<SaveTransaction>(*Files,*Files);
        if(!TestTrue(TEXT("初期状態を取引保存"),Service->Prepare(RunSaveState{progress,SavedCombat{checkpoint,control.Checkpoint()}},profile)==SlotIssue::None && Service->Retry()==SlotIssue::None))return;
        Async(EAsyncExecution::TaskGraphMainThread,[this]{Window->SetContent(SNew(SRunScreen).SaveService(Service));}).Wait();DrainSlateLayout();
        const auto click=[&](const char* key){const auto target=Driver->FindElement(By::Id(key));if(!target->IsVisible()){const auto scroll=Driver->FindElement(By::Id("battle.scroll"))->Exists()?Driver->FindElement(By::Id("battle.scroll")):Driver->FindElement(By::Id("run.scroll"));scroll->ScrollToBeginning();DrainSlateLayout();if(!target->IsVisible()){scroll->ScrollToEndUntil(By::Id(key));DrainSlateLayout();}}return TestTrue(UTF8_TO_TCHAR(key),ClickAndDrain(target));};
        if(!click("title.resume") || !click("battle.cell.1.1") || !click("battle.move.confirm") || !click("battle.cell.1.2") || !click("battle.confirm") || !click("battle.wait.south") || !click("battle.cell.1.2") || !click("intent.execute"))return;
        const auto result=Service->Load();const auto recipe=FindComboRecipe(200+id);
        TestTrue(TEXT("専用だけゲージ20・発見を同時保存"),result.Issue==SlotIssue::None && result.Permanent.DiscoveredCombos==std::vector<int>{200+id} && result.Run && result.Run->Combat && result.Run->Combat->Battle.State.LastCombo && result.Run->Combat->Battle.State.LastCombo->RecipeId==200+id && result.Run->Combat->Battle.State.LastCombo->GaugeGain==20 && result.Run->Combat->Battle.State.Combos.Gauge==20);
        const auto text=Driver->FindElement(By::Id("battle.combo.result"))->GetText().ToString();TestTrue(TEXT("初発見/名称/承→結の理由を表示"),recipe && text.Contains(UTF8_TO_TCHAR(recipe->Name.data())) && text.Contains(TEXT("初発見")) && text.Contains(TEXT("承→")));
        if(!click("save.suspend"))return;TestTrue(TEXT("タイトルの恒久一覧に表示"),Driver->FindElement(By::Id("title.combos"))->GetText().ToString().Contains(UTF8_TO_TCHAR(recipe->Name.data())));
        Async(EAsyncExecution::TaskGraphMainThread,[this]{Window->SetContent(SNew(SRunScreen).SaveService(Service));}).Wait();DrainSlateLayout();if(!click("title.resume"))return;
        const auto resumed=Service->Load();TestTrue(TEXT("既知表示・乱数/状態を保持し再発見しない"),Driver->FindElement(By::Id("battle.combo.result"))->GetText().ToString().Contains(TEXT("発見済み")) && resumed.Permanent==result.Permanent && resumed.Run && result.Run && resumed.Run->Combat==result.Run->Combat);
    });
    AfterEach([this]{Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}Service.reset();Files.reset();IAutomationDriverModule::Get().Disable();});
}
