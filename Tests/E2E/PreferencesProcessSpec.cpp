#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Presentation/RunScreen.h"
#include "Infrastructure/DiskSlotFiles.h"
BEGIN_DEFINE_SPEC(FGirlsSpirePreferencesProcessSpec,"GirlsSpire.UI.SaveProcess.Preferences",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;TSharedPtr<SWindow> Window;
    std::shared_ptr<GirlsSpire::DiskSlotFiles> Files;
    std::shared_ptr<GirlsSpire::PreferencesStore> Store;
END_DEFINE_SPEC(FGirlsSpirePreferencesProcessSpec)
void FGirlsSpirePreferencesProcessSpec::Define()
{
    FString stage,root;if(!FParse::Value(FCommandLine::Get(),TEXT("GirlsSpireResumeStage="),stage))return;
    FParse::Value(FCommandLine::Get(),TEXT("GirlsSpireResumeRoot="),root);root=FPaths::ConvertRelativePathToFull(root);
    const auto prefix=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Tests/ProcessResume"));
    const bool valid=root!=prefix && FPaths::IsUnderDirectory(root,prefix);
    BeforeEach([this]{FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();Window=SNew(SWindow).ClientSize(FVector2D(1200,1000));FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();});
    LatentBeforeEach(WaitForSlateLayout);
    It(TEXT("独立設定を別プロセスで読込み実画面へ反映する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this,stage,root,valid]{
        using namespace GirlsSpire;if(!TestTrue(TEXT("隔離先と段階を検証"),valid && (stage==TEXT("Save") || stage==TEXT("Verify"))))return;
        Files=std::make_shared<DiskSlotFiles>(std::filesystem::path(*(root/TEXT("preferences"))));Store=std::make_shared<PreferencesStore>(*Files);
        const Preferences expected{TextSize::Largest,0,37,false,true,false};
        if(stage==TEXT("Save")){TestTrue(TEXT("独立設定ファイルを確定"),Store->Save(expected)==PreferencesIssue::None);return;}
        const auto before=Files->ReadPreferences().Bytes;
        if(!TestTrue(TEXT("前プロセスの設定を完全復元"),Store->Load().Issue==PreferencesIssue::None && Store->Load().Value==expected))return;
        Async(EAsyncExecution::TaskGraphMainThread,[this]{Window->SetContent(SNew(SRunScreen).PreferencesService(Store).DevelopmentTools(false));}).Wait();DrainSlateLayout();
        const auto scroll=Driver->FindElement(By::Id("run.scroll"));scroll->ScrollToBeginning();DrainSlateLayout();scroll->ScrollToEndUntil(By::Id("title.settings"));DrainSlateLayout();
        if(!TestTrue(TEXT("再起動後の設定をマウスで開く"),ClickAndDrain(Driver->FindElement(By::Id("title.settings")))))return;
        if(!TestTrue(TEXT("再起動後も文字サイズ/独立音量/全フラグ一致"),Driver->FindElement(By::Id("settings.text.value"))->GetText().ToString().Contains(TEXT("150%")) && Files->ReadPreferences().Bytes==before && Store->Load().Value==expected))return;
        TestTrue(TEXT("設定再起動の成功記録"),FFileHelper::SaveStringToFile(TEXT("pass"),*(root/TEXT("preferences.done"))));
    });
    AfterEach([this]{Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}Store.reset();Files.reset();IAutomationDriverModule::Get().Disable();});
}
