#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/RunScreen.h"
#include <atomic>
namespace {
class DiscoveryMemory : public GirlsSpire::SlotFiles,public GirlsSpire::JournalFiles {
public:
    std::array<std::array<std::optional<std::string>,2>,2> Slots{};
    std::optional<std::string> Journal;
    std::atomic_bool Fail=false;
    GirlsSpire::SlotBytes Read(GirlsSpire::SaveKind kind,bool backup) override {return {false,Slots[static_cast<int>(kind)-1][backup]};}
    GirlsSpire::SlotBytes ReadJournal() override {return {false,Journal};}
    bool Replace(GirlsSpire::SaveKind kind,bool backup,std::string_view bytes) override {if(Fail)return false;Slots[static_cast<int>(kind)-1][backup]=std::string(bytes);return true;}
    bool ReplaceJournal(std::string_view bytes) override {if(Fail)return false;Journal=std::string(bytes);return true;}
};
}
BEGIN_DEFINE_SPEC(FGirlsSpireComboDiscoverySpec,"GirlsSpire.UI.ComboDiscovery",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
    bool Click(const char* id);
END_DEFINE_SPEC(FGirlsSpireComboDiscoverySpec)
bool FGirlsSpireComboDiscoverySpec::Click(const char* id)
{
    auto target=Driver->FindElement(By::Id(id));
    if(!target->IsVisible()) {
        const auto scroll=Driver->FindElement(By::Id("battle.scroll"));
        if(scroll->Exists())scroll->ScrollToEndUntil(By::Id(id));
        else Driver->FindElement(By::Id("run.scroll"))->ScrollToEndUntil(By::Id(id));
        DrainSlateLayout();
    }
    return ClickAndDrain(target);
}
void FGirlsSpireComboDiscoverySpec::Define()
{
    BeforeEach([this] {
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        Window=SNew(SWindow).Title(FText::FromString(TEXT("Girls Spire — 恒久連携発見テスト"))).ClientSize(FVector2D(1100,760));
        FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    for(bool fail:{false,true})It(fail?TEXT("発見保存失敗で停止し同じ取引を再試行する"):TEXT("初発見を保存し中断再開後は既知と表示する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this,fail] {
        using namespace GirlsSpire;
        auto files=std::make_shared<DiscoveryMemory>();auto service=std::make_shared<SaveTransaction>(*files,*files);
        Profile profile;profile.Bonds[0]=75;
        Run run(42,Difficulty::Beginner,HeroRole::Sword,profile);auto progress=run.Checkpoint();
        progress.State.Day=5;progress.State.Phase=RunPhase::Event;progress.State.Active={NodeKind::Battle};progress.State.Choices.clear();progress.State.Recruited={0};
        BattleUnit hero;hero.At={1,1};hero.Hp=hero.MaxHp=80;hero.Speed=20;
        auto ally=hero;ally.Id=1;ally.At={2,1};ally.Hp=ally.MaxHp=40;ally.Speed=15;ally.Range=8;ally.Accuracy=200;
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={6,1};enemy.Hp=enemy.MaxHp=1000;enemy.Speed=0;enemy.Attack=0;enemy.Movement=0;
        Battle battle(8,8,std::vector<Tile>(64),{hero,ally,enemy},0);
        CombatControl control;control.ProfileDriven=true;control.Trust[1]=75;
        RunSaveState saved{progress,SavedCombat{battle.Checkpoint(),control.Checkpoint()},true};
        if(!TestTrue(TEXT("試験保存を準備"),service->Prepare(saved,profile)==SlotIssue::None && service->Retry()==SlotIssue::None))return;
        Async(EAsyncExecution::TaskGraphMainThread,[this,service]{Window->SetContent(SNew(SRunScreen).SaveService(service));}).Wait();DrainSlateLayout();
        if(!Click("title.resume") || !Click("battle.wait.south"))return;
        files->Fail=fail;
        if(!TestTrue(TEXT("仲間の攻撃で連携"),Click("intent.execute")))return;
        if(fail) {
            TestTrue(TEXT("発見の保存失敗で停止"),service->HasPending() && Driver->FindElement(By::Id("save.error"))->Exists() && !Driver->FindElement(By::Id("intent.execute"))->Exists());
            files->Fail=false;if(!TestTrue(TEXT("同じ取引を再試行"),Click("save.retry")))return;
        }
        const auto result=service->Load();
        TestTrue(TEXT("発見を周回戦闘と同時保存"),result.Issue==SlotIssue::None && result.Permanent.DiscoveredCombos==std::vector<int>{2} && result.Run && result.Run->Combat && result.Run->Combat->Battle.State.LastCombo && result.Run->Combat->Battle.State.LastCombo->RecipeId==2);
        const auto announcement=Driver->FindElement(By::Id("battle.combo.result"))->GetText().ToString();
        TestTrue(TEXT("初発見・名前・参加者・理由を表示"),announcement.Contains(TEXT("初発見")) && announcement.Contains(TEXT("2人・隣接連携")) && announcement.Contains(TEXT("参加者")) && announcement.Contains(TEXT("成立理由")));
        if(!Click("save.suspend"))return;
        TestTrue(TEXT("タイトルに発見済み一覧"),Driver->FindElement(By::Id("title.combos"))->GetText().ToString().Contains(TEXT("2")));
        Async(EAsyncExecution::TaskGraphMainThread,[this,service]{Window->SetContent(SNew(SRunScreen).SaveService(service));}).Wait();DrainSlateLayout();
        if(!Click("title.resume"))return;
        TestTrue(TEXT("再開時は発見済み"),Driver->FindElement(By::Id("battle.combo.result"))->GetText().ToString().Contains(TEXT("発見済み")));
        TestTrue(TEXT("再開で再発見や乱数消費をしない"),service->Load().Permanent.DiscoveredCombos==std::vector<int>{2} && service->Load().Run->Combat->Battle.RandomState==result.Run->Combat->Battle.RandomState);
    });
    AfterEach([this] {
        Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}
        IAutomationDriverModule::Get().Disable();
    });
}
