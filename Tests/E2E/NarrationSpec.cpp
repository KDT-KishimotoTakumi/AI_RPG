#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/RunScreen.h"
#include "Domain/NarrationText.h"
BEGIN_DEFINE_SPEC(FGirlsSpireNarrationSpec,"GirlsSpire.UI.Narration",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;TSharedPtr<SWindow> Window;
    FString Expected(std::string_view text){return FString(UTF8_TO_TCHAR(std::string(text).c_str()));}
    // 経路の先頭候補だけを選び、指定の必須ボスが経路に現れる直前まで進める。
    std::shared_ptr<GirlsSpire::Run> RunBeforeBoss(GirlsSpire::NodeKind boss){
        using namespace GirlsSpire;auto run=std::make_shared<Run>(42,Difficulty::Beginner,HeroRole::Sword,Profile{});
        for(int step=0;step<80;++step){
            const auto state=run->Snapshot();
            if(state.Phase==RunPhase::Route && state.Choices.size()==1 && state.Choices[0].Kind==boss)return run;
            if(state.Phase==RunPhase::Route){if(!run->ChooseNode(state.Revision,0))return nullptr;}
            else if(state.Phase==RunPhase::Event){if(!run->CompleteEvent(state.Revision))return nullptr;}
            else return nullptr;
        }
        return nullptr;
    }
END_DEFINE_SPEC(FGirlsSpireNarrationSpec)
void FGirlsSpireNarrationSpec::Define()
{
    BeforeEach([this]{FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();Window=SNew(SWindow).ClientSize(FVector2D(1200,1000));FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();});
    LatentBeforeEach(WaitForSlateLayout);
    It(TEXT("出陣の支度で承認済みの地の文を読め、開始すると経路には出ない"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(90),[this]{
        Async(EAsyncExecution::TaskGraphMainThread,[this]{Window->SetContent(SNew(SRunScreen));}).Wait();DrainSlateLayout();
        if(!TestTrue(TEXT("出陣の支度へ"),ClickAndDrain(Driver->FindElement(By::Id("title.start")))))return;
        TestEqual(TEXT("出陣の地の文が承認文面と一致"),Driver->FindElement(By::Id("run.narration.setup"))->GetText().ToString(),Expected(GirlsSpire::SetupNarration()));
        TestTrue(TEXT("出発操作は残る"),Driver->FindElement(By::Id("role.confirm"))->Exists());
        if(!TestTrue(TEXT("周回を開始"),ClickAndDrain(Driver->FindElement(By::Id("role.confirm")))))return;
        TestTrue(TEXT("1日目の経路に出陣の地の文は残らない"),!Driver->FindElement(By::Id("run.narration.setup"))->Exists());
        TestTrue(TEXT("1日目に6日目の地の文は出ない"),!Driver->FindElement(By::Id("run.narration.day6"))->Exists() && !Driver->FindElement(By::Id("run.narration.day12"))->Exists());
    });
    for(const bool final:{false,true})
    It(final?TEXT("12日目の城主戦で地の文を読め、進行は変わらない"):TEXT("6日目の落城姫戦で地の文を読め、進行は変わらない"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this,final]{
        using namespace GirlsSpire;
        auto run=RunBeforeBoss(final?NodeKind::FinalBoss:NodeKind::MidBoss);if(!TestTrue(TEXT("必須ボス直前まで進める"),bool(run)))return;
        auto expected=*run;const auto revision=run->Snapshot().Revision;
        const char* id=final?"run.narration.day12":"run.narration.day6";const char* other=final?"run.narration.day6":"run.narration.day12";
        Async(EAsyncExecution::TaskGraphMainThread,[this,run]{Window->SetContent(SNew(SRunScreen).InitialRun(run));}).Wait();DrainSlateLayout();
        TestTrue(TEXT("経路選択中は地の文を出さない"),!Driver->FindElement(By::Id(id))->Exists());
        TestTrue(TEXT("表示だけでは状態が変わらない"),run->Checkpoint()==expected.Checkpoint() && run->Snapshot().Revision==revision);
        if(!TestTrue(TEXT("ボスを選ぶ"),ClickAndDrain(Driver->FindElement(By::Id("route.node.0")))))return;
        expected.ChooseNode(revision,0);
        TestEqual(TEXT("日付に対応する地の文が承認文面と一致"),Driver->FindElement(By::Id(id))->GetText().ToString(),Expected(DayNarration(final?12:6)));
        TestTrue(TEXT("別の日の地の文は出ない"),!Driver->FindElement(By::Id(other))->Exists());
        TestTrue(TEXT("戦闘開始操作は残る"),Driver->FindElement(By::Id("preview.battle"))->Exists());
        TestTrue(TEXT("地の文は経路選択以外の状態を変えない"),run->Checkpoint()==expected.Checkpoint());
    });
    AfterEach([this]{Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}IAutomationDriverModule::Get().Disable();});
}
