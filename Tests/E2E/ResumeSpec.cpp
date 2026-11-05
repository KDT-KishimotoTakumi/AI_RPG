#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Presentation/RunScreen.h"
#include "Infrastructure/DiskSlotFiles.h"
#include "Application/Encounter.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include <fstream>
#include <atomic>

namespace {
class ResumeFaultFiles : public GirlsSpire::DiskSlotFiles {
public:
    explicit ResumeFaultFiles(const std::filesystem::path& root) : DiskSlotFiles(root) {}
    std::atomic_bool Fail=false;
    std::atomic_int UnexpectedWrites=0,UnexpectedReads=0;
    std::atomic_uint32_t LastWriteError=0;
    std::vector<GirlsSpire::RunSaveState> Checkpoints;
    bool Replace(GirlsSpire::SaveKind kind,bool backup,std::string_view bytes) override {
        if(Fail)return false;
        if(!DiskSlotFiles::Replace(kind,backup,bytes)){LastWriteError=FPlatformMisc::GetLastError();++UnexpectedWrites;return false;}
        if(kind==GirlsSpire::SaveKind::Run && !backup) {
            const auto envelope=GirlsSpire::DecodeSave(bytes,kind);
            if(envelope.Value) if(const auto state=GirlsSpire::DecodeRunPayload(envelope.Value->Payload)) Checkpoints.push_back(*state);
        }
        return true;
    }
    bool ReplaceJournal(std::string_view bytes) override {
        if(Fail)return false;
        const bool ok=DiskSlotFiles::ReplaceJournal(bytes);if(!ok){LastWriteError=FPlatformMisc::GetLastError();++UnexpectedWrites;}return ok;
    }
    GirlsSpire::SlotBytes Read(GirlsSpire::SaveKind kind,bool backup) override {
        auto result=DiskSlotFiles::Read(kind,backup);if(result.Failed)++UnexpectedReads;return result;
    }
    GirlsSpire::SlotBytes ReadJournal() override {
        auto result=DiskSlotFiles::ReadJournal();if(result.Failed)++UnexpectedReads;return result;
    }
};
}
BEGIN_DEFINE_SPEC(FGirlsSpireResumeSpec,"GirlsSpire.UI.Save",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
    std::shared_ptr<ResumeFaultFiles> Files;
    std::shared_ptr<GirlsSpire::SaveTransaction> Service;
    FString TestRoot;
    bool PreserveFailure=false;
    bool Click(const char* id);
    void Reopen();
    void Diagnose();
END_DEFINE_SPEC(FGirlsSpireResumeSpec)
bool FGirlsSpireResumeSpec::Click(const char* id)
{
    const int writesBefore=Files->UnexpectedWrites.load();
    const auto target=Driver->FindElement(By::Id(id));
    if(!target->IsVisible()) {
        const auto scroll=Driver->FindElement(By::Id("run.scroll"));
        scroll->ScrollToBeginning();Async(EAsyncExecution::TaskGraphMainThread,[]{}).Wait();
        if(!target->IsVisible()) {scroll->ScrollToEndUntil(By::Id(id));Async(EAsyncExecution::TaskGraphMainThread,[]{}).Wait();}
    }
    bool clicked=ClickAndDrain(target);if(!clicked)Diagnose();
    if(clicked && !Files->Fail && Files->UnexpectedWrites.load()>writesBefore &&
       Driver->FindElement(By::Id("save.error"))->Exists()) {
        // 実環境の共有制約は、停止・保全・画面からの明示再試行までを受け入れ条件とする。
        Diagnose();
        const auto error=Files->LastWriteError.load();
        if(!TestTrue(TEXT("OSのアクセス拒否/共有違反を記録"),error==5 || error==32) ||
           !TestTrue(TEXT("保存拒否では固定取引を保持して進行停止"),Service->HasPending() &&
              !Driver->FindElement(By::Id("route.node.0"))->Exists() && !Driver->FindElement(By::Id("role.confirm"))->Exists()))return false;
        if(!TestTrue(TEXT("実IO拒否時の元ファイルと取引を保全"),Files->Preserve(GirlsSpire::SaveKind::Run) &&
              Files->Preserve(GirlsSpire::SaveKind::Profile) && Files->PreserveJournal()))return false;
        for(int retry=0;retry<2 && Driver->FindElement(By::Id("save.error"))->Exists();++retry) {
            AddInfo(TEXT("実IO拒否を検証・保全後、保存画面の再試行ボタンを操作"));
            if(!TestTrue(TEXT("固定内容を画面から再保存"),ClickAndDrain(Driver->FindElement(By::Id("save.retry")))))return false;
        }
        clicked=TestTrue(TEXT("明示再試行後に同じ取引を確定"),!Service->HasPending() &&
            !Driver->FindElement(By::Id("save.error"))->Exists());
    }
    return TestTrue(*FString::Printf(TEXT("保存操作 %hs"),id),clicked);
}
void FGirlsSpireResumeSpec::Diagnose()
{
    PreserveFailure=true;const auto loaded=Service->Load();FString visible;
    for(const char* id:{"title.start","title.resume","role.confirm","result.lost","save.error","save.load.error","save.version.error","route.node.0","battle.cell.3.1","rescue.help"})
        if(Driver->FindElement(By::Id(id))->Exists())visible+=FString::Printf(TEXT("%hs "),id);
    AddInfo(FString::Printf(TEXT("保存UI診断: Issue=%d Run=%d Profile=%d Pending=%d Generation=%llu UnexpectedWrites=%d UnexpectedReads=%d LastWriteError=%u / UI=%s / 保存先=%s"),
        static_cast<int>(loaded.Issue),static_cast<int>(loaded.RunIssue),static_cast<int>(loaded.ProfileIssue),Service->HasPending(),
        static_cast<unsigned long long>(loaded.Generation),Files->UnexpectedWrites.load(),Files->UnexpectedReads.load(),Files->LastWriteError.load(),*visible,*TestRoot));
}
void FGirlsSpireResumeSpec::Reopen()
{
    Async(EAsyncExecution::TaskGraphMainThread,[this]{Window->SetContent(SNew(SRunScreen).SaveService(Service));}).Wait();
    DrainSlateLayout();
}
void FGirlsSpireResumeSpec::Define()
{
    BeforeEach([this] {
        PreserveFailure=false;
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        // 破損や将来版を作る専用データは同期領域から分離。実保存先は別プロセス試験で確認する。
        TestRoot=FPaths::ConvertRelativePathToFull(FString(FPlatformProcess::UserTempDir())/TEXT("GirlsSpireTests/Resume")/FGuid::NewGuid().ToString(EGuidFormats::Digits));
        Files=std::make_shared<ResumeFaultFiles>(std::filesystem::path(*TestRoot));
        Service=std::make_shared<GirlsSpire::SaveTransaction>(*Files,*Files);
        Window=SNew(SWindow).Title(FText::FromString(TEXT("Girls Spire — 保存テスト")))
            .ClientSize(FVector2D(1100,760)).SupportsMaximize(false)[SNew(SRunScreen).SaveService(Service)];
        FSlateApplication::Get().AddWindow(Window.ToSharedRef());
        Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    It(TEXT("新規の支度は初級だけ選べて解放条件を表示する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(90),[this]{
        if(!Click("title.start"))return;
        TestTrue(TEXT("初級の選択がある"),Driver->FindElement(By::Id("difficulty.0"))->Exists());
        for(int d=1;d<3;++d){
            TestFalse(TEXT("未解放は選択不可"),Driver->FindElement(By::Id(*FString::Printf(TEXT("difficulty.%d"),d)))->Exists());
            TestTrue(TEXT("解放条件を文字表示"),Driver->FindElement(By::Id(*FString::Printf(TEXT("difficulty.locked.%d"),d)))->Exists());
        }
        if(!Click("role.confirm"))return;
        TestTrue(TEXT("初級で出発"),Service->Load().Run && Service->Load().Run->Run.State.Level==GirlsSpire::Difficulty::Beginner);
    });
    for(int level=0;level<2;++level)It(*FString::Printf(TEXT("難易度%d勝利の解放は後日談前に保存し固定再試行する"),level),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this,level]{
        using namespace GirlsSpire;
        auto run=std::make_shared<Run>(42,static_cast<Difficulty>(level),HeroRole::Sword);
        for(int step=0;step<24 && run->Snapshot().Phase==RunPhase::Route;++step){run->ChooseNode(run->Snapshot().Revision,0);run->CompleteEvent(run->Snapshot().Revision);}
        Files->Fail=true;
        Async(EAsyncExecution::TaskGraphMainThread,[this,run]{Window->SetContent(SNew(SRunScreen).InitialRun(run).SaveService(Service));}).Wait();DrainSlateLayout();
        TestTrue(TEXT("勝利保存失敗は停止"),Driver->FindElement(By::Id("save.error"))->Exists());
        TestTrue(TEXT("取引を固定"),Service->HasPending());
        TestFalse(TEXT("後日談へ進めない"),Driver->FindElement(By::Id("result.continue"))->Exists());
        Files->Fail=false;if(!Click("save.retry"))return;
        auto saved=Service->Load();
        TestTrue(TEXT("後日談を選ぶ前に勝利と解放を同時保存"),saved.Run && saved.Run->Run.State.Phase==RunPhase::Won && saved.Permanent.HighestDifficulty==level+1 && saved.Permanent.VictoryRecorded);
        Reopen();if(!Click("title.resume"))return;
        TestTrue(TEXT("勝利再開で解放を維持"),Service->Load().Permanent.HighestDifficulty==level+1 && Service->Load().Permanent.VictoryRecorded);
    });
    It(TEXT("選択確定を自動保存し中断・画面再生成・一度だけ再開する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(150),[this] {
        using namespace GirlsSpire;
        Profile unlocked;unlocked.HighestDifficulty=2;
        if(!TestTrue(TEXT("解放済みプロフィールを準備"),Service->Prepare({},unlocked)==SlotIssue::None && Service->Retry()==SlotIssue::None))return;
        Reopen();
        if(!Click("title.start") || !Click("difficulty.1") || !Click("role.2") || !Click("role.confirm")) return;
        const auto started=Service->Load();
        if(!TestTrue(TEXT("出発直後に保存済み"),started.Run.has_value())) return;
        TestTrue(TEXT("選択した役割と難易度"),started.Run->Run.State.Role==HeroRole::CommandFan && started.Run->Run.State.Level==Difficulty::Intermediate);
        if(!Click("route.node.0")) return;
        const auto chosen=Service->Load();
        TestTrue(TEXT("経路の選択確定を保存"),chosen.Run && chosen.Run->Run.State.Phase==RunPhase::Event);
        if(!Click("save.suspend")) return;
        TestTrue(TEXT("中断後はタイトル"),Driver->FindElement(By::Id("title.resume"))->Exists());
        Reopen();if(!Click("title.resume")) return;
        const auto resumed=Service->Load();
        if(!resumed.Run || !chosen.Run || resumed.Run->Run!=chosen.Run->Run || resumed.Run->ResumeAvailable ||
           !Driver->FindElement(By::Id("preview.battle"))->Exists())Diagnose();
        TestTrue(TEXT("同じイベントへ復元"),resumed.Run && chosen.Run && resumed.Run->Run==chosen.Run->Run && !resumed.Run->ResumeAvailable);
        TestTrue(TEXT("同じ中断の二度目を拒否"),Service->PrepareResume()==SlotIssue::Invalid);
        TestTrue(TEXT("イベントの画面も復元"),Driver->FindElement(By::Id("preview.battle"))->Exists());
    });
    It(TEXT("保存失敗で進行を止め同じ出発結果を再保存する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(150),[this] {
        using namespace GirlsSpire;
        if(!Click("title.start")) return;
        Files->Fail=true;if(!Click("role.confirm")) return;
        if(!TestTrue(TEXT("保存失敗表示"),Driver->FindElement(By::Id("save.error"))->Exists())) return;
        TestFalse(TEXT("選び直し不可"),Driver->FindElement(By::Id("role.confirm"))->Exists());
        TestFalse(TEXT("次の経路へ進めない"),Driver->FindElement(By::Id("route.node.0"))->Exists());
        TestTrue(TEXT("同じ結果の保存待ち"),Service->HasPending());
        Files->Fail=false;if(!Click("save.retry")) return;
        const auto loaded=Service->Load();
        TestTrue(TEXT("一度の保存だけ確定"),loaded.Run && loaded.Generation==1 && loaded.Run->Run.State.Day==1);
        TestTrue(TEXT("成功後に経路を表示"),Driver->FindElement(By::Id("route.node.0"))->Exists());
    });
    It(TEXT("戦闘の移動を自動保存し中断から同じ盤面へ戻る"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(180),[this] {
        if(!Click("title.start") || !Click("role.confirm") || !Click("route.node.0") || !Click("preview.battle")) return;
        if(!Click("battle.cell.3.1") || !Click("battle.move.confirm")) return;
        const auto saved=Service->Load();
        if(!TestTrue(TEXT("行動直後の戦闘を保存"),saved.Run && saved.Run->Combat.has_value())) return;
        TestTrue(TEXT("移動済みの状態"),saved.Run->Combat->Battle.State.Moved);
        if(!Click("save.suspend")) return;
        Reopen();if(!Click("title.resume")) return;
        const auto resumed=Service->Load();
        TestTrue(TEXT("戦場・乱数・作戦が一致"),resumed.Run && resumed.Run->Combat==saved.Run->Combat && !resumed.Run->ResumeAvailable);
        TestTrue(TEXT("移動後の主人公表示"),Driver->FindElement(By::Id("battle.celllabel.3.1"))->GetText().ToString().Contains(TEXT("味")));
    });
    It(TEXT("放棄確認の取消と確定で恒久だけを保持する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(150),[this] {
        using namespace GirlsSpire;
        Profile profile;profile.Bonds[0]=55;Run run(42,Difficulty::Beginner,HeroRole::Sword,profile);
        Service->Prepare(RunSaveState{run.Checkpoint(),{}},profile);Service->Retry();Reopen();
        const auto before=Service->Load();
        if(!Click("title.start") || !Click("save.abandon.cancel")) return;
        TestTrue(TEXT("取消で保存不変"),Service->Load().Generation==before.Generation && Service->Load().Run==before.Run);
        if(!Click("title.start") || !Click("save.abandon.confirm")) return;
        const auto abandoned=Service->Load();TestTrue(TEXT("周回だけを放棄"),!abandoned.Run && abandoned.Permanent==profile);
        if(!Click("role.confirm")) return;
        const auto next=Service->Load();TestTrue(TEXT("次周でも絆保持"),next.Run && next.Permanent==profile && next.Run->Run.State.Recruited.empty());
    });
    It(TEXT("再開の消費保存に失敗した間は画面を開かない"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(150),[this] {
        using namespace GirlsSpire;
        Run run(42,Difficulty::Beginner,HeroRole::Sword);Service->Prepare(RunSaveState{run.Checkpoint(),{}},{});Service->Retry();Reopen();
        Files->Fail=true;if(!Click("title.resume")) return;
        TestTrue(TEXT("消費の保存エラー"),Driver->FindElement(By::Id("save.error"))->Exists());
        TestFalse(TEXT("保存成功前に再開しない"),Driver->FindElement(By::Id("route.node.0"))->Exists());
        Files->Fail=false;if(!Click("save.retry")) return;
        const auto resumed=Service->Load();
        TestTrue(TEXT("消費1回だけを確定"),resumed.Run && !resumed.Run->ResumeAvailable && resumed.Generation==2);
        TestTrue(TEXT("再試行後に経路を開く"),Driver->FindElement(By::Id("route.node.0"))->Exists());
    });
    It(TEXT("戦闘保存に失敗した行動は再実行せず敵の各行動も保存する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(180),[this] {
        if(!Click("title.start") || !Click("role.confirm") || !Click("route.node.0") || !Click("preview.battle") ||
            !Click("battle.cell.3.1") || !Click("battle.move.confirm")) return;
        const auto before=Service->Load();
        const auto generation=before.Generation;
        if(!Click("battle.target.100")) return;
        TestTrue(TEXT("攻撃予告では保存しない"),Service->Load().Generation==generation);
        Files->Fail=true;if(!Click("battle.confirm")) return;
        if(!TestTrue(TEXT("戦闘中の保存失敗表示"),Driver->FindElement(By::Id("save.error"))->Exists())) return;
        TestFalse(TEXT("保存失敗中の行動不可"),Driver->FindElement(By::Id("battle.wait.south"))->Exists());
        Files->Fail=false;if(!Click("save.retry")) return;
        const auto attacked=Service->Load();
        if(!TestTrue(TEXT("攻撃を一度だけ確定"),attacked.Run && attacked.Run->Combat && before.Run && before.Run->Combat &&
            attacked.Run->Combat->Battle.State.Revision==before.Run->Combat->Battle.State.Revision+1)) return;
        Files->Checkpoints.clear();if(!Click("battle.wait.south")) return;
        TestTrue(TEXT("待機と敵の行動も個別保存"),Files->Checkpoints.size()>=2);
        std::uint64_t revision=attacked.Run->Combat->Battle.State.Revision;
        for(const auto& checkpoint:Files->Checkpoints) {
            TestTrue(TEXT("解決済み行動を飛ばさない"),checkpoint.Combat && checkpoint.Combat->Battle.State.Revision==++revision);
        }
    });
    It(TEXT("バックアップ復旧を通知し両方破損では上書きしない"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(150),[this] {
        using namespace GirlsSpire;
        Profile profile;profile.Bonds[2]=75;Run run(42,Difficulty::Beginner,HeroRole::Sword,profile);
        Service->Prepare(RunSaveState{run.Checkpoint(),{}},profile);Service->Retry();
        const auto original=Files->Read(SaveKind::Run,false).Bytes;
        Files->Replace(SaveKind::Run,false,"broken");Reopen();
        TestTrue(TEXT("復旧通知"),Driver->FindElement(By::Id("save.recovered"))->Exists());
        Files->Replace(SaveKind::Run,true,"broken");Reopen();
        TestTrue(TEXT("復旧不能表示"),Driver->FindElement(By::Id("save.load.error"))->Exists());
        TestFalse(TEXT("破損を自動初期化しない"),Driver->FindElement(By::Id("title.start"))->Exists());
        TestTrue(TEXT("正常な恒久を保全"),Service->Load().Permanent==profile);
        TestTrue(TEXT("破損ファイルを保持"),Files->Read(SaveKind::Run,false).Bytes=="broken" && Files->Read(SaveKind::Run,true).Bytes=="broken");
        if(original) {Files->Replace(SaveKind::Run,false,*original);Files->Replace(SaveKind::Run,true,*original);}
        if(!Click("save.reload")) return;
        TestTrue(TEXT("再読込で再開可能に戻る"),Driver->FindElement(By::Id("title.resume"))->Exists());
    });
    It(TEXT("敗北からタイトルへの保存失敗も恒久を保ち再試行する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(150),[this] {
        using namespace GirlsSpire;
        if(!Click("title.start") || !Click("role.confirm") || !Click("route.node.0") || !Click("preview.defeat")) return;
        Files->Fail=true;if(!Click("result.continue")) return;
        TestTrue(TEXT("周回消去の保存失敗"),Driver->FindElement(By::Id("save.error"))->Exists());
        Files->Fail=false;if(!Click("save.retry")) return;
        TestTrue(TEXT("終了後は周回なし"),!Service->Load().Run);
        if(!Driver->FindElement(By::Id("title.start"))->Exists())Diagnose();
        TestTrue(TEXT("タイトルへ戻る"),Driver->FindElement(By::Id("title.start"))->Exists());
        TestFalse(TEXT("終了周回を再開しない"),Driver->FindElement(By::Id("title.resume"))->Exists());
    });
    for(int side=0;side<2;++side) It(side==0?TEXT("周回だけの初期化を確認し破損ファイルと恒久を保つ"):TEXT("恒久だけの初期化を確認し破損ファイルと正常周回を保つ"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(180),[this,side] {
        using namespace GirlsSpire;
        const auto kind=side==0?SaveKind::Run:SaveKind::Profile;
        Profile profile;profile.Bonds.fill(55);profile.HighestDifficulty=2;profile.VictoryRecorded=true;
        profile.SpecialConversations.fill(true);profile.Epilogues[0]=true;profile.DiscoveredCombos={2,201};profile.DiscoveredEnemies={1,12};
        profile.SelectedAppearances.fill(AppearanceVariant::Alternate);
        Run run(42,Difficulty::Intermediate,HeroRole::CommandFan,profile);
        const RunSaveState saved{run.Checkpoint(),{}};
        Service->Prepare(saved,profile);Service->Retry();
        Files->Replace(kind,false,"broken main");Files->Replace(kind,true,"broken backup");Reopen();
        const char* reset=side==0?"save.reset.run":"save.reset.profile";
        if(!TestTrue(TEXT("対象側の初期化操作"),Driver->FindElement(By::Id(reset))->Exists())) return;
        if(!Click(reset)) return;
        TestTrue(TEXT("損失内容の確認表示"),Driver->FindElement(By::Id("save.reset.warning"))->Exists());
        const auto warning=Driver->FindElement(By::Id("save.reset.warning"))->GetText().ToString();
        if(side==0)TestTrue(TEXT("周回初期化は恒久記録全体の保持を知らせる"),warning.Contains(TEXT("恒久記録")));
        else for(const TCHAR* category:{TEXT("絆"),TEXT("専用交流"),TEXT("連携"),TEXT("敵図鑑"),TEXT("難易度"),TEXT("衣装"),TEXT("勝利記録"),TEXT("後日談")})
            TestTrue(FString(TEXT("失う記録を明示: "))+category,warning.Contains(category));
        TestTrue(TEXT("確認前に破損を消さない"),Files->Read(kind,false).Bytes=="broken main");
        if(!Click("save.reset.cancel") || !Click(reset)) return;
        Files->Fail=true;if(!Click("save.reset.confirm")) return;
        TestTrue(TEXT("初期化の保存失敗も停止"),Driver->FindElement(By::Id("save.error"))->Exists());
        Files->Fail=false;if(!Click("save.retry")) return;
        const auto loaded=Service->Load();
        TestTrue(TEXT("初期化後は読み込める"),loaded.Issue==SlotIssue::None && loaded.RunIssue==SlotIssue::None && loaded.ProfileIssue==SlotIssue::None);
        TestTrue(TEXT("正常な側を保つ"),side==0?(!loaded.Run && loaded.Permanent==profile):(loaded.Run==saved && loaded.Permanent==Profile{}));
        const auto archive=std::filesystem::path(*TestRoot)/"Corrupt";
        bool main=false,backup=false;
        if(std::filesystem::exists(archive)) for(const auto& entry:std::filesystem::recursive_directory_iterator(archive)) if(entry.is_regular_file()) {
            std::ifstream input(entry.path(),std::ios::binary);
            const std::string bytes((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());
            main=main || bytes=="broken main";backup=backup || bytes=="broken backup";
        }
        TestTrue(TEXT("本体とバックアップを両方保全"),main && backup);
    });
    It(TEXT("対応していない内部形式は破損と別表示にして保持する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(150),[this] {
        using namespace GirlsSpire;
        Run run(42,Difficulty::Beginner,HeroRole::Sword);
        if(!TestTrue(TEXT("将来版試験の初期取引を確定"),Service->Prepare(RunSaveState{run.Checkpoint(),{}},{})==SlotIssue::None && Service->Retry()==SlotIssue::None))return;
        auto payload=*EncodeProfilePayload({});payload[4]=9;
        const auto future=*EncodeSave({SaveKind::Profile,1,payload});
        if(!TestTrue(TEXT("将来版を本体とバックアップへ書込み"),Files->Replace(SaveKind::Profile,false,future) && Files->Replace(SaveKind::Profile,true,future)))return;
        Reopen();
        TestTrue(TEXT("互換性エラーを別表示"),Driver->FindElement(By::Id("save.version.error"))->Exists());
        TestFalse(TEXT("将来版を初期化しない"),Driver->FindElement(By::Id("save.reset.profile"))->Exists());
        TestTrue(TEXT("記録を保持"),Files->Read(SaveKind::Profile,false).Bytes==future);
    });
    It(TEXT("取引記録だけの破損を確認後に保全して再構築する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(180),[this] {
        using namespace GirlsSpire;
        Profile profile;profile.Bonds.fill(80);Run run(42,Difficulty::Beginner,HeroRole::Sword,profile);
        const RunSaveState saved{run.Checkpoint(),{}};
        Service->Prepare(saved,profile);Service->Retry();Files->ReplaceJournal("broken journal");Reopen();
        if(!TestTrue(TEXT("再構築操作がある"),Driver->FindElement(By::Id("save.repair.journal"))->Exists()))return;
        if(!Click("save.repair.journal"))return;
        TestTrue(TEXT("維持される内容を説明"),Driver->FindElement(By::Id("save.repair.warning"))->Exists());
        TestTrue(TEXT("確認前には変更しない"),Files->ReadJournal().Bytes=="broken journal");
        if(!Click("save.repair.cancel"))return;
        TestTrue(TEXT("取消で元記録を維持"),Files->ReadJournal().Bytes=="broken journal");
        if(!Click("save.repair.journal"))return;
        Files->Fail=true;if(!Click("save.repair.confirm"))return;
        TestTrue(TEXT("書込失敗で停止"),Driver->FindElement(By::Id("save.error"))->Exists());
        Files->Fail=false;if(!Click("save.retry"))return;
        const auto restored=Service->Load();
        TestTrue(TEXT("正常な両側は変わらない"),restored.Issue==SlotIssue::None && restored.Run==saved && restored.Permanent==profile);
        bool archived=false;
        const auto archive=std::filesystem::path(*TestRoot)/"Corrupt";
        if(std::filesystem::exists(archive))for(const auto& entry:std::filesystem::recursive_directory_iterator(archive))if(entry.is_regular_file()) {
            std::ifstream input(entry.path(),std::ios::binary);
            const std::string bytes((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());
            archived=archived || bytes=="broken journal";
        }
        TestTrue(TEXT("破損取引記録を保全"),archived);
        TestTrue(TEXT("再開可能に戻る"),Driver->FindElement(By::Id("title.resume"))->Exists());
    });
    It(TEXT("旧形式の絆を保ち専用交流の保存失敗を再試行して再開する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(180),[this] {
        using namespace GirlsSpire;
        Profile profile;profile.HighestDifficulty=2;profile.Bonds.fill(80);std::shared_ptr<Run> run;
        for(int seed=0;seed<100 && !run;++seed) {
            auto candidate=std::make_shared<Run>(seed,Difficulty::Beginner,HeroRole::Sword,profile);
            for(int step=0;step<24 && candidate->Snapshot().Phase==RunPhase::Route;++step) {
                int node=0;
                for(int i=0;i<static_cast<int>(candidate->Snapshot().Choices.size());++i)
                    if(candidate->Snapshot().Choices[i].Kind==NodeKind::Social)node=i;
                candidate->ChooseNode(candidate->Snapshot().Revision,node);
                if(candidate->Snapshot().Active.Kind==NodeKind::Social){run=candidate;break;}
                candidate->CompleteEvent(candidate->Snapshot().Revision);
            }
        }
        if(!TestTrue(TEXT("交流イベントを準備"),run!=nullptr))return;
        Service->Prepare(RunSaveState{run->Checkpoint(),{}},profile);Service->Retry();
        std::string legacy;
        for(std::uint32_t value:{2u,1u,80u,80u,80u,80u,80u,80u,80u,80u})
            for(int i=0;i<4;++i)legacy.push_back(static_cast<char>((value>>(8*i))&255));
        const auto envelope=*EncodeSave({SaveKind::Profile,1,legacy});
        Files->Replace(SaveKind::Profile,false,envelope);Files->Replace(SaveKind::Profile,true,envelope);
        TestTrue(TEXT("旧形式の絆を失わない"),Service->Load().Permanent==profile);
        Async(EAsyncExecution::TaskGraphMainThread,[this,run]{Window->SetContent(SNew(SRunScreen).InitialRun(run).SaveService(Service));}).Wait();
        const auto before=run->Checkpoint();const int actor=before.State.Recruited.front()+1;
        const auto actorId=FString::Printf(TEXT("social.actor.%d"),actor);
        if(!Click(TCHAR_TO_UTF8(*actorId)) || !Click("social.special") || !Click("social.special.cancel"))return;
        TestTrue(TEXT("取消では保存内容を変えない"),Service->Load().Permanent==profile && run->Checkpoint()==before);
        if(!Click("social.special"))return;
        Files->Fail=true;if(!Click("social.special.confirm"))return;
        TestTrue(TEXT("保存失敗で進行停止"),Driver->FindElement(By::Id("save.error"))->Exists());
        TestFalse(TEXT("再選択できない"),Driver->FindElement(By::Id("social.choice.0"))->Exists());
        const auto committed=run->Checkpoint();
        Files->Fail=false;if(!Click("save.retry"))return;
        TestTrue(TEXT("再試行はイベントを二重消費しない"),run->Checkpoint()==committed);
        const auto permanent=Service->Load().Permanent;
        TestTrue(TEXT("絆と専用交流完了を保存"),permanent.Bonds==profile.Bonds && permanent.SpecialConversations[actor-1]);
        if(!Click("save.suspend"))return;Reopen();
        if(!Click("title.resume"))return;
        TestTrue(TEXT("画面再生成後も完了保持"),Service->Load().Permanent==permanent);
        if(!Click("save.suspend") || !Click("title.start") || !Click("save.abandon.confirm") || !Click("role.confirm"))return;
        TestTrue(TEXT("次周開始でも完了保持"),Service->Load().Permanent==permanent);
    });
    It(TEXT("後日談確定の保存失敗は選び直さず再試行し終了と記録を同時確定する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(180),[this] {
        using namespace GirlsSpire;
        Profile profile;profile.Bonds.fill(80);profile.SpecialConversations.fill(true);
        auto run=std::make_shared<Run>(42,Difficulty::Beginner,HeroRole::Sword,profile);
        for(int step=0;step<24 && run->Snapshot().Phase==RunPhase::Route;++step){run->ChooseNode(run->Snapshot().Revision,0);run->CompleteEvent(run->Snapshot().Revision);}
        const int actor=run->Snapshot().Recruited.front()+1;
        Async(EAsyncExecution::TaskGraphMainThread,[this,run]{Window->SetContent(SNew(SRunScreen).InitialRun(run).SaveService(Service));}).Wait();
        if(!Click("result.continue"))return;
        const auto key=FString::Printf(TEXT("epilogue.select.%d"),actor);
        if(!TestTrue(TEXT("後日談選択を表示"),Driver->FindElement(By::Id(*key))->Exists()))return;
        if(!Click(TCHAR_TO_UTF8(*key)))return;
        Files->Fail=true;if(!Click("epilogue.confirm"))return;
        TestTrue(TEXT("終了の保存失敗は停止"),Driver->FindElement(By::Id("save.error"))->Exists());
        TestFalse(TEXT("別の後日談を選択できない"),Driver->FindElement(By::Id("epilogue.select.0"))->Exists());
        Files->Fail=false;if(!Click("save.retry"))return;
        const auto finished=Service->Load();
        TestTrue(TEXT("周回消去と記録が同時確定"),finished.Issue==SlotIssue::None && !finished.Run && finished.Permanent.Epilogues[actor]);
        TestEqual(TEXT("選んだ後日談だけ記録"),static_cast<int>(std::count(finished.Permanent.Epilogues.begin(),finished.Permanent.Epilogues.end(),true)),1);
        TestTrue(TEXT("絆と専用交流を維持"),finished.Permanent.Bonds==profile.Bonds && finished.Permanent.SpecialConversations==profile.SpecialConversations);
        Reopen();if(!Click("title.epilogues"))return;
        const auto record=FString::Printf(TEXT("epilogue.record.%d"),actor);
        if(!Click(TCHAR_TO_UTF8(*record)) || !Click("epilogue.back") || !Click("title.start") || !Click("role.confirm"))return;
        TestTrue(TEXT("再起動相当の再生成と次周でも記録保持"),Service->Load().Permanent==finished.Permanent);
    });
    for(int choice=0;choice<3;++choice)It(*FString::Printf(TEXT("救出返答%dを取消・中断再開・確定して時間を追加消費しない"),choice),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(180),[this,choice] {
        using namespace GirlsSpire;
        Profile profile;profile.Bonds.fill(50);auto run=std::make_shared<Run>(42,Difficulty::Beginner,HeroRole::Sword,profile);
        run->ChooseNode(0,0);auto battle=Encounter::Create(run->Snapshot(),run->RandomState()).Snapshot();battle.Outcome=BattleOutcome::Won;
        run->CompleteBattle(run->Snapshot().Revision,battle);const int candidate=run->Snapshot().Active.Candidate;
        Async(EAsyncExecution::TaskGraphMainThread,[this,run]{Window->SetContent(SNew(SRunScreen).InitialRun(run).SaveService(Service));}).Wait();
        if(!TestTrue(TEXT("救出後の返答を表示"),Driver->FindElement(By::Id("rescue.help"))->Exists()))return;
        if(!Click("save.suspend"))return;Reopen();if(!Click("title.resume"))return;
        const auto resumed=Service->Load();
        if(!TestTrue(TEXT("再開後も未返答データを保持"),resumed.Run && resumed.Run->Run.State.PendingRescue==candidate))return;
        TestTrue(TEXT("未返答から再開"),Driver->FindElement(By::Id("rescue.help"))->Exists());
        const auto before=Service->Load();const auto key=FString::Printf(TEXT("rescue.choice.%d"),choice);
        if(!Click(TCHAR_TO_UTF8(*key)) || !Click("rescue.cancel"))return;
        TestTrue(TEXT("取消では絆も時間も不変"),Service->Load().Run==before.Run && Service->Load().Permanent==before.Permanent);
        if(!Click(TCHAR_TO_UTF8(*key)))return;
        Files->Fail=true;if(!Click("rescue.confirm"))return;
        TestTrue(TEXT("返答保存の失敗で停止"),Driver->FindElement(By::Id("save.error"))->Exists());
        Files->Fail=false;if(!Click("save.retry"))return;
        const auto after=Service->Load();const int deltas[]={10,0,-5};
        if(after.Issue!=SlotIssue::None || !after.Run || after.Permanent.Bonds[candidate]!=50+deltas[choice])Diagnose();
        TestEqual(TEXT("選択した絆だけ反映"),after.Permanent.Bonds[candidate],50+deltas[choice]);
        TestTrue(TEXT("追加の日数・イベント消費なし"),after.Run && before.Run && after.Run->Run.State.Day==before.Run->Run.State.Day && after.Run->Run.State.EventIndex==before.Run->Run.State.EventIndex && after.Run->Run.RandomState==before.Run->Run.RandomState);
        TestTrue(TEXT("返答後は一度だけ解除"),after.Run && after.Run->Run.State.PendingRescue==-1 && after.Run->Run.State.Revision==before.Run->Run.State.Revision+1);
        TestTrue(TEXT("報酬へ進める"),Driver->FindElement(By::Id("reward.recipient.0"))->Exists());
        TestFalse(TEXT("同じ救出で返答を再表示しない"),Driver->FindElement(By::Id("rescue.help"))->Exists());
    });
    It(TEXT("確定世代の共通候補を取消・確認し全保全後に固定再試行する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(180),[this]{
        using namespace GirlsSpire;Profile profile;profile.Bonds.fill(55);Run run(42,Difficulty::Beginner,HeroRole::Sword,profile);const RunSaveState saved{run.Checkpoint(),{}};
        const auto rp=*EncodeRunPayload(saved),pp=*EncodeProfilePayload(profile);
        const std::array<std::string,5> originals={*EncodeSave({SaveKind::Run,3,rp}),*EncodeSave({SaveKind::Run,2,rp}),*EncodeSave({SaveKind::Profile,2,pp}),*EncodeSave({SaveKind::Profile,1,pp}),*EncodeSave({SaveKind::Run,3,{}})};
        for(int i=0;i<4;++i)if(!TestTrue(TEXT("実ディスクに世代不一致を準備"),Files->Replace(i<2?SaveKind::Run:SaveKind::Profile,(i%2)!=0,originals[i])))return;
        if(!TestTrue(TEXT("取引世代を準備"),Files->ReplaceJournal(originals[4])))return;Reopen();
        if(!TestTrue(TEXT("共通世代の確認操作"),Driver->FindElement(By::Id("save.generation.repair"))->Exists()))return;
        if(!Click("save.generation.repair"))return;
        const auto warning=Driver->FindElement(By::Id("save.generation.warning"))->GetText().ToString();
        TestTrue(TEXT("候補と最新観測世代・失われる変更を説明"),warning.Contains(TEXT("2")) && warning.Contains(TEXT("3")) && warning.Contains(TEXT("絆")) && warning.Contains(TEXT("戻")));
        if(!Click("save.generation.cancel"))return;
        for(int i=0;i<4;++i)TestTrue(TEXT("取消は原本不変"),Files->Read(i<2?SaveKind::Run:SaveKind::Profile,(i%2)!=0).Bytes==originals[i]);
        TestTrue(TEXT("取消で取引も不変"),Files->ReadJournal().Bytes==originals[4]);
        if(!Click("save.generation.repair"))return;Files->Fail=true;
        if(!Click("save.generation.confirm"))return;
        TestTrue(TEXT("書込失敗で候補固定と進行停止"),Service->HasPending() && Driver->FindElement(By::Id("save.error"))->Exists() && !Driver->FindElement(By::Id("title.start"))->Exists());
        Files->Fail=false;if(!Click("save.retry"))return;
        const auto loaded=Service->Load();TestTrue(TEXT("世代4に共通世代2の両側だけ復旧"),loaded.Issue==SlotIssue::None && loaded.Generation==4 && loaded.Run==saved && loaded.Permanent==profile);
        std::vector<std::string> archived;const auto folder=std::filesystem::path(*TestRoot)/"Corrupt";
        if(std::filesystem::exists(folder))for(const auto& entry:std::filesystem::recursive_directory_iterator(folder))if(entry.is_regular_file()){
            std::ifstream input(entry.path(),std::ios::binary);archived.emplace_back(std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>());
        }
        for(const auto& bytes:originals)TestTrue(TEXT("全4原本と元取引の内容を実保全"),std::find(archived.begin(),archived.end(),bytes)!=archived.end());
        if(!Click("title.resume"))return;TestTrue(TEXT("復旧後の実画面再開"),Driver->FindElement(By::Id("route.node.0"))->Exists());
    });
    It(TEXT("確定世代の確認後に原本が変われば再確認を要求する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this]{
        using namespace GirlsSpire;Profile profile;Run run(42,Difficulty::Beginner,HeroRole::Sword,profile);const RunSaveState saved{run.Checkpoint(),{}};
        const auto rp=*EncodeRunPayload(saved),pp=*EncodeProfilePayload(profile);
        Files->Replace(SaveKind::Run,false,*EncodeSave({SaveKind::Run,3,rp}));Files->Replace(SaveKind::Run,true,*EncodeSave({SaveKind::Run,2,rp}));
        Files->Replace(SaveKind::Profile,false,*EncodeSave({SaveKind::Profile,2,pp}));Files->Replace(SaveKind::Profile,true,*EncodeSave({SaveKind::Profile,1,pp}));Files->ReplaceJournal(*EncodeSave({SaveKind::Run,3,{}}));Reopen();
        if(!TestTrue(TEXT("共通候補の操作がある"),Driver->FindElement(By::Id("save.generation.repair"))->Exists()) || !Click("save.generation.repair"))return;
        const auto external=*EncodeSave({SaveKind::Run,4,rp});Files->Replace(SaveKind::Run,false,external);
        if(!Click("save.generation.confirm"))return;
        TestTrue(TEXT("変更を通知して保存しない"),Driver->FindElement(By::Id("save.generation.changed"))->Exists() && !Service->HasPending() && Files->Read(SaveKind::Run,false).Bytes==external);
        if(!Click("save.generation.reload") || !Click("save.generation.repair"))return;
        TestTrue(TEXT("新しい観測世代で再確認"),Driver->FindElement(By::Id("save.generation.warning"))->GetText().ToString().Contains(TEXT("4")));
    });
    It(TEXT("確定世代の共通ペアが無ければ原本を保持して停止する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this]{
        using namespace GirlsSpire;Profile profile;Run run(42,Difficulty::Beginner,HeroRole::Sword,profile);const RunSaveState saved{run.Checkpoint(),{}};
        const auto rp=*EncodeRunPayload(saved),pp=*EncodeProfilePayload(profile);
        const std::array<std::string,4> originals={*EncodeSave({SaveKind::Run,3,rp}),*EncodeSave({SaveKind::Run,3,rp}),*EncodeSave({SaveKind::Profile,2,pp}),*EncodeSave({SaveKind::Profile,1,pp})};
        for(int i=0;i<4;++i)if(!TestTrue(TEXT("共通世代なしを実ディスクに準備"),Files->Replace(i<2?SaveKind::Run:SaveKind::Profile,(i%2)!=0,originals[i])))return;
        const auto journal=*EncodeSave({SaveKind::Run,3,{}});Files->ReplaceJournal(journal);Reopen();
        TestTrue(TEXT("共通ペアなしの停止を通知"),Driver->FindElement(By::Id("save.generation.unavailable"))->Exists());
        for(const char* id:{"save.generation.repair","save.reset.run","save.reset.profile","title.start","title.resume"})TestFalse(TEXT("初期化・復旧・進行を許可しない"),Driver->FindElement(By::Id(id))->Exists());
        if(!Click("save.reload"))return;
        for(int i=0;i<4;++i)TestTrue(TEXT("再読込後も全原本が不変"),Files->Read(i<2?SaveKind::Run:SaveKind::Profile,(i%2)!=0).Bytes==originals[i]);
        TestTrue(TEXT("取引も不変・固定保存なし"),Files->ReadJournal().Bytes==journal && !Service->HasPending());
    });
    AfterEach([this] {
        if(HasAnyErrors() && !PreserveFailure)Diagnose();
        Driver.Reset();
        if(Window.IsValid()) {
            if(PreserveFailure) {
                TArray<FColor> pixels;FIntVector size;
                if(FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),pixels,size)) {
                    TArray64<uint8> png;FImageUtils::PNGCompressImageArray(size.X,size.Y,pixels,png);
                    FFileHelper::SaveArrayToFile(png,*(TestRoot/TEXT("failure.png")));
                }
            }
            Window->SetContent(SNullWidget::NullWidget);
            FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());
        }
        Window.Reset();
        Service.reset();Files.reset();IAutomationDriverModule::Get().Disable();
        const auto prefix=FPaths::ConvertRelativePathToFull(FString(FPlatformProcess::UserTempDir())/TEXT("GirlsSpireTests/Resume"))+TEXT("/");
        if(!PreserveFailure && TestRoot.StartsWith(prefix)) IFileManager::Get().DeleteDirectory(*TestRoot,false,true);
    });
}
