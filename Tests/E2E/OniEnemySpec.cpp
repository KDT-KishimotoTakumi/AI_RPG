#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/CombatScreen.h"
BEGIN_DEFINE_SPEC(FGirlsSpireOniEnemySpec,"GirlsSpire.UI.OniEnemy",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
END_DEFINE_SPEC(FGirlsSpireOniEnemySpec)
void FGirlsSpireOniEnemySpec::Define()
{
    BeforeEach([this] {
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        Window=SNew(SWindow).Title(FText::FromString(TEXT("Girls Spire — 鬼武者テスト"))).ClientSize(FVector2D(1100,760));
        FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    for(int mode=0;mode<3;++mode)It(*FString::Printf(TEXT("鬼武者の攻撃選択%dをマウス待機後に検証"),mode),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(90),[this,mode] {
        using namespace GirlsSpire;
        BattleUnit hero;hero.At={1,1};hero.Speed=30;hero.Hp=hero.MaxHp=200;hero.Range=8;
        if(mode==2)hero.Defense=100;
        BattleUnit oni;oni.Id=100;oni.Side=Faction::Enemy;oni.Role=BattleRole::Oni;oni.At=mode==1?Position{4,1}:Position{2,1};
        oni.Speed=20;oni.Movement=2;oni.Attack=16;oni.Skills[0]=true;oni.Accuracy=200;
        auto battle=MakeShared<Battle>(8,8,std::vector<Tile>(64),std::vector<BattleUnit>{hero,oni},42);
        Async(EAsyncExecution::TaskGraphMainThread,[this,battle]{Window->SetContent(SNew(SCombatScreen).Combat(battle));}).Wait();DrainSlateLayout();
        TestTrue(TEXT("鬼武者の名前を表示"),Driver->FindElement(By::Id("battle.target.100"))->GetText().ToString().Contains(TEXT("鬼武者")));
        Driver->FindElement(By::Id("battle.scroll"))->ScrollToEndUntil(By::Id("battle.wait.south"));DrainSlateLayout();
        if(!TestTrue(TEXT("待機して敵の判断を実行"),ClickAndDrain(Driver->FindElement(By::Id("battle.wait.south")))))return;
        const auto& state=battle->Snapshot();
        TestTrue(TEXT("予告が増える時だけ強撃の実ダメージ"),state.ActiveId==0 && state.Units[0].Hp==(mode==2?199:178));
        TestEqual(TEXT("強撃の再使用待ち"),state.Units[1].Cooldowns[0],mode==2?0:2);
        TestTrue(TEXT("移動と攻撃は各1回まで"),state.Revision==static_cast<std::uint64_t>(mode==1?4:3));
        if(mode==0) {
            if(!TestTrue(TEXT("次の手番へ待機"),ClickAndDrain(Driver->FindElement(By::Id("battle.wait.south")))))return;
            TestTrue(TEXT("クールダウン中は通常攻撃"),battle->Snapshot().Units[0].Hp==164 && battle->Snapshot().Units[1].Cooldowns[0]==1);
        }
    });
    AfterEach([this] {
        Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}
        IAutomationDriverModule::Get().Disable();
    });
}
