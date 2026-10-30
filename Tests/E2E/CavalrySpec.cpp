#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/CombatScreen.h"
BEGIN_DEFINE_SPEC(FGirlsSpireCavalrySpec,"GirlsSpire.UI.Cavalry",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
END_DEFINE_SPEC(FGirlsSpireCavalrySpec)
void FGirlsSpireCavalrySpec::Define()
{
    BeforeEach([this] {
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        Window=SNew(SWindow).Title(FText::FromString(TEXT("Girls Spire — 突進テスト"))).ClientSize(FVector2D(1100,760));
        FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    for(int mode=0;mode<5;++mode)It(*FString::Printf(TEXT("騎馬の経路条件%dをマウス待機後の実行で検証"),mode),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(90),[this,mode] {
        using namespace GirlsSpire;
        const int steps=mode==0?0:mode==1?2:3;
        BattleUnit hero;hero.At={steps+2,1};hero.Speed=30;hero.Hp=hero.MaxHp=200;hero.Range=8;
        BattleUnit rider;rider.Id=100;rider.Side=Faction::Enemy;rider.Role=BattleRole::Cavalry;rider.At={1,1};rider.Movement=5;rider.Speed=20;rider.Attack=10;rider.Accuracy=200;
        std::vector<Tile> tiles(64);if(mode==3)tiles[8+2].Wall=true;if(mode==4)tiles[8+2].Trap=true;
        auto battle=MakeShared<Battle>(8,8,tiles,std::vector<BattleUnit>{hero,rider},42);
        Async(EAsyncExecution::TaskGraphMainThread,[this,battle]{Window->SetContent(SNew(SCombatScreen).Combat(battle));}).Wait();DrainSlateLayout();
        TestTrue(TEXT("盤面で騎馬を文字識別"),Driver->FindElement(By::Id("battle.celllabel.1.1"))->GetText().ToString().Contains(TEXT("敵騎")));
        if(mode!=3)TestTrue(TEXT("騎馬の種類名を表示"),Driver->FindElement(By::Id("battle.target.100"))->GetText().ToString().Contains(TEXT("騎馬亡者")));
        Driver->FindElement(By::Id("battle.scroll"))->ScrollToEndUntil(By::Id("battle.wait.south"));DrainSlateLayout();
        if(!TestTrue(TEXT("待機して敵行動を実行"),ClickAndDrain(Driver->FindElement(By::Id("battle.wait.south")))))return;
        const auto& state=battle->Snapshot();
        TestEqual(TEXT("直線3マスだけ突進加算"),state.Units[0].Hp,200-(mode==2?14:8));
        TestEqual(TEXT("移動と攻撃を必要回数だけ実行"),state.Revision,static_cast<std::uint64_t>(mode==0?3:4));
        TestTrue(TEXT("終了時に突進を残さない"),state.ActiveId==0 && !state.ChargeReady && state.Units[1].Attack==10);
        if(mode==4)TestTrue(TEXT("罠回避の共通経路を維持"),state.Tiles[8+2].Trap && state.Units[1].Hp==30);
    });
    AfterEach([this] {
        Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}
        IAutomationDriverModule::Get().Disable();
    });
}
