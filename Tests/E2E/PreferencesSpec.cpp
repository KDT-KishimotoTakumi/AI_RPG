#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Presentation/RunScreen.h"
#include "Infrastructure/DiskSlotFiles.h"
#include "Application/Encounter.h"
namespace {
class PreferencesTestFiles : public GirlsSpire::DiskSlotFiles {
public:
    using DiskSlotFiles::DiskSlotFiles;
    bool FailRead=false,FailWrite=false,FailPreserve=false;
    GirlsSpire::SlotBytes ReadPreferences()override{return FailRead?GirlsSpire::SlotBytes{true,{}}:DiskSlotFiles::ReadPreferences();}
    bool ReplacePreferences(std::string_view bytes)override{return !FailWrite && DiskSlotFiles::ReplacePreferences(bytes);}
    bool PreservePreferences()override{return !FailPreserve && DiskSlotFiles::PreservePreferences();}
};
}
BEGIN_DEFINE_SPEC(FGirlsSpirePreferencesSpec,"GirlsSpire.UI.Preferences",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;TSharedPtr<SWindow> Window;
    std::shared_ptr<PreferencesTestFiles> Files;
    std::shared_ptr<GirlsSpire::PreferencesStore> Store;
    std::shared_ptr<GirlsSpire::SaveTransaction> Saves;
    FString Root;
    void Open(){Async(EAsyncExecution::TaskGraphMainThread,[this]{Window->SetContent(SNew(SRunScreen).SaveService(Saves).PreferencesService(Store).DevelopmentTools(false));}).Wait();DrainSlateLayout();}
    bool Click(const char* key){const auto target=Driver->FindElement(By::Id(key));if(!target->IsVisible()){const auto scroll=Driver->FindElement(By::Id("run.scroll"));scroll->ScrollToBeginning();DrainSlateLayout();if(!target->IsVisible()){scroll->ScrollToEndUntil(By::Id(key));DrainSlateLayout();}}return TestTrue(UTF8_TO_TCHAR(key),ClickAndDrain(target));}
END_DEFINE_SPEC(FGirlsSpirePreferencesSpec)
void FGirlsSpirePreferencesSpec::Define()
{
    BeforeEach([this]{FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();Window=SNew(SWindow).ClientSize(FVector2D(1200,1000));FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();Root=FPaths::ProjectSavedDir()/TEXT("Tests/Preferences")/FGuid::NewGuid().ToString(EGuidFormats::Digits);Files=std::make_shared<PreferencesTestFiles>(std::filesystem::path(*Root));Store=std::make_shared<GirlsSpire::PreferencesStore>(*Files);Saves=std::make_shared<GirlsSpire::SaveTransaction>(*Files,*Files);});
    LatentBeforeEach(WaitForSlateLayout);
    for(int size=0;size<3;++size)It(*FString::Printf(TEXT("文字%dと全設定を取消・実保存・画面再生成・戦闘へ反映する"),size),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(180),[this,size]{
        using namespace GirlsSpire;Run run(42,Difficulty::Beginner,HeroRole::Sword);run.ChooseNode(0,0);
        if(!TestTrue(TEXT("周回を独立保存"),Saves->Prepare(RunSaveState{run.Checkpoint(),{}},run.ProfileState())==SlotIssue::None && Saves->Retry()==SlotIssue::None))return;
        const auto runBytes=Files->Read(SaveKind::Run,false).Bytes,profileBytes=Files->Read(SaveKind::Profile,false).Bytes;
        Open();const auto base=Driver->FindElement(By::Id("title.heading"))->GetSize().Y;
        if(!Click("title.settings") || !Click("settings.text.2") || !Click("settings.bgm.mute") || !Click("settings.cancel"))return;
        TestTrue(TEXT("取消は設定ファイル/周回を変更しない"),Store->Load().Issue==PreferencesIssue::Empty && Files->Read(SaveKind::Run,false).Bytes==runBytes && Files->Read(SaveKind::Profile,false).Bytes==profileBytes);
        if(!Click("title.settings") || !Click(TCHAR_TO_UTF8(*FString::Printf(TEXT("settings.text.%d"),size))) || !Click("settings.bgm.mute") || !Click("settings.sfx.down") || !Click("settings.subtitles") || !Click("settings.effects") || !Click("settings.intimacy") || !Click("settings.save"))return;
        const Preferences expected{static_cast<TextSize>(size),0,90,false,true,false};
        TestTrue(TEXT("全値を保存し周回/恒久記録は不変"),Store->Load().Issue==PreferencesIssue::None && Store->Load().Value==expected && Files->Read(SaveKind::Run,false).Bytes==runBytes && Files->Read(SaveKind::Profile,false).Bytes==profileBytes);
        Store=std::make_shared<PreferencesStore>(*Files);Open();const auto height=Driver->FindElement(By::Id("title.heading"))->GetSize().Y;
        TestTrue(TEXT("再生成後もタイトルの実文字サイズへ反映"),size==0?FMath::IsNearlyEqual(height,base):height>base);
        if(!Click("title.settings"))return;TestTrue(TEXT("全フラグ/音量/サイズを再読込"),Driver->FindElement(By::Id("settings.effects"))->GetText().ToString().Contains(TEXT("入")) && Driver->FindElement(By::Id("settings.intimacy"))->GetText().ToString().Contains(TEXT("切")) && Driver->FindElement(By::Id("settings.bgm.value"))->GetText().ToString().Contains(TEXT("BGM音量: 0%")) && Driver->FindElement(By::Id("settings.sfx.value"))->GetText().ToString().Contains(TEXT("効果音量: 90%")) && Driver->FindElement(By::Id("settings.subtitles"))->GetText().ToString().Contains(TEXT("字幕: 切")) && Store->Load().Value==expected);
        if(!Click("settings.cancel") || !Click("title.resume") || !Click("battle.start"))return;
        const auto saved=Saves->Load();TestTrue(TEXT("文字設定は戦場/行動順/乱数を変えない"),saved.Run && saved.Run->Combat && saved.Run->Combat->Battle==Encounter::Create(run.Snapshot(),run.RandomState(),run.ProfileState()).Checkpoint());
        TestTrue(TEXT("戦闘の実文字サイズにも反映"),Driver->FindElement(By::Id("battle.round"))->GetSize().Y>=ScaledFontSize(expected.Text,23));
    });
    It(TEXT("保存失敗は未適用で下書きを維持し再試行する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this]{
        using namespace GirlsSpire;Open();const auto base=Driver->FindElement(By::Id("title.heading"))->GetSize().Y;
        if(!Click("title.settings") || !Click("settings.text.1") || !Click("settings.bgm.down") || !Click("settings.bgm.up") || !Click("settings.sfx.mute") || !Click("settings.sfx.up"))return;
        Files->FailWrite=true;if(!Click("settings.save"))return;TestTrue(TEXT("保存失敗中は旧適用値・下書きを保持"),Store->Load().Issue==PreferencesIssue::Empty && Driver->FindElement(By::Id("settings.message"))->Exists() && Driver->FindElement(By::Id("settings.text.value"))->GetText().ToString().Contains(TEXT("125%")));
        Files->FailWrite=false;if(!Click("settings.save"))return;TestTrue(TEXT("再試行成功時だけ適用"),Store->Load().Value.Text==TextSize::Large && Store->Load().Value.BgmVolume==100 && Store->Load().Value.SfxVolume==10 && Driver->FindElement(By::Id("title.heading"))->GetSize().Y>base);
        if(!Click("title.settings") || !Click("settings.defaults") || !Click("settings.save"))return;TestTrue(TEXT("標準設定の選択も明示保存"),Store->Load().Value==Preferences{});
    });
    It(TEXT("破損原本を確認・保全してから標準設定へ戻す"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this]{
        using namespace GirlsSpire;const std::string original="future-or-corrupt";Files->ReplacePreferences(original);Open();
        if(!Click("title.settings") || !Click("settings.reset") || !Click("settings.reset.cancel"))return;
        TestTrue(TEXT("確認取消は原本を保持"),Files->ReadPreferences().Bytes==original && !Driver->FindElement(By::Id("settings.save"))->Exists());
        Files->FailPreserve=true;if(!Click("settings.reset") || !Click("settings.reset.confirm"))return;TestTrue(TEXT("保全失敗は復旧停止"),Files->ReadPreferences().Bytes==original && Driver->FindElement(By::Id("settings.message"))->Exists());
        Files->FailPreserve=false;if(!Click("settings.reset") || !Click("settings.reset.confirm"))return;TestTrue(TEXT("明示復旧後だけ既定値を保存"),Store->Load().Issue==PreferencesIssue::None && Store->Load().Value==Preferences{});
        if(!TestTrue(TEXT("原本保全先が存在"),std::filesystem::is_directory(std::filesystem::path(*(Root/TEXT("Corrupt"))))))return;bool preserved=false;for(const auto& entry:std::filesystem::recursive_directory_iterator(std::filesystem::path(*(Root/TEXT("Corrupt"))))){if(entry.path().filename()=="preferences.sav"){FString bytes;FFileHelper::LoadFileToString(bytes,entry.path().c_str());preserved|=bytes==UTF8_TO_TCHAR(original.c_str());}}TestTrue(TEXT("隔離先に原本を保全"),preserved);
    });
    It(TEXT("読込失敗は初回設定と混同せず再試行する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this]{
        Files->FailRead=true;Open();TestTrue(TEXT("タイトルにも設定読込失敗を表示"),Driver->FindElement(By::Id("title.settings.error"))->Exists());if(!Click("title.settings"))return;TestTrue(TEXT("読込エラー中は保存/初期化不可"),Driver->FindElement(By::Id("settings.error"))->Exists() && !Driver->FindElement(By::Id("settings.save"))->Exists() && !Driver->FindElement(By::Id("settings.reset"))->Exists());Files->FailRead=false;if(!Click("settings.retry") || !Click("settings.cancel"))return;
    });
    for(bool readFailure:{false,true})It(*FString::Printf(TEXT("編集中の外部変更/読込失敗%dを保存時に再確認する"),readFailure),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this,readFailure]{
        Open();if(!Click("title.settings") || !Click("settings.text.2"))return;
        Files->ReplacePreferences("externally-changed");Files->FailRead=readFailure;
        if(!Click("settings.save"))return;
        TestTrue(TEXT("保存直前の異常で上書きを止める"),Driver->FindElement(By::Id("settings.error"))->Exists() && !Driver->FindElement(By::Id("settings.save"))->Exists());
        Files->FailRead=false;TestTrue(TEXT("外部変更の原本を保全"),Files->ReadPreferences().Bytes==std::string("externally-changed"));
    });
    AfterEach([this]{Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}Store.reset();Saves.reset();Files.reset();IAutomationDriverModule::Get().Disable();});
}
