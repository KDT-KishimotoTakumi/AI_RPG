#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/CombatScreen.h"
BEGIN_DEFINE_SPEC(FGirlsSpireSpiderEnemySpec,"GirlsSpire.UI.SpiderEnemy",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
END_DEFINE_SPEC(FGirlsSpireSpiderEnemySpec)
void FGirlsSpireSpiderEnemySpec::Define()
{
    BeforeEach([this] {
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        Window=SNew(SWindow).Title(FText::FromString(TEXT("Girls Spire — 糸縛りテスト"))).ClientSize(FVector2D(1100,760));
        FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    for(int mode=0;mode<3;++mode)It(*FString::Printf(TEXT("糸縛り後の移動%dを実操作で検証"),mode),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(90),[this,mode] {
        using namespace GirlsSpire;
        BattleUnit hero;hero.At={1,1};hero.Speed=30;hero.Hp=hero.MaxHp=200;hero.Range=8;hero.Accuracy=200;
        if(mode==1)hero.Role=BattleRole::Dancer;
        if(mode==2){hero.Role=BattleRole::Ninja;hero.Movement=1;}
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.Role=BattleRole::Spider;enemy.At={3,1};enemy.Speed=20;enemy.Movement=2;enemy.Range=3;enemy.Attack=10;enemy.Hp=enemy.MaxHp=200;enemy.Accuracy=200;
        auto battle=MakeShared<Battle>(8,8,std::vector<Tile>(64),std::vector<BattleUnit>{hero,enemy},42);
        Async(EAsyncExecution::TaskGraphMainThread,[this,battle]{Window->SetContent(SNew(SCombatScreen).Combat(battle));}).Wait();DrainSlateLayout();
        const auto click=[&](const char* id){return TestTrue(TEXT("糸縛りのマウス操作"),ClickAndDrain(Driver->FindElement(By::Id(id))));};
        TestTrue(TEXT("絡新婦の種類名"),Driver->FindElement(By::Id("battle.target.100"))->GetText().ToString().Contains(TEXT("絡新婦")));
        Driver->FindElement(By::Id("battle.scroll"))->ScrollToEndUntil(By::Id("battle.wait.south"));DrainSlateLayout();
        if(!click("battle.wait.south"))return;
        TestTrue(TEXT("命中で移動力を低下"),battle->Snapshot().Units[0].MovementWebbed && battle->MovementAllowance(0)==(mode==2?0:1));
        Driver->FindElement(By::Id("battle.cell.0.7"))->ScrollBy(5);DrainSlateLayout();
        TestTrue(TEXT("糸縛りを文字表示"),Driver->FindElement(By::Id("battle.celllabel.1.1"))->GetText().ToString().Contains(TEXT("糸")));
        if(mode==1) {
            if(!click("battle.cell.3.1"))return;
            Driver->FindElement(By::Id("battle.scroll"))->ScrollToEndUntil(By::Id("battle.confirm"));DrainSlateLayout();
            if(!click("battle.confirm"))return;
            TestTrue(TEXT("移動弱体は攻撃力を変えない"),battle->Snapshot().Units[1].Hp==190);
            Driver->FindElement(By::Id("battle.cell.0.7"))->ScrollBy(5);DrainSlateLayout();
        }
        if(!click("battle.cell.1.3"))return;
        TestFalse(TEXT("弱体範囲外の2マス移動は確定できない"),Driver->FindElement(By::Id("battle.move.confirm"))->Exists());
        if(!click("battle.cell.1.2"))return;
        if(mode==2){TestFalse(TEXT("移動力0では1マスも確定できない"),Driver->FindElement(By::Id("battle.move.confirm"))->Exists());return;}
        Driver->FindElement(By::Id("battle.scroll"))->ScrollToEndUntil(By::Id("battle.move.confirm"));DrainSlateLayout();
        if(!click("battle.move.confirm"))return;
        TestTrue(TEXT("通常/行動後に弱体内の移動を実行"),battle->Snapshot().Units[0].At==Position{1,2} && battle->Snapshot().Units[0].Movement==3 && battle->Snapshot().Units[0].MovementWebbed);
    });
    AfterEach([this] {
        Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}
        IAutomationDriverModule::Get().Disable();
    });
}
