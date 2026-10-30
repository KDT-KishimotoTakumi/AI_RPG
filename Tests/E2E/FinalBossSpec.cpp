#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/CombatScreen.h"
BEGIN_DEFINE_SPEC(FGirlsSpireFinalBossSpec,"GirlsSpire.UI.FinalBoss",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
END_DEFINE_SPEC(FGirlsSpireFinalBossSpec)
void FGirlsSpireFinalBossSpec::Define()
{
    BeforeEach([this] {
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        Window=SNew(SWindow).Title(FText::FromString(TEXT("Girls Spire — 城主テスト"))).ClientSize(FVector2D(1100,760));
        FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    for(bool victory:{false,true})It(victory?TEXT("配下が残る城主撃破を実操作で検証"):TEXT("3ラウンド開始の召喚と通常行動を実操作で検証"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(90),[this,victory] {
        using namespace GirlsSpire;
        BattleUnit hero;hero.At={1,1};hero.Speed=20;hero.Hp=hero.MaxHp=5000;hero.Range=8;hero.Attack=1000;hero.Accuracy=200;
        BattleUnit lord;lord.Id=100;lord.Side=Faction::Enemy;lord.Role=BattleRole::CastleLord;lord.At={4,1};lord.Speed=15;lord.Movement=2;lord.Range=3;lord.Attack=22;lord.Defense=6;lord.Hp=victory?20:240;lord.MaxHp=240;lord.Accuracy=200;
        std::vector<BattleUnit> units{hero,lord};
        if(victory){BattleUnit soldier;soldier.Id=1000;soldier.Side=Faction::Enemy;soldier.At={7,7};units.push_back(soldier);}
        Battle initial(8,8,std::vector<Tile>(64),units,42,BattleGoal::DefeatLord);
        auto checkpoint=initial.Checkpoint();checkpoint.State.Round=3;checkpoint.State.Combos.Round=3;
        auto restored=Battle::Restore(checkpoint);if(!TestTrue(TEXT("城主の試験状態を復元"),restored.has_value()))return;
        auto battle=MakeShared<Battle>(*restored);
        Async(EAsyncExecution::TaskGraphMainThread,[this,battle]{Window->SetContent(SNew(SCombatScreen).Combat(battle));}).Wait();DrainSlateLayout();
        TestTrue(TEXT("城主の正式名称を表示"),Driver->FindElement(By::Id("battle.target.100"))->GetText().ToString().Contains(TEXT("歴史喰らいの城主")));
        TestTrue(TEXT("召喚と勝利条件を文字で表示"),Driver->FindElement(By::Id("battle.finalboss.100"))->GetText().ToString().Contains(TEXT("城主撃破で勝利")));
        if(victory) {
            if(!ClickAndDrain(Driver->FindElement(By::Id("battle.target.100"))))return;
            Driver->FindElement(By::Id("battle.scroll"))->ScrollToEndUntil(By::Id("battle.confirm"));DrainSlateLayout();
            TestTrue(TEXT("攻撃確定"),ClickAndDrain(Driver->FindElement(By::Id("battle.confirm"))));
            TestTrue(TEXT("配下が残っても勝利"),battle->Snapshot().Outcome==BattleOutcome::Won && battle->Snapshot().Units.back().Hp>0);
        } else {
            Driver->FindElement(By::Id("battle.scroll"))->ScrollToEndUntil(By::Id("battle.wait.south"));DrainSlateLayout();
            TestTrue(TEXT("待機から城主の手番を実行"),ClickAndDrain(Driver->FindElement(By::Id("battle.wait.south"))));
            TestTrue(TEXT("召喚兵1体と城主の通常攻撃"),battle->Snapshot().Units.size()==3 && battle->Snapshot().Units.back().SummonerId==100 && battle->Snapshot().Units[0].Hp<5000);
            TestTrue(TEXT("次の主人公の手番まで進行"),battle->Snapshot().ActiveId==0 && battle->Snapshot().Round==4);
        }
    });
    AfterEach([this] {
        Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}
        IAutomationDriverModule::Get().Disable();
    });
}
