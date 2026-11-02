#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Widgets/SWindow.h"
#include "Presentation/RunScreen.h"
#include "Domain/RelationshipText.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include <algorithm>

BEGIN_DEFINE_SPEC(FGirlsSpireStorySpec,"GirlsSpire.UI.Story",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
    int Page=0;
    bool Click(const char* id);
END_DEFINE_SPEC(FGirlsSpireStorySpec)
bool FGirlsSpireStorySpec::Click(const char* id)
{
    const auto target=Driver->FindElement(By::Id(id));
    if(!target->IsVisible()) {
        const auto scroll=Driver->FindElement(By::Id("run.scroll"));scroll->ScrollToBeginning();Async(EAsyncExecution::TaskGraphMainThread,[]{}).Wait();
        if(!target->IsVisible()){scroll->ScrollToEndUntil(By::Id(id));Async(EAsyncExecution::TaskGraphMainThread,[]{}).Wait();}
    }
    return TestTrue(*FString::Printf(TEXT("後日談操作 %hs"),id),ClickAndDrain(target));
}
void FGirlsSpireStorySpec::Define()
{
    BeforeEach([this] {
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        IConsoleManager::Get().ProcessUserConsoleInput(TEXT("GirlsSpire.Preview"),*GLog,nullptr);
        for(const auto& w:FSlateApplication::Get().GetTopLevelWindows())if(w->GetTitle().ToString()==TEXT("Girls Spire — 周回プレビュー"))Window=w;
        Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    for(int actor=0;actor<=8;++actor)It(*FString::Printf(TEXT("後日談%dを選択・取消・記録し本から再読する"),actor),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this,actor] {
        using namespace GirlsSpire;
        std::shared_ptr<Run> won;
        for(int seed=0;seed<100 && !won;++seed) {
            Profile profile;profile.Bonds.fill(80);profile.SpecialConversations.fill(true);
            auto run=std::make_shared<Run>(seed,Difficulty::Beginner,HeroRole::Sword,profile);
            for(int step=0;step<24 && run->Snapshot().Phase==RunPhase::Route;++step) {
                run->ChooseNode(run->Snapshot().Revision,0);run->CompleteEvent(run->Snapshot().Revision);
            }
            const auto choices=run->EpilogueChoices();if(std::find(choices.begin(),choices.end(),actor)!=choices.end())won=run;
        }
        if(!TestTrue(TEXT("条件を満たす勝利を準備"),won!=nullptr))return;
        Async(EAsyncExecution::TaskGraphMainThread,[this,won]{Window->SetContent(SNew(SRunScreen).InitialRun(won));}).Wait();
        if(!Click("result.continue"))return;
        const auto choices=won->EpilogueChoices();
        for(int id=0;id<=8;++id) {
            const auto key=FString::Printf(TEXT("epilogue.select.%d"),id);
            TestTrue(TEXT("全条件達成者と単独だけを表示"),Driver->FindElement(By::Id(*key))->Exists()==(std::find(choices.begin(),choices.end(),id)!=choices.end()));
        }
        const auto key=FString::Printf(TEXT("epilogue.select.%d"),actor);
        if(!TestTrue(TEXT("後日談の選択操作がある"),Driver->FindElement(By::Id(*key))->Exists()))return;
        if(!Click(TCHAR_TO_UTF8(*key)))return;
        TestTrue(TEXT("本のページを予告"),Driver->FindElement(By::Id("epilogue.page"))->Exists());
        const auto* text=FindEndingText(actor);if(!TestNotNull(TEXT("承認済み後日談"),text))return;
        TestEqual(TEXT("正式題名を予告"),Driver->FindElement(By::Id("epilogue.title"))->GetText().ToString(),FString(UTF8_TO_TCHAR(text->Title.data())));
        TestEqual(TEXT("正式本文を予告"),Driver->FindElement(By::Id("epilogue.page"))->GetText().ToString(),FString(UTF8_TO_TCHAR(text->Page.data())));
        if(!Click("epilogue.cancel"))return;
        TestFalse(TEXT("取消では記録しない"),won->ProfileState().Epilogues[actor]);
        if(!Click(TCHAR_TO_UTF8(*key)) || !Click("epilogue.confirm") || !Click("epilogue.back"))return;
        TestTrue(TEXT("周回終了後はタイトル"),Driver->FindElement(By::Id("title.start"))->Exists());
        if(!Click("title.epilogues"))return;
        const auto record=FString::Printf(TEXT("epilogue.record.%d"),actor);
        if(!Click(TCHAR_TO_UTF8(*record)))return;
        if(!Click("epilogue.library") || !Click(TCHAR_TO_UTF8(*record)))return;
        TestTrue(TEXT("再読では確定操作なし"),Driver->FindElement(By::Id("epilogue.page"))->Exists() && !Driver->FindElement(By::Id("epilogue.confirm"))->Exists());
        TestEqual(TEXT("再読も同一の正式本文"),Driver->FindElement(By::Id("epilogue.page"))->GetText().ToString(),FString(UTF8_TO_TCHAR(text->Page.data())));
    });
    AfterEach([this] {
        if(Window.IsValid()) {
            TArray<FColor> pixels;FIntVector size;
            if(FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),pixels,size)) {
                TArray64<uint8> png;FImageUtils::PNGCompressImageArray(size.X,size.Y,pixels,png);
                const auto folder=FPaths::ProjectSavedDir()/TEXT("Screenshots");IFileManager::Get().MakeDirectory(*folder,true);
                FFileHelper::SaveArrayToFile(png,*(folder/FString::Printf(TEXT("Epilogue-%d.png"),Page++)));
            }
        }
        Driver.Reset();IAutomationDriverModule::Get().Disable();
        if(Window.IsValid()){FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}
    });
}
