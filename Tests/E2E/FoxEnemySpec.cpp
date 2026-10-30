#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/CombatScreen.h"
BEGIN_DEFINE_SPEC(FGirlsSpireFoxEnemySpec,"GirlsSpire.UI.FoxEnemy",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
END_DEFINE_SPEC(FGirlsSpireFoxEnemySpec)
void FGirlsSpireFoxEnemySpec::Define()
{
    BeforeEach([this] {
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        Window=SNew(SWindow).Title(FText::FromString(TEXT("Girls Spire — 妖狐テスト"))).ClientSize(FVector2D(1100,760));
        FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    for(int mode=0;mode<3;++mode)It(*FString::Printf(TEXT("妖狐の回復選択%dを実操作で検証"),mode),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(90),[this,mode] {
        using namespace GirlsSpire;
        BattleUnit hero;hero.At={1,1};hero.Speed=30;hero.Hp=hero.MaxHp=200;hero.Range=8;
        BattleUnit fox;fox.Id=100;fox.Side=Faction::Enemy;fox.Role=BattleRole::Fox;fox.At={3,1};fox.Speed=20;fox.Movement=3;fox.Range=3;fox.Attack=8;fox.Skills[1]=true;fox.Accuracy=200;fox.MaxHp=100;fox.Hp=mode==1?50:mode==2?51:100;
        BattleUnit ally;ally.Id=101;ally.Side=Faction::Enemy;ally.At={4,1};ally.Speed=0;ally.Movement=0;ally.Hp=mode==0?50:mode==2?51:100;ally.MaxHp=100;
        auto battle=MakeShared<Battle>(8,8,std::vector<Tile>(64),std::vector<BattleUnit>{hero,fox,ally},42);
        Async(EAsyncExecution::TaskGraphMainThread,[this,battle]{Window->SetContent(SNew(SCombatScreen).Combat(battle));}).Wait();DrainSlateLayout();
        TestTrue(TEXT("妖狐の名前を表示"),Driver->FindElement(By::Id("battle.target.100"))->GetText().ToString().Contains(TEXT("妖狐")));
        Driver->FindElement(By::Id("battle.scroll"))->ScrollToEndUntil(By::Id("battle.wait.south"));DrainSlateLayout();
        if(!TestTrue(TEXT("待機から敵AIを実行"),ClickAndDrain(Driver->FindElement(By::Id("battle.wait.south")))))return;
        const auto& state=battle->Snapshot();
        TestTrue(TEXT("回復時は攻撃せず、51%なら射撃"),state.ActiveId==0 && state.Units[0].Hp==(mode==2?194:200));
        TestEqual(TEXT("自己回復の反映"),state.Units[1].Hp,mode==1?70:mode==2?51:100);
        TestEqual(TEXT("友軍回復の反映"),state.Units[2].Hp,mode==0?70:mode==2?51:100);
        TestEqual(TEXT("応急手当の再使用待ち"),state.Units[1].Cooldowns[1],mode==2?0:2);
        if(mode!=2)TestTrue(TEXT("回復結果を文字表示"),Driver->FindElement(By::Id("battle.log"))->GetText().ToString().Contains(TEXT("20回復")));
    });
    AfterEach([this] {
        Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}
        IAutomationDriverModule::Get().Disable();
    });
}
