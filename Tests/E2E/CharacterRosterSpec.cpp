#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/RunScreen.h"
#include "Presentation/CharacterPortraits.h"
BEGIN_DEFINE_SPEC(FGirlsSpireCharacterRosterSpec,"GirlsSpire.UI.CharacterRoster",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
END_DEFINE_SPEC(FGirlsSpireCharacterRosterSpec)
void FGirlsSpireCharacterRosterSpec::Define()
{
    BeforeEach([this] {
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        Window=SNew(SWindow).Title(FText::FromString(TEXT("Girls Spire — 救出候補テスト"))).ClientSize(FVector2D(1100,760));
        FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    for(int candidate=0;candidate<8;++candidate)It(*FString::Printf(TEXT("候補%dの名前と役割を見て救出経路を選ぶ"),candidate),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(90),[this,candidate] {
        using namespace GirlsSpire;
        const TCHAR* roles[]={TEXT("近接"),TEXT("舞姫"),TEXT("忍"),TEXT("近接"),TEXT("弓使い"),TEXT("鉄砲使い"),TEXT("近接"),TEXT("舞姫")};
        const int ranges[]={1,1,1,1,4,5,1,1};
        Run run(42,Difficulty::Beginner,HeroRole::Sword);auto checkpoint=run.Checkpoint();checkpoint.State.Choices={{NodeKind::Rescue,candidate}};
        const auto restored=Run::Restore(checkpoint,{});if(!TestTrue(TEXT("救出候補の状態を準備"),restored.has_value()))return;
        auto current=std::make_shared<Run>(*restored);auto expected=*current;expected.ChooseNode(expected.Snapshot().Revision,0);
        Async(EAsyncExecution::TaskGraphMainThread,[this,current]{Window->SetContent(SNew(SRunScreen).InitialRun(current).DevelopmentTools(false));}).Wait();DrainSlateLayout();
        const auto option=Driver->FindElement(By::Id("route.node.0"));
        const auto label=option->GetText().ToString();
        TestTrue(TEXT("救出前に固定名と役割を表示"),label.Contains(FCharacterPortraits::Name(candidate+1)) && label.Contains(roles[candidate]));
        TestTrue(TEXT("既存の移動力と射程を表示"),label.Contains(TEXT("移動3")) && label.Contains(FString::Printf(TEXT("射程%d"),ranges[candidate])));
        if(!option->IsVisible()){Driver->FindElement(By::Id("run.scroll"))->ScrollToEndUntil(By::Id("route.node.0"));DrainSlateLayout();}
        if(!TestTrue(TEXT("マウスで候補を選ぶ"),ClickAndDrain(option)))return;
        TestTrue(TEXT("選択後も同じ役割を表示"),Driver->FindElement(By::Id("event.kind"))->GetText().ToString()==label);
        TestTrue(TEXT("表示追加で通常の経路/RNG/恒久を変更しない"),current->Checkpoint()==expected.Checkpoint() && current->ProfileState()==expected.ProfileState());
    });
    AfterEach([this] {
        Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}
        IAutomationDriverModule::Get().Disable();
    });
}
