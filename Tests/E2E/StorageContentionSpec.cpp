#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "Presentation/RunScreen.h"
#include "Infrastructure/DiskSlotFiles.h"
#include "Application/Encounter.h"
#include "Windows/WindowsHWrapper.h"
namespace {
struct FStorageContentionState {
    HANDLE Lock=INVALID_HANDLE_VALUE;
    unsigned Waits=0;
    int Mode=0;
    void Release(){if(Lock!=INVALID_HANDLE_VALUE){CloseHandle(Lock);Lock=INVALID_HANDLE_VALUE;}}
    ~FStorageContentionState(){Release();}
};
}
BEGIN_DEFINE_SPEC(FGirlsSpireStorageContentionSpec,"GirlsSpire.UI.StorageContention",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
    std::shared_ptr<GirlsSpire::DiskSlotFiles> Files;
    std::shared_ptr<GirlsSpire::SaveTransaction> Service;
END_DEFINE_SPEC(FGirlsSpireStorageContentionSpec)
void FGirlsSpireStorageContentionSpec::Define()
{
    BeforeEach([this] {
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        Window=SNew(SWindow).Title(FText::FromString(TEXT("Girls Spire — 保存競合テスト"))).ClientSize(FVector2D(1100,850));
        FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    for(int phase=0;phase<2;++phase)for(int mode=0;mode<3;++mode)It(*FString::Printf(TEXT("保存段階%dの実ファイル競合%dで進行と同内容再試行を守る"),phase,mode),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(90),[this,phase,mode] {
        using namespace GirlsSpire;
        const auto state=std::make_shared<FStorageContentionState>();state->Mode=mode;
        // 制御した共有競合を同期フォルダ等の外部競合から分離する。
        // 通常ゲームの保存先は別途Runtime/別プロセス再開試験で検証する。
        const auto folder=TEXT("girls-spire-ui-contention-")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
        const auto directory=std::filesystem::temp_directory_path()/std::filesystem::path(*folder);
        std::error_code directoryError;
        if(!TestTrue(TEXT("専用の一時保存先を新規作成"),std::filesystem::create_directory(directory,directoryError) && !directoryError))return;
        AddInfo(TEXT("保存競合試験の隔離先: ")+FString(directory.c_str()));
        Files=std::make_shared<DiskSlotFiles>(directory,[state](unsigned){++state->Waits;if(state->Mode==0 || (state->Mode==1 && state->Waits==2))state->Release();});
        Service=std::make_shared<SaveTransaction>(*Files,*Files);
        auto run=std::make_shared<Run>(42,Difficulty::Beginner,HeroRole::Sword);
        if(phase==1)run->ChooseNode(run->Snapshot().Revision,0);
        if(!TestTrue(TEXT("通常の取引で初期保存を用意"),Service->Prepare(RunSaveState{run->Checkpoint(),{}},run->ProfileState())==SlotIssue::None && Service->Retry()==SlotIssue::None))return;
        Async(EAsyncExecution::TaskGraphMainThread,[this,run]{Window->SetContent(SNew(SRunScreen).InitialRun(run).SaveService(Service).DevelopmentTools(false));}).Wait();DrainSlateLayout();
        const auto before=Files->Read(SaveKind::Run,false);
        if(!TestTrue(TEXT("有効な本体保存を用意"),before.Bytes.has_value()))return;
        if(!Files->Read(SaveKind::Run,true).Bytes && !TestTrue(TEXT("同じ確定記録のバックアップを用意"),Files->Replace(SaveKind::Run,true,*before.Bytes)))return;
        state->Waits=0;
        state->Lock=CreateFileW((directory/"run.bak").c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(!TestTrue(TEXT("実ハンドルでバックアップ置換を競合させる"),state->Lock!=INVALID_HANDLE_VALUE))return;
        auto expected=*run;if(phase==0)expected.ChooseNode(expected.Snapshot().Revision,0);
        std::optional<BattleCheckpoint> expectedCombat;
        if(phase==1)expectedCombat=Encounter::Create(expected.Snapshot(),expected.RandomState(),expected.ProfileState()).Checkpoint();
        const char* ready=phase==0?"battle.start":"battle.ultimate.select";
        const auto click=[&](const char* id) {
            const auto target=Driver->FindElement(By::Id(id));
            if(!target->IsVisible()){const auto scroll=Driver->FindElement(By::Id("run.scroll"));scroll->ScrollToBeginning();DrainSlateLayout();if(!target->IsVisible()){scroll->ScrollToEndUntil(By::Id(id));DrainSlateLayout();}}
            return TestTrue(*FString::Printf(TEXT("マウス操作 %hs"),id),ClickAndDrain(target));
        };
        if(!click(phase==0?"route.node.0":"battle.start"))return;
        TestTrue(TEXT("確定した結果とRNGを作り直さない"),run->Checkpoint()==expected.Checkpoint());
        if(mode==2) {
            TestTrue(TEXT("恒常競合では保存停止だけを表示"),Driver->FindElement(By::Id("save.error"))->Exists() && !Driver->FindElement(By::Id(ready))->Exists() && !Driver->FindElement(By::Id("route.node.0"))->Exists());
            TestTrue(TEXT("旧本体と未確定取引を保全"),Files->Read(SaveKind::Run,false).Bytes==before.Bytes && Service->HasPending());
            TestTrue(TEXT("有限の内部再試行を行う"),state->Waits==2);
            state->Release();if(!click("save.retry"))return;
        } else TestTrue(TEXT("短期競合は有限再試行で回復"),!Driver->FindElement(By::Id("save.error"))->Exists() && state->Waits==static_cast<unsigned>(mode+1));
        if(!TestFalse(*FString::Printf(TEXT("明示再試行後に未確定取引を残さない（MoveError=%u/Attempts=%u）"),Files->LastMoveError(),Files->LastMoveAttempts()),Service->HasPending()))return;
        const auto saved=Service->Load();
        TestTrue(*FString::Printf(TEXT("保存診断 ready=%d issue=%d run=%d pending=%d stop=%d"),Driver->FindElement(By::Id(ready))->Exists(),static_cast<int>(saved.Issue),saved.Run.has_value(),Service->HasPending(),Driver->FindElement(By::Id("save.error"))->Exists()),Driver->FindElement(By::Id(ready))->Exists() && saved.Issue==SlotIssue::None && saved.Run && !Service->HasPending());
        TestTrue(TEXT("画面へ戻り同じ確定結果だけを保存"),Driver->FindElement(By::Id(ready))->Exists() && saved.Issue==SlotIssue::None && saved.Run && saved.Run->Run==expected.Checkpoint() && run->Checkpoint()==expected.Checkpoint() && !Service->HasPending());
        if(expectedCombat)TestTrue(TEXT("戦闘開始の盤面/HP/手番/戦闘乱数と仲間抽選位置も不変"),saved.Run && saved.Run->Combat && saved.Run->Combat->Battle==*expectedCombat && saved.Run->Combat->Control.ChoiceRandomState==expectedCombat->RandomState);
        state->Release();
    });
    AfterEach([this] {
        Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}
        Service.reset();Files.reset();IAutomationDriverModule::Get().Disable();
    });
}
