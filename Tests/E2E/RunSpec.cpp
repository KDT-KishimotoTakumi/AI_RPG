#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Widgets/SWindow.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#if defined(GIRLS_SPIRE_COVERAGE)
extern "C" int __llvm_profile_write_file();
#endif

BEGIN_DEFINE_SPEC(FGirlsSpireRunUiSpec, "GirlsSpire.UI.RunPreview",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
    int ScreenshotIndex = 0;
END_DEFINE_SPEC(FGirlsSpireRunUiSpec)

void FGirlsSpireRunUiSpec::Define()
{
    BeforeEach([this] {
        FSlateApplication::Get().CloseToolTip();
        // 新しいウィンドウの入力も最初から同じ模擬入力経路へ渡す。
        IAutomationDriverModule::Get().Enable();
        IConsoleManager::Get().ProcessUserConsoleInput(TEXT("GirlsSpire.Preview"), *GLog, nullptr);
        for (const auto& Candidate : FSlateApplication::Get().GetTopLevelWindows())
            if (Candidate->GetTitle().ToString() == TEXT("Girls Spire — 周回プレビュー")) Window = Candidate;
        Driver = IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    It(TEXT("マウスで12日を進め、結果からタイトルへ戻る"), EAsyncExecution::ThreadPool,
        FTimespan::FromSeconds(180), [this] {
        if (!TestTrue(TEXT("開始ボタンをクリック"), ClickAndDrain(Driver->FindElement(By::Id("title.start"))))) return;
        TestTrue(TEXT("上級を選択"), ClickAndDrain(Driver->FindElement(By::Id("difficulty.2"))));
        TestTrue(TEXT("軍配を選択"), ClickAndDrain(Driver->FindElement(By::Id("role.2"))));
        TestTrue(TEXT("周回を開始"), ClickAndDrain(Driver->FindElement(By::Id("role.confirm"))));
        int MidBosses = 0, FinalBosses = 0;
        for (int Event = 0; Event < 24; ++Event)
        {
            if (Driver->FindElement(By::Id("result.won"))->Exists()) break;
            if (!TestTrue(TEXT("経路を選択"), ClickAndDrain(Driver->FindElement(By::Id("route.node.0"))))) return;
            const auto Label = Driver->FindElement(By::Id("event.kind"))->GetText().ToString();
            if (Label.Contains(TEXT("落城姫"))) ++MidBosses;
            if (Label.Contains(TEXT("歴史喰らい"))) ++FinalBosses;
            if(Driver->FindElement(By::Id("rest.heal"))->Exists()) {
                if(!TestTrue(TEXT("休息を選択"),ClickAndDrain(Driver->FindElement(By::Id("rest.heal"))))) return;
                const auto scroll=Driver->FindElement(By::Id("run.scroll"));
                const bool found=scroll->ScrollToEndUntil(By::Id("rest.confirm"));Async(EAsyncExecution::TaskGraphMainThread,[]{}).Wait();
                if(!TestTrue(TEXT("休息確定までスクロール"),found) || !TestTrue(TEXT("休息を確定"),ClickAndDrain(Driver->FindElement(By::Id("rest.confirm"))))) return;
            } else if(Driver->FindElement(By::Id("shop.exit"))->Exists()) {
                const auto scroll=Driver->FindElement(By::Id("run.scroll"));
                scroll->ScrollToEndUntil(By::Id("shop.exit"));Async(EAsyncExecution::TaskGraphMainThread,[]{}).Wait();
                if(!TestTrue(TEXT("商店を退出"),ClickAndDrain(Driver->FindElement(By::Id("shop.exit"))))) return;
            } else if(Driver->FindElement(By::Id("social.help"))->Exists()) {
                const auto scroll=Driver->FindElement(By::Id("run.scroll"));
                for(int id=1;id<=8;++id) {
                    const auto name=FString::Printf(TEXT("social.actor.%d"),id);
                    if(!Driver->FindElement(By::Id(*name))->Exists()) continue;
                    scroll->ScrollToEndUntil(By::Id(*name));Async(EAsyncExecution::TaskGraphMainThread,[]{}).Wait();
                    if(!TestTrue(TEXT("交流相手"),ClickAndDrain(Driver->FindElement(By::Id(*name))))) return;
                    break;
                }
                for(const char* id:{"social.choice.1","social.confirm"}) {
                    scroll->ScrollToBeginning();scroll->ScrollToEndUntil(By::Id(id));Async(EAsyncExecution::TaskGraphMainThread,[]{}).Wait();
                    if(!TestTrue(TEXT("交流確定"),ClickAndDrain(Driver->FindElement(By::Id(id))))) return;
                }
                scroll->ScrollToBeginning();Async(EAsyncExecution::TaskGraphMainThread,[]{}).Wait();
            } else if (!TestTrue(TEXT("検証用の勝利入力"), ClickAndDrain(Driver->FindElement(By::Id("preview.complete"))))) return;
        }
        TestTrue(TEXT("周回成功を画面で確認"), Driver->FindElement(By::Id("result.won"))->Exists());
        TestEqual(TEXT("6日目の中ボスを経由"), MidBosses, 1);
        TestEqual(TEXT("12日目の最終ボスを経由"), FinalBosses, 1);
        TestTrue(TEXT("救出4人を画面で確認"), Driver->FindElement(By::Id("run.party"))->GetText().ToString().Contains(TEXT("仲間4人")));
        TestTrue(TEXT("タイトルへ戻る"), ClickAndDrain(Driver->FindElement(By::Id("result.continue"))));
        if(!TestTrue(TEXT("単独の後日談を選ぶ"),ClickAndDrain(Driver->FindElement(By::Id("epilogue.select.0")))))return;
        if(!TestTrue(TEXT("後日談を確定"),ClickAndDrain(Driver->FindElement(By::Id("epilogue.confirm")))))return;
        if(!TestTrue(TEXT("本からタイトルへ戻る"),ClickAndDrain(Driver->FindElement(By::Id("epilogue.back")))))return;
        TestTrue(TEXT("開始画面が復帰"), Driver->FindElement(By::Id("title.start"))->Exists());
    });
    It(TEXT("全滅の検証入力で敗北し再出発できる"), EAsyncExecution::ThreadPool, [this] {
        if (!TestTrue(TEXT("開始"), ClickAndDrain(Driver->FindElement(By::Id("title.start"))))) return;
        TestTrue(TEXT("選択を取り消す"), ClickAndDrain(Driver->FindElement(By::Id("setup.back"))));
        TestTrue(TEXT("再び開始"), ClickAndDrain(Driver->FindElement(By::Id("title.start"))));
        TestTrue(TEXT("初期設定で出発"), ClickAndDrain(Driver->FindElement(By::Id("role.confirm"))));
        TestTrue(TEXT("別の救出経路"), ClickAndDrain(Driver->FindElement(By::Id("route.node.1"))));
        const auto defeat=Driver->FindElement(By::Id("preview.defeat"));
        if(!defeat->IsVisible()) {
            Driver->FindElement(By::Id("run.scroll"))->ScrollToEndUntil(By::Id("preview.defeat"));
            DrainSlateLayout();
        }
        TestTrue(TEXT("全滅の検証入力"), ClickAndDrain(defeat));
        TestTrue(TEXT("敗北画面"), Driver->FindElement(By::Id("result.lost"))->Exists());
        TestTrue(TEXT("敗北後は経路選択不可"), !Driver->FindElement(By::Id("route.node.0"))->Exists());
        TestTrue(TEXT("タイトルに復帰"), ClickAndDrain(Driver->FindElement(By::Id("result.continue"))));
    });
    It(TEXT("全難易度と四武器を選び経路画面を表示する"), EAsyncExecution::ThreadPool, [this] {
        if (!TestTrue(TEXT("開始"), ClickAndDrain(Driver->FindElement(By::Id("title.start"))))) return;
        for (int Index = 0; Index < 3; ++Index)
            TestTrue(TEXT("難易度切替"), ClickAndDrain(Driver->FindElement(By::Id(*FString::Printf(TEXT("difficulty.%d"), Index)))));
        for (int Index = 0; Index < 4; ++Index)
            TestTrue(TEXT("武器切替"), ClickAndDrain(Driver->FindElement(By::Id(*FString::Printf(TEXT("role.%d"), Index)))));
        TestTrue(TEXT("出発"), ClickAndDrain(Driver->FindElement(By::Id("role.confirm"))));
        const auto Heading = Driver->FindElement(By::Id("run.day"))->GetText().ToString();
        TestTrue(TEXT("選択内容の表示"), Heading.Contains(TEXT("上級")) && Heading.Contains(TEXT("火縄銃")));
        TestTrue(TEXT("三つ目の経路も操作可能"), Driver->FindElement(By::Id("route.node.2"))->IsVisible());
    });
    AfterEach([this] {
        FSlateApplication::Get().CloseToolTip();
        if (Window.IsValid()) {
            TArray<FColor> Pixels;
            FIntVector Size;
            if (TestTrue(TEXT("画面キャプチャ"), FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(), Pixels, Size))) {
                TArray64<uint8> Png;
                FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
                const auto Folder = FPaths::ProjectSavedDir() / TEXT("Screenshots");
                IFileManager::Get().MakeDirectory(*Folder, true);
                TestTrue(TEXT("検証画像の保存"), FFileHelper::SaveArrayToFile(Png,
                    *(Folder / FString::Printf(TEXT("RunPreview-%d.png"), ++ScreenshotIndex))));
            }
        }
        Driver.Reset();
        IAutomationDriverModule::Get().Disable();
        if (Window.IsValid()) { FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef()); Window.Reset(); }
#if defined(GIRLS_SPIRE_COVERAGE)
        TestEqual(TEXT("UI計測の記録"), __llvm_profile_write_file(), 0);
#endif
    });
}
