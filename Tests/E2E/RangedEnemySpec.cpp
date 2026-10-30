#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/CombatScreen.h"

BEGIN_DEFINE_SPEC(FGirlsSpireRangedEnemySpec,"GirlsSpire.UI.RangedEnemy",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
END_DEFINE_SPEC(FGirlsSpireRangedEnemySpec)
void FGirlsSpireRangedEnemySpec::Define()
{
    BeforeEach([this] {
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        Window=SNew(SWindow).Title(FText::FromString(TEXT("Girls Spire — 射撃敵テスト"))).ClientSize(FVector2D(1100,760));
        FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    for(const auto role:{GirlsSpire::BattleRole::Archer,GirlsSpire::BattleRole::Gunner})for(bool wall:{false,true})
    It(*FString::Printf(TEXT("射撃敵%dは%s射撃する"),static_cast<int>(role),wall?TEXT("壁を迂回して"):TEXT("その場で")),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(90),[this,role,wall] {
        using namespace GirlsSpire;
        BattleUnit hero;hero.At={5,3};hero.Speed=30;hero.Hp=hero.MaxHp=200;hero.Defense=0;hero.Evasion=0;hero.Range=8;
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.Role=role;enemy.At={1,3};enemy.Speed=20;enemy.Range=4;enemy.Movement=3;enemy.Accuracy=200;
        std::vector<Tile> tiles(64);tiles[3*8+3].Wall=wall;
        auto combat=MakeShared<Battle>(8,8,tiles,std::vector<BattleUnit>{hero,enemy},42);
        Async(EAsyncExecution::TaskGraphMainThread,[this,combat]{Window->SetContent(SNew(SCombatScreen).Combat(combat));}).Wait();DrainSlateLayout();
        // 壁ありでは主人公からも敵を直接狙えない。名前は射線のある場合に検証する。
        if(!wall)TestTrue(TEXT("敵の種類を表示"),Driver->FindElement(By::Id("battle.target.100"))->GetText().ToString().Contains(role==BattleRole::Archer?TEXT("弓兵亡者"):TEXT("鉄砲亡者")));
        Driver->FindElement(By::Id("battle.scroll"))->ScrollToEndUntil(By::Id("battle.wait.south"));DrainSlateLayout();
        if(!TestTrue(TEXT("マウスで待機して敵の手番へ"),ClickAndDrain(Driver->FindElement(By::Id("battle.wait.south")))))return;
        const auto& state=combat->Snapshot();const auto& foe=state.Units[1];
        TestTrue(TEXT("射線を得てから射撃"),combat->HasLineOfSight(foe.At,hero.At));
        TestTrue(TEXT("壁がなければ移動しない"),wall?foe.At!=enemy.At:foe.At==enemy.At);
        TestEqual(TEXT("実際の射撃ダメージ"),state.Units[0].Hp,188);
        TestEqual(TEXT("必要な行動だけを実行"),state.Revision,static_cast<std::uint64_t>(wall?4:3));
        TestEqual(TEXT("次は主人公の手番"),state.ActiveId,0);
    });
    AfterEach([this] {
        Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}
        IAutomationDriverModule::Get().Disable();
    });
}
