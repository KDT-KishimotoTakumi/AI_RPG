#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/RunScreen.h"
#include "Presentation/GameEntry.h"
#include "Infrastructure/DiskSlotFiles.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"

BEGIN_DEFINE_SPEC(FGirlsSpireExitSpec,"GirlsSpire.UI.Exit",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
    int Exits=0;
    std::shared_ptr<GirlsSpire::DiskSlotFiles> Files;
    std::shared_ptr<GirlsSpire::SaveTransaction> Saves;
END_DEFINE_SPEC(FGirlsSpireExitSpec)
void FGirlsSpireExitSpec::Define()
{
    BeforeEach([this]{Exits=0;FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();Driver=IAutomationDriverModule::Get().CreateDriver();Window=SNew(SWindow).ClientSize(FVector2D(1280,900));FSlateApplication::Get().AddWindow(Window.ToSharedRef());const auto root=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Tests/Exit")/FGuid::NewGuid().ToString(EGuidFormats::Digits));Files=std::make_shared<GirlsSpire::DiskSlotFiles>(std::filesystem::path(*root));Saves=std::make_shared<GirlsSpire::SaveTransaction>(*Files,*Files);});
    LatentBeforeEach(WaitForSlateLayout);
    for(bool error:{false,true})for(bool bound:{false,true})It(*FString::Printf(TEXT("%sで終了先%sのマウス操作"),error?TEXT("読込エラー"):TEXT("タイトル"),bound?TEXT("あり"):TEXT("なし")),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(90),[this,bound,error]{
        if(error && !TestTrue(TEXT("専用の破損原本を準備"),Files->ReplaceJournal("broken journal")))return;
        const auto journal=Files->ReadJournal().Bytes;
        Async(EAsyncExecution::TaskGraphMainThread,[this,bound]{
            const auto exit=bound?FSimpleDelegate::CreateLambda([this]{++Exits;}):FSimpleDelegate{};
            Window->SetContent(SNew(SRunScreen).SaveService(Saves).DevelopmentTools(false).OnExitGame(exit));
        }).Wait();DrainSlateLayout();
        const auto button=Driver->FindElement(By::Id("title.quit"));
        if(!TestTrue(TEXT("終了先がある場合だけ終了操作を提供"),button->Exists()==bound))return;
        TestEqual(TEXT("表示だけでは終了しない"),Exits,0);
        TestTrue(TEXT("原本を変更せず終了を提供"),Files->ReadJournal().Bytes==journal && !Saves->HasPending());
        if(bound){
            if(!button->IsVisible()){Driver->FindElement(By::Id("run.scroll"))->ScrollToEndUntil(By::Id("title.quit"));DrainSlateLayout();}
            TestTrue(TEXT("終了を実マウス操作"),ClickAndDrain(button));
            TestEqual(TEXT("終了要求を一度だけ通知"),Exits,1);
            TestTrue(TEXT("終了要求で保存原本を変更しない"),Files->ReadJournal().Bytes==journal && !Saves->HasPending());
        }
    });
    It(TEXT("実行中の単独ゲームだけに終了先を与える"),[this]{
        for(bool game:{false,true})for(bool editor:{false,true})
            TestTrue(TEXT("PIE・エディタ・プレビューを終了しない"),GirlsSpire::GameExitAction(game,editor).IsBound()==(game && !editor));
    });
    AfterEach([this]{Driver.Reset();if(Window){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}Saves.reset();Files.reset();IAutomationDriverModule::Get().Disable();});
}
