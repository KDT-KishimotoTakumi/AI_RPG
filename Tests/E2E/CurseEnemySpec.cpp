#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/CombatScreen.h"
BEGIN_DEFINE_SPEC(FGirlsSpireCurseEnemySpec,"GirlsSpire.UI.CurseEnemy",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
END_DEFINE_SPEC(FGirlsSpireCurseEnemySpec)
void FGirlsSpireCurseEnemySpec::Define()
{
    BeforeEach([this] {
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        Window=SNew(SWindow).Title(FText::FromString(TEXT("Girls Spire — 呪詛テスト"))).ClientSize(FVector2D(1100,760));
        FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    for(int mode=0;mode<3;++mode)It(*FString::Printf(TEXT("弱体後の攻撃%dをマウス操作で検証"),mode),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(90),[this,mode] {
        using namespace GirlsSpire;
        BattleUnit hero;hero.At={1,1};hero.Speed=30;hero.Hp=hero.MaxHp=200;hero.Range=8;hero.Skills[0]=true;hero.Accuracy=200;
        BattleUnit monk;monk.Id=100;monk.Side=Faction::Enemy;monk.Role=BattleRole::CurseMonk;monk.At=mode==2?Position{6,1}:Position{3,1};
        monk.Speed=20;monk.Movement=2;monk.Range=3;monk.Attack=10;monk.Hp=monk.MaxHp=200;monk.Accuracy=200;
        Battle initial(8,8,std::vector<Tile>(64),{hero,monk},42);auto saved=initial.Checkpoint();saved.State.Combos.Gauge=50;
        auto battle=MakeShared<Battle>(*Battle::Restore(saved));
        Async(EAsyncExecution::TaskGraphMainThread,[this,battle]{Window->SetContent(SNew(SCombatScreen).Combat(battle));}).Wait();DrainSlateLayout();
        const auto click=[&](const char* id){return TestTrue(TEXT("マウス操作"),ClickAndDrain(Driver->FindElement(By::Id(id))));};
        TestTrue(TEXT("僧の名前"),Driver->FindElement(By::Id("battle.target.100"))->GetText().ToString().Contains(TEXT("呪詛僧")));
        Driver->FindElement(By::Id("battle.scroll"))->ScrollToEndUntil(By::Id("battle.wait.south"));DrainSlateLayout();
        if(!click("battle.wait.south"))return;
        TestTrue(TEXT("通常命中で次の主人公手番に弱体"),battle->Snapshot().ActiveId==0 && battle->Snapshot().Units[0].AttackCursed && battle->Snapshot().Units[0].Hp==192);
        Driver->FindElement(By::Id("battle.cell.0.7"))->ScrollBy(5);DrainSlateLayout();
        TestTrue(TEXT("弱体を文字で表示"),Driver->FindElement(By::Id("battle.celllabel.1.1"))->GetText().ToString().Contains(TEXT("呪")));
        if(mode==1 && !click("battle.skill.0"))return;
        if(mode==2 && !click("battle.ultimate.select"))return;
        const auto at=battle->Snapshot().Units[1].At;
        const auto cell=FString::Printf(TEXT("battle.cell.%d.%d"),at.X,at.Y);
        if(!TestTrue(TEXT("対象をクリック"),ClickAndDrain(Driver->FindElement(By::Id(*cell)))))return;
        const char* confirm=mode==0?"battle.confirm":mode==1?"battle.skill.confirm":"battle.ultimate.confirm";
        Driver->FindElement(By::Id("battle.scroll"))->ScrollToEndUntil(By::Id(confirm));DrainSlateLayout();
        if(!click(confirm))return;
        TestTrue(TEXT("弱体中の通常/強撃/必殺の実ダメージ"),battle->Snapshot().Units[1].Hp==200-(mode==0?6:mode==1?14:26));
        TestTrue(TEXT("基礎能力値を変えない"),battle->Snapshot().Units[0].Attack==12);
    });
    AfterEach([this] {
        Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}
        IAutomationDriverModule::Get().Disable();
    });
}
