#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/CombatScreen.h"
BEGIN_DEFINE_SPEC(FGirlsSpireMidBossSpec,"GirlsSpire.UI.MidBoss",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
END_DEFINE_SPEC(FGirlsSpireMidBossSpec)
void FGirlsSpireMidBossSpec::Define()
{
    BeforeEach([this] {
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        Window=SNew(SWindow).Title(FText::FromString(TEXT("Girls Spire — 落城姫テスト"))).ClientSize(FVector2D(1100,760));
        FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    for(int hp:{81,80,79})It(*FString::Printf(TEXT("落城姫HP%dの攻撃段階を実操作で検証"),hp),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(90),[this,hp] {
        using namespace GirlsSpire;
        BattleUnit hero;hero.At={1,1};hero.Speed=30;hero.Hp=hero.MaxHp=200;hero.Range=8;
        auto ally=hero;ally.Id=1;ally.At={1,2};ally.Speed=0;
        BattleUnit boss;boss.Id=100;boss.Side=Faction::Enemy;boss.Role=BattleRole::FallenPrincess;boss.At={3,1};boss.Speed=12;boss.Movement=2;boss.Range=2;boss.Attack=18;boss.Defense=4;boss.Hp=hp;boss.MaxHp=160;boss.Accuracy=200;
        auto battle=MakeShared<Battle>(8,8,std::vector<Tile>(64),std::vector<BattleUnit>{hero,ally,boss},42);
        Async(EAsyncExecution::TaskGraphMainThread,[this,battle]{Window->SetContent(SNew(SCombatScreen).Combat(battle));}).Wait();DrainSlateLayout();
        TestTrue(TEXT("落城姫の名前"),Driver->FindElement(By::Id("battle.target.100"))->GetText().ToString().Contains(TEXT("落城姫")));
        TestTrue(TEXT("段階を文字表示"),Driver->FindElement(By::Id("battle.midboss.100"))->GetText().ToString().Contains(hp<=80?TEXT("通常範囲攻撃"):TEXT("通常単体攻撃")));
        Driver->FindElement(By::Id("battle.scroll"))->ScrollToEndUntil(By::Id("battle.wait.south"));DrainSlateLayout();
        if(!TestTrue(TEXT("マウス待機からボスを実行"),ClickAndDrain(Driver->FindElement(By::Id("battle.wait.south")))))return;
        TestEqual(TEXT("主対象への実ダメージ"),battle->Snapshot().Units[0].Hp,184);
        TestEqual(TEXT("半分以下で隣接対象にも実ダメージ"),battle->Snapshot().Units[1].Hp,hp<=80?184:200);
        TestTrue(TEXT("攻撃1回・追加手番なし"),battle->Snapshot().Revision==3 && battle->Snapshot().ActiveId==1 && battle->Snapshot().Units[2].Attack==18);
    });
    AfterEach([this] {
        Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}
        IAutomationDriverModule::Get().Disable();
    });
}
