#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/RunScreen.h"
#include "Infrastructure/DiskSlotFiles.h"
#include "Application/Encounter.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "ImageUtils.h"

namespace {
class AppearanceMenuFiles : public GirlsSpire::SlotFiles,public GirlsSpire::JournalFiles {
public:
    std::array<std::array<std::optional<std::string>,2>,2> Slots{};
    std::optional<std::string> Journal;
    bool Fail=false;
    GirlsSpire::SlotBytes Read(GirlsSpire::SaveKind kind,bool backup)override{return {false,Slots[static_cast<int>(kind)-1][backup]};}
    GirlsSpire::SlotBytes ReadJournal()override{return {false,Journal};}
    bool Replace(GirlsSpire::SaveKind kind,bool backup,std::string_view value)override{if(Fail)return false;Slots[static_cast<int>(kind)-1][backup]=std::string(value);return true;}
    bool ReplaceJournal(std::string_view value)override{if(Fail)return false;Journal=std::string(value);return true;}
};
GirlsSpire::Profile AppearanceMenuUnlocked()
{
    GirlsSpire::Profile value;value.Bonds.fill(80);value.SpecialConversations.fill(true);value.VictoryRecorded=true;value.HighestDifficulty=1;return value;
}
}
BEGIN_DEFINE_SPEC(FGirlsSpireAppearanceMenuSpec,"GirlsSpire.UI.AppearanceMenu",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;TSharedPtr<SWindow> Window;
    std::shared_ptr<AppearanceMenuFiles> Files;
    std::shared_ptr<GirlsSpire::SaveTransaction> Service;
    std::vector<std::shared_ptr<GirlsSpire::DiskSlotFiles>> DiskBackends;
    FString DiskFolder;
    void CapturePreview(){Async(EAsyncExecution::TaskGraphMainThread,[this]{TArray<FColor> pixels;FIntVector size;if(!TestTrue(TEXT("衣装予告を撮影"),FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),pixels,size)))return;TArray64<uint8> png;FImageUtils::PNGCompressImageArray(size.X,size.Y,pixels,png);const auto dir=FPaths::ProjectSavedDir()/TEXT("Screenshots/AppearanceMenu")/FGuid::NewGuid().ToString(EGuidFormats::Digits);IFileManager::Get().MakeDirectory(*dir,true);TestTrue(TEXT("予告画像を保全"),FFileHelper::SaveArrayToFile(png,*(dir/TEXT("preview.png"))));}).Wait();}
    bool Click(const char* id){
        const auto element=Driver->FindElement(By::Id(id));if(!TestTrue(UTF8_TO_TCHAR(id),element->Exists()))return false;
        if(!element->IsVisible())Driver->FindElement(By::Id(Driver->FindElement(By::Id("battle.scroll"))->Exists()?"battle.scroll":"run.scroll"))->ScrollToEndUntil(By::Id(id));
        return TestTrue(UTF8_TO_TCHAR(id),ClickAndDrain(element));
    }
    void Reopen(){
        if(DiskFolder.IsEmpty())Service=std::make_shared<GirlsSpire::SaveTransaction>(*Files,*Files);
        else{auto disk=std::make_shared<GirlsSpire::DiskSlotFiles>(std::filesystem::path(*DiskFolder));DiskBackends.push_back(disk);Service=std::make_shared<GirlsSpire::SaveTransaction>(*disk,*disk);}
        Async(EAsyncExecution::TaskGraphMainThread,[this]{Window->SetContent(SNew(SRunScreen).SaveService(Service).DevelopmentTools(false));}).Wait();DrainSlateLayout();
    }
    void Open(const GirlsSpire::Profile& profile,const std::optional<GirlsSpire::RunSaveState>& run={}){
        Files=std::make_shared<AppearanceMenuFiles>();Service=std::make_shared<GirlsSpire::SaveTransaction>(*Files,*Files);
        TestTrue(TEXT("初期記録を準備"),Service->Prepare(run,profile)==GirlsSpire::SlotIssue::None && Service->Retry()==GirlsSpire::SlotIssue::None);Reopen();
    }
END_DEFINE_SPEC(FGirlsSpireAppearanceMenuSpec)
void FGirlsSpireAppearanceMenuSpec::Define()
{
    BeforeEach([this]{FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();Window=SNew(SWindow).ClientSize(FVector2D(1280,900));FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();});
    LatentBeforeEach(WaitForSlateLayout);
    It(TEXT("未解放9人は標準だけを確認し保存しない"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(180),[this]{
        Open({});const auto slots=Files->Slots;const auto journal=Files->Journal;
        if(!Click("title.appearance"))return;
        for(int id=0;id<=8;++id){const auto entry=FString::Printf(TEXT("appearance.actor.%d"),id);if(!Click(TCHAR_TO_UTF8(*entry)))return;
            TestTrue(TEXT("人物画像を表示"),Driver->FindElement(By::Id("appearance.image"))->Exists());
            TestFalse(TEXT("未解放の差分は選べない"),Driver->FindElement(By::Id("appearance.alternate"))->Exists());
            TestTrue(TEXT("解放条件を知らせる"),Driver->FindElement(By::Id("appearance.locked"))->Exists());
            if(!Click("appearance.standard") || !Click("appearance.cancel"))return;
        }
        TestTrue(TEXT("閲覧で保存しない"),slots==Files->Slots && journal==Files->Journal);
        if(!Click("appearance.back"))return;TestTrue(TEXT("タイトルへ戻る"),Driver->FindElement(By::Id("title.start"))->Exists());
    });
    It(TEXT("9人の予告取消と確定を保存し周回を維持する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(240),[this]{
        using namespace GirlsSpire;auto expected=AppearanceMenuUnlocked();Run run(42,Difficulty::Beginner,HeroRole::Sword,expected);RunSaveState saved{run.Checkpoint(),{}};const auto original=EncodeRunPayload(saved);
        Open(expected,saved);if(!Click("title.appearance"))return;
        for(int id=0;id<=8;++id){const auto entry=FString::Printf(TEXT("appearance.actor.%d"),id);const auto before=Service->Load();const auto slots=Files->Slots;
            if(!Click(TCHAR_TO_UTF8(*entry)) || !Click("appearance.alternate"))return;
            if(id==0)CapturePreview();
            TestTrue(TEXT("画像予告だけでは保存しない"),slots==Files->Slots && Service->Load().Generation==before.Generation);
            if(!Click("appearance.cancel"))return;TestTrue(TEXT("取消で選択を維持"),Service->Load().Permanent==expected);
            if(!Click(TCHAR_TO_UTF8(*entry)) || !Click("appearance.alternate") || !Click("appearance.confirm"))return;
            expected.SelectedAppearances[id]=AppearanceVariant::Alternate;const auto after=Service->Load();
            TestTrue(TEXT("対象人物だけ保存し進行/RNG/中断権を維持"),after.Permanent==expected && after.Run==saved && after.Generation>before.Generation && EncodeRunPayload(*after.Run)==original);
            if(!Click("appearance.cancel"))return;
        }
        Reopen();TestTrue(TEXT("別ストアで恒久選択を再読"),Service->Load().Permanent==expected);
        if(!Click("title.appearance") || !Click("appearance.actor.0") || !Click("appearance.standard") || !Click("appearance.confirm"))return;
        expected.SelectedAppearances[0]=AppearanceVariant::Standard;TestTrue(TEXT("標準へ戻しても解放と他人物を保持"),Service->Load().Permanent==expected && Service->Load().Run==saved);
    });
    It(TEXT("外見保存の失敗で停止し同じ内容を再試行する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this]{
        using namespace GirlsSpire;const auto profile=AppearanceMenuUnlocked();Open(profile);
        if(!Click("title.appearance") || !Click("appearance.actor.0") || !Click("appearance.alternate"))return;
        const auto slots=Files->Slots;Files->Fail=true;if(!Click("appearance.confirm"))return;
        TestTrue(TEXT("保存失敗画面で停止"),Driver->FindElement(By::Id("save.error"))->Exists());TestTrue(TEXT("失敗前の原本を維持"),Files->Slots==slots);
        Files->Fail=false;if(!Click("save.retry"))return;auto expected=profile;expected.SelectedAppearances[0]=AppearanceVariant::Alternate;
        TestTrue(TEXT("同じ選択を一度だけ保存"),Service->Load().Permanent==expected && !Service->HasPending());
    });
    It(TEXT("設定を変えても旧v19の保存戦闘は標準のまま再開する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this]{
        using namespace GirlsSpire;auto profile=AppearanceMenuUnlocked();Run run(42,Difficulty::Beginner,HeroRole::Sword,profile);run.ChooseNode(run.Snapshot().Revision,0);
        auto battle=Encounter::Create(run.Snapshot(),run.RandomState(),profile);CombatControl control;control.ChoiceRandom=Random(12);control.ProfileDriven=true;for(int id=1;id<=8;++id)control.Trust[id]=profile.Bonds[id-1];
        if(!TestEqual(TEXT("初期プレイヤー手番を再開する準備"),battle.Snapshot().ActiveId,0))return;
        RunSaveState saved{run.Checkpoint(),SavedCombat{battle.Checkpoint(),control.Checkpoint()}};Open(profile,saved);
        if(!Click("title.appearance") || !Click("appearance.actor.0") || !Click("appearance.alternate") || !Click("appearance.confirm") || !Click("appearance.cancel") || !Click("appearance.back"))return;
        TestTrue(TEXT("設定確定で古い戦闘を上書きしない"),Service->Load().Run==saved);
        if(!Click("title.resume"))return;const auto after=Service->Load();
        TestTrue(TEXT("再開は標準を固定し選択だけは差分を保持"),after.Run && after.Run->Combat && after.Run->Combat->Appearances[0]==AppearanceVariant::Standard && after.Permanent.SelectedAppearances[0]==AppearanceVariant::Alternate && after.Run->Combat->Battle==saved.Combat->Battle && !after.Run->ResumeAvailable);
    });
    It(TEXT("配色v20戦闘を実ディスクの新ストアから同じ外見で再開する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(180),[this]{
        using namespace GirlsSpire;auto profile=AppearanceMenuUnlocked();Run run(42,Difficulty::Beginner,HeroRole::Sword,profile);run.ChooseNode(run.Snapshot().Revision,0);
        auto battle=Encounter::Create(run.Snapshot(),run.RandomState(),profile);CombatControl control;control.ChoiceRandom=Random(12);control.ProfileDriven=true;for(int id=1;id<=8;++id)control.Trust[id]=profile.Bonds[id-1];
        if(!TestEqual(TEXT("初期プレイヤー手番を使う"),battle.Snapshot().ActiveId,0))return;
        SavedCombat combat{battle.Checkpoint(),control.Checkpoint()};combat.Appearances[0]=AppearanceVariant::Alternate;RunSaveState saved{run.Checkpoint(),combat};
        DiskFolder=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Tests/AppearanceMenu")/FGuid::NewGuid().ToString(EGuidFormats::Digits));
        if(!TestTrue(TEXT("隔離保存フォルダ"),IFileManager::Get().MakeDirectory(*DiskFolder,true)))return;
        auto disk=std::make_shared<DiskSlotFiles>(std::filesystem::path(*DiskFolder));DiskBackends.push_back(disk);Service=std::make_shared<SaveTransaction>(*disk,*disk);
        if(!TestTrue(TEXT("v20の配色戦闘を実保存"),Service->Prepare(saved,profile)==SlotIssue::None && Service->Retry()==SlotIssue::None))return;
        Reopen();if(!Click("title.resume"))return;
        auto loaded=Service->Load();TestTrue(TEXT("現在標準でも保存された配色を維持"),loaded.Run && loaded.Run->Combat && loaded.Run->Combat->Appearances==combat.Appearances && loaded.Run->Combat->Battle==combat.Battle && loaded.Permanent.SelectedAppearances[0]==AppearanceVariant::Standard);
        if(!Click("save.suspend"))return;Reopen();if(!Click("title.resume"))return;
        loaded=Service->Load();TestTrue(TEXT("再中断/別ストア再開でも固定外見と乱数を維持"),loaded.Run && loaded.Run->Combat && loaded.Run->Combat->Appearances==combat.Appearances && loaded.Run->Combat->Battle==combat.Battle);
    });
    AfterEach([this]{Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}IAutomationDriverModule::Get().Disable();Service.reset();Files.reset();DiskBackends.clear();DiskFolder.Empty();});
}
