#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/CombatScreen.h"
BEGIN_DEFINE_SPEC(FGirlsSpireEnemyCoverSpec,"GirlsSpire.UI.EnemyCover",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
    bool Click(const char* id);
END_DEFINE_SPEC(FGirlsSpireEnemyCoverSpec)
bool FGirlsSpireEnemyCoverSpec::Click(const char* id)
{
    auto target=Driver->FindElement(By::Id(id));
    if(!target->IsVisible()) {
        auto scroll=Driver->FindElement(By::Id("battle.scroll"));scroll->ScrollToBeginning();DrainSlateLayout();
        if(!target->IsVisible())scroll->ScrollToEndUntil(By::Id(id));
    }
    return TestTrue(*FString::Printf(TEXT("遮蔽操作 %hs"),id),ClickAndDrain(target));
}
void FGirlsSpireEnemyCoverSpec::Define()
{
    BeforeEach([this] {
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        Window=SNew(SWindow).Title(FText::FromString(TEXT("Girls Spire — 遮蔽テスト"))).ClientSize(FVector2D(1100,760));
        FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    It(TEXT("生成した遮蔽が射線を遮り通常行動と破壊を維持する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this] {
        using namespace GirlsSpire;
        BattleUnit hero;hero.Id=0;hero.At={3,3};hero.Hp=hero.MaxHp=200;hero.Speed=20;hero.Attack=20;hero.Range=3;
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={4,3};enemy.Hp=enemy.MaxHp=200;enemy.CreatesCover=true;enemy.Defense=8;
        auto combat=MakeShared<Battle>(8,8,std::vector<Tile>(64),std::vector<BattleUnit>{hero,enemy},42);
        Async(EAsyncExecution::TaskGraphMainThread,[this,combat]{Window->SetContent(SNew(SCombatScreen).Combat(combat));}).Wait();
        TestTrue(TEXT("特性の説明を表示"),Driver->FindElement(By::Id("battle.cover.rule.100"))->Exists());
        if(!Click("battle.wait.south") || !Click("battle.wait.south"))return;
        if(!TestTrue(TEXT("偶数ラウンドに生成"),combat->Snapshot().Tiles[2*8+4].ObstacleHp==20))return;
        TestTrue(TEXT("通常行動も実行"),combat->Snapshot().Revision==6);
        TestTrue(TEXT("射線が変わる"),!combat->HasLineOfSight({4,0},{4,3}));
        TestTrue(TEXT("地形を画面へ表示"),Driver->FindElement(By::Id("battle.celllabel.4.2"))->GetText().ToString().Contains(TEXT("障20")));
        if(!Click("battle.cell.4.2") || !Click("battle.confirm"))return;
        TestTrue(TEXT("通常攻撃で破壊"),combat->Snapshot().Tiles[2*8+4].ObstacleHp==0 && combat->Snapshot().Acted);
    });
    AfterEach([this] {
        Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}
        IAutomationDriverModule::Get().Disable();
    });
}
