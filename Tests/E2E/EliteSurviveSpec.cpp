#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/CombatScreen.h"
#include "Application/Encounter.h"
// FR-022: 精鋭戦の偶数日は生存戦になり、勝利条件を画面に示す。奇数日は従来どおり表示しない。
BEGIN_DEFINE_SPEC(FGirlsSpireEliteSurviveSpec,"GirlsSpire.UI.EliteSurvive",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
    TSharedRef<GirlsSpire::Battle> ShowElite(int day);
END_DEFINE_SPEC(FGirlsSpireEliteSurviveSpec)
TSharedRef<GirlsSpire::Battle> FGirlsSpireEliteSurviveSpec::ShowElite(int day)
{
    using namespace GirlsSpire;
    RunSnapshot run;run.Phase=RunPhase::Event;run.Level=Difficulty::Beginner;run.Role=HeroRole::Sword;
    run.Recruited={0,2};run.Day=day;run.Active={NodeKind::Elite,-1};
    auto combat=MakeShared<Battle>(Encounter::Create(run,0));
    Async(EAsyncExecution::TaskGraphMainThread,[this,combat]{Window->SetContent(SNew(SCombatScreen).Combat(combat));}).Wait();
    return combat;
}
void FGirlsSpireEliteSurviveSpec::Define()
{
    BeforeEach([this] {
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        Window=SNew(SWindow).Title(FText::FromString(TEXT("Girls Spire — 生存戦テスト"))).ClientSize(FVector2D(1100,760));
        FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    It(TEXT("偶数日の精鋭戦は生存戦として勝利条件を表示する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this] {
        using namespace GirlsSpire;
        const auto combat=ShowElite(6);
        TestTrue(TEXT("目的は生存"),combat->Checkpoint().Goal==BattleGoal::Survive);
        auto goal=Driver->FindElement(By::Id("battle.goal.survive"));
        if(!TestTrue(TEXT("勝利条件を表示"),goal->Exists()))return;
        const auto text=goal->GetText().ToString();
        TestTrue(TEXT("文言に生存戦と耐える回数を含む"),text.Contains(TEXT("生存戦")) && text.Contains(TEXT("第3ラウンド")));
        TestTrue(TEXT("現在ラウンドを示す"),text.Contains(TEXT("現在第1ラウンド")));
    });
    It(TEXT("奇数日の精鋭戦は全滅戦のままで生存の表示を出さない"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this] {
        using namespace GirlsSpire;
        const auto combat=ShowElite(5);
        TestTrue(TEXT("目的は全滅"),combat->Checkpoint().Goal==BattleGoal::Eliminate);
        TestFalse(TEXT("生存の表示なし"),Driver->FindElement(By::Id("battle.goal.survive"))->Exists());
        TestTrue(TEXT("通常の戦闘画面は表示"),Driver->FindElement(By::Id("battle.round"))->Exists());
    });
    AfterEach([this] {
        Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}
        IAutomationDriverModule::Get().Disable();
    });
}
