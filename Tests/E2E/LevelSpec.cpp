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

BEGIN_DEFINE_SPEC(FGirlsSpireLevelUiSpec,"GirlsSpire.UI.Level",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
    int CaptureIndex=0;
    bool Click(const char* id);
    bool Win();
    std::shared_ptr<GirlsSpire::Run> Prepare(int level);
END_DEFINE_SPEC(FGirlsSpireLevelUiSpec)
bool FGirlsSpireLevelUiSpec::Click(const char* id)
{
    auto target=Driver->FindElement(By::Id(id));
    // 外側を盤面上のホイールで戻し、その後は内側の操作欄だけをスクロールする。
    const FString controlId=UTF8_TO_TCHAR(id);
    auto battleScroll=Driver->FindElement(By::Id("battle.scroll"));
    if(battleScroll->Exists() && (controlId.StartsWith(TEXT("battle.")) || controlId.StartsWith(TEXT("intent.")))) {
        Driver->FindElement(By::Id("battle.cell.0.7"))->ScrollBy(8);DrainSlateLayout();
        battleScroll->ScrollBy(12);DrainSlateLayout();
        for(int step=0;step<10 && !target->IsVisible();++step){battleScroll->ScrollBy(-3);DrainSlateLayout();}
    } else if(!target->IsVisible()) {
        auto scroll=Driver->FindElement(By::Id("run.scroll"));
        scroll->ScrollToBeginning();Async(EAsyncExecution::TaskGraphMainThread,[]{}).Wait();
        if(!target->IsVisible()) {scroll->ScrollToEndUntil(By::Id(id));Async(EAsyncExecution::TaskGraphMainThread,[]{}).Wait();}
    }
    return TestTrue(*FString::Printf(TEXT("レベル操作 %hs"),id),ClickAndDrain(target));
}
bool FGirlsSpireLevelUiSpec::Win()
{
    for(int turn=0;turn<50;++turn) {
        if(Driver->FindElement(By::Id("battle.won"))->Exists()) {
            if(!Click("battle.return"))return false;
            if(Driver->FindElement(By::Id("rescue.help"))->Exists())return Click("rescue.choice.1") && Click("rescue.confirm");
            return true;
        }
        if(Driver->FindElement(By::Id("battle.lost"))->Exists()) break;
        if(Driver->FindElement(By::Id("intent.execute"))->Exists()) {
            if(!Click("intent.execute")) return false;
        } else {
            for(int enemy=100;enemy<=102;++enemy) {
                const auto id=FString::Printf(TEXT("battle.target.%d"),enemy);
                const auto target=Driver->FindElement(By::Id(*id));
                if(!target->Exists() || !target->IsInteractable()) continue;
                if(!TestTrue(TEXT("攻撃対象"),ClickAndDrain(target))) return false;
                if(Driver->FindElement(By::Id("battle.confirm"))->IsInteractable()) {if(!Click("battle.confirm")) return false;break;}
            }
        }
        if(!Driver->FindElement(By::Id("battle.won"))->Exists() && !Click("battle.wait.south")) return false;
    }
    return TestTrue(TEXT("実戦勝利"),false);
}
std::shared_ptr<GirlsSpire::Run> FGirlsSpireLevelUiSpec::Prepare(int level)
{
    using namespace GirlsSpire;
    for(int seed=0;seed<100;++seed) {
        auto run=std::make_shared<Run>(seed,Difficulty::Beginner,HeroRole::Matchlock);
        for(int step=0;step<24 && run->Snapshot().Phase==RunPhase::Route;++step) {
            int index=0;
            for(int i=0;i<static_cast<int>(run->Snapshot().Choices.size());++i) if(run->Snapshot().Choices[i].Kind==NodeKind::Elite) index=i;
            run->ChooseNode(run->Snapshot().Revision,index);
            const auto state=run->Snapshot();const auto kind=state.Active.Kind;
            const int amount=kind==NodeKind::Battle || kind==NodeKind::Rescue?100:kind==NodeKind::Elite?150:kind==NodeKind::MidBoss || kind==NodeKind::FinalBoss?200:0;
            if(amount==0){run->CompleteEvent(state.Revision);continue;}
            if(state.Levels[0].Level==level && (level==10 || state.Levels[0].Experience+amount>=200)) return run;
            auto end=Encounter::Create(state,0).Snapshot();end.Outcome=BattleOutcome::Won;
            run->CompleteBattle(state.Revision,end);run->ChooseRecipient(run->Snapshot().Revision,0);run->ClaimReward(run->Snapshot().Revision,0);
            if(run->Snapshot().PendingRescue>=0)run->ConverseRescue(run->Snapshot().Revision,ConversationChoice::Neutral);
        }
    }
    return nullptr;
}
void FGirlsSpireLevelUiSpec::Define()
{
    BeforeEach([this] {
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        IConsoleManager::Get().ProcessUserConsoleInput(TEXT("GirlsSpire.Preview"),*GLog,nullptr);
        for(const auto& w:FSlateApplication::Get().GetTopLevelWindows()) if(w->GetTitle().ToString()==TEXT("Spiral RPG — 周回プレビュー")) Window=w;
        Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    It(TEXT("新規開始から実戦経験値とレベル成長を得る"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(180),[this] {
        for(const char* id:{"title.start","role.3","role.confirm"}) if(!Click(id)) return;
        if(!TestTrue(TEXT("初期レベル表示"),Driver->FindElement(By::Id("run.growth"))->GetText().ToString().Contains(TEXT("Lv1 経験値0/200")))) return;
        for(const char* id:{"route.node.0","preview.battle"}) if(!Click(id)) return;
        if(!Win()) return;
        auto text=Driver->FindElement(By::Id("run.growth"))->GetText().ToString();
        TestTrue(TEXT("主人公100・新規仲間0"),text.Contains(TEXT("Lv1 経験値100/200")) && text.Contains(TEXT("Lv1 経験値0/200")));
        for(const char* id:{"reward.recipient.0","reward.choice.0","reward.confirm","route.node.2","preview.battle"}) if(!Click(id)) return;
        if(!Win()) return;
        text=Driver->FindElement(By::Id("run.growth"))->GetText().ToString();
        TestTrue(TEXT("強敵150でLv2・余剰50、仲間150"),text.Contains(TEXT("Lv2 経験値50/200")) && text.Contains(TEXT("Lv1 経験値150/200")));
        TestTrue(TEXT("基礎成長表示"),text.Contains(TEXT("攻撃＋1 防御＋1")));
        for(const char* id:{"reward.recipient.0","reward.choice.0","reward.confirm","route.node.0","preview.battle","battle.target.100"}) if(!Click(id)) return;
        TestTrue(TEXT("次戦へ攻撃成長反映"),Driver->FindElement(By::Id("battle.preview"))->GetText().ToString().Contains(TEXT("17")));
    });
    for(int level:{9,10}) It(*FString::Printf(TEXT("レベル%dから実戦勝利後も上限10と報酬を維持"),level),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(240),[this,level] {
        const auto run=Prepare(level);if(!TestTrue(TEXT("上限直前の初期状態"),run!=nullptr)) return;
        Async(EAsyncExecution::TaskGraphMainThread,[this,run]{Window->SetContent(SNew(SRunScreen).InitialRun(run));}).Wait();
        if(!Click("preview.battle") || !Win()) return;
        const auto text=Driver->FindElement(By::Id("run.growth"))->GetText().ToString();
        TestTrue(TEXT("上限表示"),text.Contains(TEXT("Lv10 経験値MAX")));
        TestFalse(TEXT("レベル11にしない"),text.Contains(TEXT("Lv11")));
        for(const char* id:{"reward.recipient.0","reward.choice.0","reward.confirm"}) if(!Click(id)) return;
        TestFalse(TEXT("報酬は通常どおり取得でき二重確定不可"),Driver->FindElement(By::Id("reward.confirm"))->Exists());
        Driver->FindElement(By::Id("run.scroll"))->ScrollToBeginning();Async(EAsyncExecution::TaskGraphMainThread,[]{}).Wait();
    });
    AfterEach([this] {
        FSlateApplication::Get().CloseToolTip();
        if(Window.IsValid()) {
            TArray<FColor> pixels;FIntVector size;
            if(TestTrue(TEXT("レベル画面撮影"),FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),pixels,size))) {
                TArray64<uint8> png;FImageUtils::PNGCompressImageArray(size.X,size.Y,pixels,png);
                const auto folder=FPaths::ProjectSavedDir()/TEXT("Screenshots");IFileManager::Get().MakeDirectory(*folder,true);
                TestTrue(TEXT("撮影保存"),FFileHelper::SaveArrayToFile(png,*(folder/FString::Printf(TEXT("Level-%d.png"),++CaptureIndex))));
            }
        }
        Driver.Reset();IAutomationDriverModule::Get().Disable();
        if(Window.IsValid()){FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}
#if defined(GIRLS_SPIRE_COVERAGE)
        TestEqual(TEXT("レベルUI計測"),__llvm_profile_write_file(),0);
#endif
    });
}
