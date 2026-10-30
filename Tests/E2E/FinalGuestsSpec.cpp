#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/CombatScreen.h"
#include "Presentation/CharacterPortraits.h"
#include "Application/Encounter.h"
BEGIN_DEFINE_SPEC(FGirlsSpireFinalGuestsSpec,"GirlsSpire.UI.FinalGuests",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
END_DEFINE_SPEC(FGirlsSpireFinalGuestsSpec)
void FGirlsSpireFinalGuestsSpec::Define()
{
    BeforeEach([this] {
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        Window=SNew(SWindow).Title(FText::FromString(TEXT("Girls Spire — 未救出者テスト"))).ClientSize(FVector2D(1100,760));
        FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    for(int team:{0,4})It(*FString::Printf(TEXT("加入組%dの全員表示と城主撃破"),team),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(90),[this,team] {
        using namespace GirlsSpire;
        RunSnapshot run;run.Day=12;run.Phase=RunPhase::Event;run.Active={NodeKind::FinalBoss};run.Recruited={team,team+1,team+2,team+3};
        auto checkpoint=Encounter::Create(run,42).Checkpoint();checkpoint.State.Tiles.assign(checkpoint.State.Tiles.size(),Tile{});
        for(auto& unit:checkpoint.State.Units) {
            if(unit.Id==0){unit.Range=24;unit.Attack=1000;unit.Accuracy=200;}
            if(unit.Id==100){unit.At={3,1};unit.Hp=1;}
        }
        const auto restored=Battle::Restore(checkpoint);if(!TestTrue(TEXT("編成の準備"),restored.has_value()))return;
        auto battle=MakeShared<Battle>(*restored);
        Async(EAsyncExecution::TaskGraphMainThread,[this,battle]{Window->SetContent(SNew(SCombatScreen).Combat(battle));}).Wait();DrainSlateLayout();
        for(const auto& unit:battle->Snapshot().Units)if(unit.Id>=0 && unit.Id<=8) {
            TestEqual(TEXT("陣営を問わず固定名"),Driver->FindElement(By::Id(*FString::Printf(TEXT("battle.portrait.name.%d"),unit.Id)))->GetText().ToString(),FString(FCharacterPortraits::Name(unit.Id)));
            TestTrue(TEXT("実立ち絵を表示"),Driver->FindElement(By::Id(*FString::Printf(TEXT("battle.portrait.image.%d"),unit.Id)))->Exists());
            TestEqual(TEXT("陣営を文字で区別"),Driver->FindElement(By::Id(*FString::Printf(TEXT("battle.portrait.faction.%d"),unit.Id)))->GetText().ToString(),FString(unit.Side==Faction::Player?TEXT("味方"):unit.Side==Faction::Enemy?TEXT("敵"):TEXT("中立・手番なし")));
        }
        const auto before=battle->Snapshot().Units;
        if(!ClickAndDrain(Driver->FindElement(By::Id("battle.target.100"))))return;
        Driver->FindElement(By::Id("battle.scroll"))->ScrollToEndUntil(By::Id("battle.confirm"));DrainSlateLayout();
        TestTrue(TEXT("城主へ攻撃確定"),ClickAndDrain(Driver->FindElement(By::Id("battle.confirm"))));
        TestTrue(TEXT("未救出者が存命のまま勝利"),battle->Snapshot().Outcome==BattleOutcome::Won);
        for(std::size_t i=0;i<before.size();++i)if(before[i].Id!=100)TestTrue(TEXT("他人物の状態を変えない"),battle->Snapshot().Units[i]==before[i]);
    });
    AfterEach([this] {
        Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}
        IAutomationDriverModule::Get().Disable();
    });
}
