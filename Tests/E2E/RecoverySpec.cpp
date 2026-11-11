#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Widgets/SWindow.h"
#include "Presentation/RunScreen.h"
#include "Application/Encounter.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

BEGIN_DEFINE_SPEC(FGirlsSpireRecoveryUiSpec,"GirlsSpire.UI.Recovery",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
    int CaptureIndex=0;
    bool Click(const char* id);
    std::shared_ptr<GirlsSpire::Run> PrepareRest(int wounds);
END_DEFINE_SPEC(FGirlsSpireRecoveryUiSpec)
bool FGirlsSpireRecoveryUiSpec::Click(const char* id)
{
    auto target=Driver->FindElement(By::Id(id));
    if(!target->IsVisible()) {
        auto scroll=Driver->FindElement(By::Id("run.scroll"));
        const bool top=scroll->ScrollToBeginning();Async(EAsyncExecution::TaskGraphMainThread,[]{}).Wait();
        if(!TestTrue(TEXT("上へスクロール"),top)) return false;
        if(!target->IsVisible()) {
            const bool found=scroll->ScrollToEndUntil(By::Id(id));Async(EAsyncExecution::TaskGraphMainThread,[]{}).Wait();
            if(!TestTrue(TEXT("対象までスクロール"),found)) return false;
        }
    }
    return TestTrue(*FString::Printf(TEXT("回復操作 %hs"),id),ClickAndDrain(target));
}
std::shared_ptr<GirlsSpire::Run> FGirlsSpireRecoveryUiSpec::PrepareRest(int wounds)
{
    using namespace GirlsSpire;
    auto run=std::make_shared<Run>(42,Difficulty::Beginner,HeroRole::Sword);
    run->ChooseNode(0,0);run->CompleteEvent(run->Snapshot().Revision);
    int completed=0;
    for(int step=0;step<24 && run->Snapshot().Phase==RunPhase::Route;++step) {
        int index=0;
        if(completed>=FMath::Max(1,wounds)) {
            for(int i=0;i<static_cast<int>(run->Snapshot().Choices.size());++i) if(run->Snapshot().Choices[i].Kind==NodeKind::Rest) {index=i;break;}
        }
        run->ChooseNode(run->Snapshot().Revision,index);
        if(completed>=FMath::Max(1,wounds) && run->Snapshot().Active.Kind==NodeKind::Rest) return run;
        const auto kind=run->Snapshot().Active.Kind;
        const bool combat=kind==NodeKind::Battle || kind==NodeKind::Elite || kind==NodeKind::Rescue || kind==NodeKind::MidBoss || kind==NodeKind::FinalBoss;
        if(combat && completed<FMath::Max(1,wounds)) {
            auto end=Encounter::Create(run->Snapshot(),0).Snapshot();end.Outcome=BattleOutcome::Won;end.Units[0].Hp=wounds>0?0:1;
            if(!run->CompleteBattle(run->Snapshot().Revision,end)) return nullptr;
            run->ChooseRecipient(run->Snapshot().Revision,0);
            int reward=0;while(run->Snapshot().Rewards[reward]==Reward::MaxHp) ++reward;
            run->ClaimReward(run->Snapshot().Revision,reward);++completed;
            if(run->Snapshot().PendingRescue>=0)run->ConverseRescue(run->Snapshot().Revision,ConversationChoice::Neutral);
        } else run->CompleteEvent(run->Snapshot().Revision);
    }
    return nullptr;
}
void FGirlsSpireRecoveryUiSpec::Define()
{
    BeforeEach([this] {
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        IConsoleManager::Get().ProcessUserConsoleInput(TEXT("GirlsSpire.Preview"),*GLog,nullptr);
        for(const auto& w:FSlateApplication::Get().GetTopLevelWindows()) if(w->GetTitle().ToString()==TEXT("Spiral RPG — 周回プレビュー")) Window=w;
        Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    for(int wounds:{0,1,4}) It(*FString::Printf(TEXT("休息で負傷%dの予告取消と回復または治療"),wounds),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this,wounds] {
        const auto run=PrepareRest(wounds);if(!TestTrue(TEXT("初期状態準備"),run!=nullptr)) return;
        Async(EAsyncExecution::TaskGraphMainThread,[this,run]{Window->SetContent(SNew(SRunScreen).InitialRun(run));}).Wait();
        const auto id=wounds==0?"rest.heal":"rest.treat.0";
        if(!TestTrue(TEXT("休息選択肢"),Driver->FindElement(By::Id(id))->Exists())) return;
        if(wounds==0) TestFalse(TEXT("負傷なしの治療は選べない"),Driver->FindElement(By::Id("rest.treat.0"))->Exists());
        if(!Click(id)) return;
        const auto preview=Driver->FindElement(By::Id("rest.preview"))->GetText().ToString();
        TestTrue(TEXT("効果を予告"),preview.Contains(wounds==0?TEXT("17→57"):wounds==1?TEXT("1→0"):TEXT("4→3")));
        if(!Click("rest.cancel")) return;
        TestTrue(TEXT("取消はイベントを消費しない"),Driver->FindElement(By::Id(id))->Exists());
        if(!Click(id) || !Click("rest.confirm")) return;
        TestFalse(TEXT("二重確定不可"),Driver->FindElement(By::Id("rest.confirm"))->Exists());
        const auto result=Driver->FindElement(By::Id("run.growth"))->GetText().ToString();
        // 4回の実戦でLv3となるため、重傷復帰25/50→治療25/60。
        TestTrue(TEXT("回復結果表示"),result.Contains(wounds==0?TEXT("HP57/80"):wounds==1?TEXT("HP35/80"):TEXT("HP25/60")));
    });
    It(TEXT("実戦で受けた被害を回復後に次戦へ引き継ぐ"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(180),[this] {
        for(const char* id:{"title.start","role.confirm","route.node.0","preview.battle","battle.cell.3.1","battle.move.confirm","battle.wait.south","battle.wait.south"}) if(!Click(id)) return;
        for(int turn=0;turn<8;++turn) {
            if(Driver->FindElement(By::Id("battle.won"))->Exists()) break;
            if(!Click("battle.target.100") || !Click("battle.confirm")) return;
            if(Driver->FindElement(By::Id("battle.won"))->Exists()) break;
            if(!Click("battle.wait.south")) return;
        }
        if(!Click("battle.return")) return;
        if(Driver->FindElement(By::Id("rescue.help"))->Exists())if(!Click("rescue.choice.1") || !Click("rescue.confirm"))return;
        TestTrue(TEXT("勝利後72まで回復"),Driver->FindElement(By::Id("run.growth"))->GetText().ToString().Contains(TEXT("HP72/80")));
        for(const char* id:{"reward.recipient.0","reward.choice.0","reward.confirm","route.node.2","preview.battle"}) if(!Click(id)) return;
        TestEqual(TEXT("次戦で全快しない"),Driver->FindElement(By::Id("battle.celllabel.1.1"))->GetText().ToString(),FString(TEXT("味\n72")));
    });
    AfterEach([this] {
        FSlateApplication::Get().CloseToolTip();
        if(Window.IsValid()) {
            TArray<FColor> pixels;FIntVector size;
            if(TestTrue(TEXT("回復画面撮影"),FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),pixels,size))) {
                TArray64<uint8> png;FImageUtils::PNGCompressImageArray(size.X,size.Y,pixels,png);
                const auto folder=FPaths::ProjectSavedDir()/TEXT("Screenshots");IFileManager::Get().MakeDirectory(*folder,true);
                TestTrue(TEXT("撮影保存"),FFileHelper::SaveArrayToFile(png,*(folder/FString::Printf(TEXT("Recovery-%d.png"),++CaptureIndex))));
            }
        }
        Driver.Reset();IAutomationDriverModule::Get().Disable();
        if(Window.IsValid()){FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}
#if defined(GIRLS_SPIRE_COVERAGE)
        TestEqual(TEXT("回復UI計測"),__llvm_profile_write_file(),0);
#endif
    });
}
