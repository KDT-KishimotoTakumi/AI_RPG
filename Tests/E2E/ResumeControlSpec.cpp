#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Widgets/SWindow.h"
#include "Presentation/CombatScreen.h"

BEGIN_DEFINE_SPEC(FGirlsSpireResumeControlSpec,"GirlsSpire.UI.ResumeControl",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
END_DEFINE_SPEC(FGirlsSpireResumeControlSpec)
void FGirlsSpireResumeControlSpec::Define()
{
    BeforeEach([this] {
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        IConsoleManager::Get().ProcessUserConsoleInput(TEXT("GirlsSpire.Preview"),*GLog,nullptr);
        for(const auto& w:FSlateApplication::Get().GetTopLevelWindows())
            if(w->GetTitle().ToString()==TEXT("Spiral RPG — 周回プレビュー")) Window=w;
        Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    It(TEXT("保存済み作戦・開始時信頼・仲間乱数を復元して操作する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this] {
        using namespace GirlsSpire;
        BattleUnit ally;ally.Id=1;ally.At={1,1};ally.Speed=20;
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={6,6};
        auto battle=MakeShared<Battle>(8,8,std::vector<Tile>(64),std::vector<BattleUnit>{ally,enemy},42);
        CombatControlCheckpoint saved;saved.Orders[1]={TacticalIntent::Escort,{},Position{4,1}};
        saved.Trust[1]=50;saved.ProfileDriven=true;saved.ChoiceRandomState=123456;
        Profile later;later.Bonds[0]=99;
        auto expected=*battle;Random expectedRandom(saved.ChoiceRandomState);
        const auto forecast=IntentForecast::Build(expected,RankIntentActions(expected,saved.Orders[1]),50);
        TestTrue(TEXT("候補準備"),forecast.Execute(expected,expectedRandom));
        TSharedPtr<SCombatScreen> screen;
        Async(EAsyncExecution::TaskGraphMainThread,[&] {
            Window->SetContent(SAssignNew(screen,SCombatScreen).Combat(battle).BondProfile(later).SavedControl(saved));
        }).Wait();
        TestTrue(TEXT("現在の絆で開始時信頼を上書きしない"),Driver->FindElement(By::Id("intent.trust"))->GetText().ToString().Contains(TEXT("50")));
        TestTrue(TEXT("保存済み地点"),Driver->FindElement(By::Id("intent.target"))->GetText().ToString().Contains(TEXT("4,1")));
        TestTrue(TEXT("信頼50の候補確率"),Driver->FindElement(By::Id("intent.forecast"))->GetText().ToString().Contains(TEXT("70%")));
        TestFalse(TEXT("製品信頼は手動変更不可"),Driver->FindElement(By::Id("intent.trust.75"))->Exists());
        CombatControlCheckpoint actual;
        Async(EAsyncExecution::TaskGraphMainThread,[&]{actual=screen->ControlCheckpoint();}).Wait();
        TestTrue(TEXT("表示だけで抽選を消費しない"),actual==saved);
        if(!TestTrue(TEXT("復元後に作戦実行"),ClickAndDrain(Driver->FindElement(By::Id("intent.execute"))))) return;
        Async(EAsyncExecution::TaskGraphMainThread,[&]{actual=screen->ControlCheckpoint();}).Wait();
        TestTrue(TEXT("仲間抽選の消費位置一致"),actual.ChoiceRandomState==expectedRandom.State());
        TestTrue(TEXT("実行結果の位置一致"),battle->Snapshot().Units[0].At==expected.Snapshot().Units[0].At);
    });
    It(TEXT("不正な作戦保存をエラー表示し行動させない"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this] {
        using namespace GirlsSpire;
        BattleUnit ally;ally.Id=1;ally.At={1,1};ally.Speed=20;
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={6,6};
        auto battle=MakeShared<Battle>(8,8,std::vector<Tile>(64),std::vector<BattleUnit>{ally,enemy},42);
        const auto before=battle->Checkpoint();CombatControlCheckpoint bad;bad.Trust[1]=101;
        Async(EAsyncExecution::TaskGraphMainThread,[&]{Window->SetContent(SNew(SCombatScreen).Combat(battle).SavedControl(bad));}).Wait();
        TestTrue(TEXT("復元失敗表示"),Driver->FindElement(By::Id("save.control.error"))->Exists());
        TestFalse(TEXT("行動操作を出さない"),Driver->FindElement(By::Id("intent.execute"))->Exists());
        TestTrue(TEXT("戦闘本体を変更しない"),battle->Checkpoint()==before);
    });
    AfterEach([this] {
        Driver.Reset();if(Window.IsValid()) FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());
        Window.Reset();IAutomationDriverModule::Get().Disable();
    });
}
