#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/CombatScreen.h"
BEGIN_DEFINE_SPEC(FGirlsSpireNinjaEnemySpec,"GirlsSpire.UI.NinjaEnemy",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
END_DEFINE_SPEC(FGirlsSpireNinjaEnemySpec)
void FGirlsSpireNinjaEnemySpec::Define()
{
    BeforeEach([this] {
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        Window=SNew(SWindow).Title(FText::FromString(TEXT("Girls Spire — 骨忍テスト"))).ClientSize(FVector2D(1100,760));
        FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    for(int mode=0;mode<3;++mode)It(*FString::Printf(TEXT("骨忍の退避条件%dを実操作で検証"),mode),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(90),[this,mode] {
        using namespace GirlsSpire;
        BattleUnit hero;hero.At={1,1};hero.Speed=30;hero.Hp=hero.MaxHp=200;hero.Range=8;
        BattleUnit ninja;ninja.Id=100;ninja.Side=Faction::Enemy;ninja.Role=BattleRole::Ninja;
        ninja.At=mode==2?Position{4,1}:Position{2,1};ninja.Speed=20;ninja.Movement=4;ninja.Accuracy=200;
        std::vector<Tile> tiles(64);
        if(mode==1){ninja.Hp=10;for(int y=0;y<8;++y)for(int x=0;x<8;++x)if(Position{x,y}!=hero.At && Position{x,y}!=ninja.At)tiles[y*8+x].Trap=true;}
        auto battle=MakeShared<Battle>(8,8,tiles,std::vector<BattleUnit>{hero,ninja},42);
        Async(EAsyncExecution::TaskGraphMainThread,[this,battle]{Window->SetContent(SNew(SCombatScreen).Combat(battle));}).Wait();DrainSlateLayout();
        TestTrue(TEXT("骨忍の種類名を表示"),Driver->FindElement(By::Id("battle.target.100"))->GetText().ToString().Contains(TEXT("骨忍")));
        Driver->FindElement(By::Id("battle.scroll"))->ScrollToEndUntil(By::Id("battle.wait.south"));DrainSlateLayout();
        if(!TestTrue(TEXT("待機から敵行動を実行"),ClickAndDrain(Driver->FindElement(By::Id("battle.wait.south")))))return;
        const auto& state=battle->Snapshot();
        TestTrue(TEXT("敵の攻撃後に主人公へ手番が戻る"),state.ActiveId==0 && state.Units[0].Hp==190);
        const auto at=state.Units[1].At;
        if(mode==0)TestTrue(TEXT("未移動なら攻撃後に4マス退避"),std::abs(at.X-1)+std::abs(at.Y-1)==5 && state.Revision==4);
        if(mode==1)TestTrue(TEXT("致死罠へ退避しない"),at==Position{2,1} && state.Units[1].Hp==10 && state.Revision==3);
        if(mode==2)TestTrue(TEXT("接近してから攻撃した手番は再移動しない"),std::abs(at.X-1)+std::abs(at.Y-1)==1 && state.Revision==4);
    });
    AfterEach([this] {
        Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}
        IAutomationDriverModule::Get().Disable();
    });
}
