#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/CombatScreen.h"
#include "../Fixtures/PersonalityCase.h"
BEGIN_DEFINE_SPEC(FGirlsSpirePersonalitySpec,"GirlsSpire.UI.Personality",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
END_DEFINE_SPEC(FGirlsSpirePersonalitySpec)
void FGirlsSpirePersonalitySpec::Define()
{
    BeforeEach([this]{FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();Window=SNew(SWindow).ClientSize(FVector2D(1200,1000));FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();});
    LatentBeforeEach(WaitForSlateLayout);
    for(bool enabled:{false,true})It(enabled?TEXT("新規戦闘は性格順位で予告・実行・保存"):TEXT("旧戦闘は従来順位で予告・実行・保存"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this,enabled]{
        using namespace GirlsSpire;const auto state=TestData::PersonalityCase(enabled);
        const auto restored=Battle::Restore(state.Combat->Battle);if(!TestTrue(TEXT("戦闘を復元"),restored.has_value()))return;
        auto combat=MakeShared<Battle>(*restored);auto expected=*restored;
        const auto control=state.Combat->Control;const auto saved=std::make_shared<std::optional<SavedCombat>>();
        Async(EAsyncExecution::TaskGraphMainThread,[this,combat,control,saved]{Window->SetContent(SNew(SCombatScreen).Combat(combat).SavedControl(control).OnCheckpoint([saved](const SavedCombat& value){*saved=value;return true;}));}).Wait();DrainSlateLayout();
        const Position target=enabled?Position{3,1}:Position{1,3};
        const auto forecast=Driver->FindElement(By::Id("intent.forecast"))->GetText().ToString();
        TestTrue(TEXT("確定予告は世代別の通常攻撃対象"),forecast.StartsWith(FString::Printf(TEXT("確定予定\n通常攻撃(%d,%d)"),target.X,target.Y)));
        TestTrue(TEXT("予告は全状態・乱数不変"),combat->Checkpoint()==state.Combat->Battle);
        const auto execute=Driver->FindElement(By::Id("intent.execute"));
        if(!execute->IsVisible()){Driver->FindElement(By::Id("battle.scroll"))->ScrollToEndUntil(By::Id("intent.execute"));DrainSlateLayout();}
        expected.Attack(expected.Snapshot().Revision,3,target);
        if(!TestTrue(TEXT("画面から確定予告を実行"),ClickAndDrain(execute)))return;
        TestTrue(TEXT("実効果・RNG・識別を保存"),saved->has_value() && saved->value().Battle==expected.Checkpoint() && combat->Checkpoint()==expected.Checkpoint());
        if(saved->has_value()){auto result=state;result.Combat=saved->value();const auto bytes=EncodeRunPayload(result);TestTrue(TEXT("全状態をRun v17で往復"),bytes && DecodeRunPayload(*bytes)==result);}
    });
    AfterEach([this]{Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}IAutomationDriverModule::Get().Disable();});
}
