#include "Misc/AutomationTest.h"
#include "Presentation/RunScreen.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Widgets/SWindow.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "HAL/FileManager.h"
#if defined(GIRLS_SPIRE_COVERAGE)
extern "C" int __llvm_profile_write_file();
#endif

#include "Infrastructure/DiskSlotFiles.h"
#include "Widgets/SNullWidget.h"
#include "AI/Intent.h"
#include <algorithm>
#include <set>

namespace {
// 実ディスクに触れず、通常ゲームと同じ保存取引だけを使う。
class RunPlayMemory : public GirlsSpire::SlotFiles,public GirlsSpire::JournalFiles {
public:
    std::array<std::array<std::optional<std::string>,2>,2> Slots{};
    std::optional<std::string> Journal;
    GirlsSpire::SlotBytes Read(GirlsSpire::SaveKind kind,bool backup)override{return {false,Slots[static_cast<int>(kind)-1][backup]};}
    GirlsSpire::SlotBytes ReadJournal()override{return {false,Journal};}
    bool Replace(GirlsSpire::SaveKind kind,bool backup,std::string_view bytes)override{Slots[static_cast<int>(kind)-1][backup]=std::string(bytes);return true;}
    bool ReplaceJournal(std::string_view bytes)override{Journal=std::string(bytes);return true;}
};
// 実測(6回で145～224ステップ・41～64秒、タイムアウト300秒)に対する余裕を持たせた上限。無限ループ検出用。
constexpr int RunPlayMaxSteps=1024;
constexpr std::string_view RunPlayBoardCellPrefix="battle.cell.",RunPlayBoardUnitPrefix="battle.unit.select.",RunPlayFieldMenuPrefix="battle.field.";
}

// 開発用の勝利入力を使わず、通常ゲーム(DevelopmentTools=false)の画面を実マウス操作だけで12日進める。
// 戦闘中の主人公の行動は CampaignSpec と同じく共通ルールの最有力候補を選び、仲間は作戦予告を実行する。
BEGIN_DEFINE_SPEC(FGirlsSpireRunSpec,"GirlsSpire.UI.Run",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
    bool Click(const char* id){
        const auto target=Driver->FindElement(By::Id(id));
        if(!TestTrue(UTF8_TO_TCHAR(id),target->Exists()))return false;
        const std::string_view key(id);
        // 盤面のマス・駒・行動メニューは盤面内スクロールの外にあるため、欄へのホイール調整を省く(CampaignSpecと同じ)。
        const bool direct=key.starts_with(RunPlayBoardCellPrefix) || key.starts_with(RunPlayBoardUnitPrefix) || key.starts_with(RunPlayFieldMenuPrefix);
        if(!direct){
            const auto scroll=Driver->FindElement(By::Id(Driver->FindElement(By::Id("battle.field.root"))->Exists()?"battle.scroll":"run.scroll"));
            for(int wheel=0;wheel<80;++wheel){
                const auto top=scroll->GetAbsolutePosition().Y,bottom=top+scroll->GetSize().Y;
                const auto y=target->GetAbsolutePosition().Y,h=target->GetSize().Y;
                if(y>=top+2 && y+h<=bottom-2 && target->IsVisible())break;
                scroll->ScrollBy(y<top+2?1.f:-1.f);DrainSlateLayout();
            }
        }
        return TestTrue(UTF8_TO_TCHAR(id),ClickAndDrain(target));
    }
END_DEFINE_SPEC(FGirlsSpireRunSpec)

void FGirlsSpireRunSpec::Define()
{
    BeforeEach([this]{
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        Window=SNew(SWindow).ClientSize(FVector2D(1280,900));
        FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    It(TEXT("UIから開始し実戦で12日を完走して結果からタイトルへ戻る"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(300),[this]{
        using namespace GirlsSpire;
        const auto files=std::make_shared<RunPlayMemory>();
        const auto service=std::make_shared<SaveTransaction>(*files,*files);
        Async(EAsyncExecution::TaskGraphMainThread,[this,service]{Window->SetContent(SNew(SRunScreen).SaveService(service).DevelopmentTools(false).FullField(true));}).Wait();DrainSlateLayout();
        // 周回の開始そのものを画面から行う(初期状態の注入なし)。
        if(!Click("title.start") || !Click("difficulty.0") || !Click("role.2") || !Click("role.confirm"))return;
        std::set<int> days;int battles=0,midBosses=0,finalBosses=0,playerActions=0;
        // 通常のシードは保存されない。開始直後の乱数状態を再現用の手掛かりとして残す。
        FString seedText=TEXT("開始時の乱数状態: 読み取れず");
        for(int step=0;step<RunPlayMaxSteps;++step){
            const auto saved=service->Load();
            if(!TestTrue(TEXT("各操作後に確定済み取引を読み取れる"),saved.Issue==SlotIssue::None && saved.Run.has_value()))return;
            if(step==0){
                seedText=FString::Printf(TEXT("開始時の乱数状態: %llu"),static_cast<unsigned long long>(saved.Run->Run.RandomState));
                AddInfo(seedText);
            }
            const auto& state=saved.Run->Run.State;days.insert(state.Day);
            if(saved.Run->Combat){
                const auto restored=Battle::Restore(saved.Run->Combat->Battle);
                if(!TestTrue(TEXT("通常戦闘の保存を復元"),restored.has_value()))return;
                auto battle=*restored;
                const auto restoredControl=CombatControl::Restore(saved.Run->Combat->Control,battle);
                if(!TestTrue(TEXT("固定信頼と抽選状態を復元"),restoredControl.has_value()))return;
                const auto& current=battle.Snapshot();
                if(current.Outcome!=BattleOutcome::Ongoing){
                    if(!TestTrue(TEXT("戦闘に実際に勝利 ")+seedText,current.Outcome==BattleOutcome::Won) || !Click("battle.return"))return;
                    continue;
                }
                const auto actor=std::find_if(current.Units.begin(),current.Units.end(),[&](const auto& unit){return unit.Id==current.ActiveId;});
                if(!TestTrue(TEXT("画面はプレイヤー側の手番で止まる"),actor!=current.Units.end() && actor->Side==Faction::Player))return;
                if(actor->Id==0){
                    const auto ranked=RankIntentActions(battle,{});
                    if(!TestTrue(TEXT("主人公の合法操作候補"),!ranked.empty()))return;
                    const auto action=ranked.front();
                    const auto occupied=std::find_if(current.Units.begin(),current.Units.end(),[&](const auto& unit){return unit.Hp>0 && unit.At==action.Target;});
                    const auto cell=occupied==current.Units.end()?FString::Printf(TEXT("battle.cell.%d.%d"),action.Target.X,action.Target.Y):FString::Printf(TEXT("battle.unit.select.%d"),occupied->Id);
                    if(action.Kind==BasicActionKind::Wait){if(!Click("battle.wait.south"))return;}
                    else{
                        if(action.Kind==BasicActionKind::Move){if(!Click("battle.field.move"))return;}
                        else if(action.Kind==BasicActionKind::Attack){if(!Click("battle.field.attack"))return;}
                        else{const auto skill=FString::Printf(TEXT("battle.skill.%d"),static_cast<int>(action.Ability));if(!Click(TCHAR_TO_UTF8(*skill)))return;}
                        if(!Click(TCHAR_TO_UTF8(*cell)))return;
                        const char* confirm=action.Kind==BasicActionKind::Move?"battle.move.confirm":action.Kind==BasicActionKind::Attack?"battle.confirm":"battle.skill.confirm";
                        if(!Click(confirm))return;
                    }
                }else if(!Click("intent.execute"))return;
                ++playerActions;
                continue;
            }
            if(state.PendingRescue>=0){if(!Click("rescue.choice.0") || !Click("rescue.confirm"))return;continue;}
            if(state.Phase==RunPhase::Won){
                if(!TestTrue(TEXT("12日/加入4人/両ボスを実戦で経由し周回成功 ")+seedText,days.size()==12 && state.Day==12 && state.Recruited.size()==4 && midBosses==1 && finalBosses==1 && battles>0 && playerActions>0))return;
                TestTrue(TEXT("周回成功を画面で確認"),Driver->FindElement(By::Id("result.won"))->Exists());
                TestTrue(TEXT("救出4人を画面で確認"),Driver->FindElement(By::Id("run.party"))->GetText().ToString().Contains(TEXT("仲間4人")));
                if(!Click("result.continue") || !Click("epilogue.select.0") || !Click("epilogue.confirm") || !Click("epilogue.back"))return;
                TestTrue(TEXT("開始画面へ復帰し周回は終了"),Driver->FindElement(By::Id("title.start"))->Exists() && !service->Load().Run && service->Load().Permanent.VictoryRecorded);
                AddInfo(FString::Printf(TEXT("実戦12日: %d戦、プレイヤー操作%d、画面ステップ%d/%d。検証用の勝利入力なし。%s"),battles,playerActions,step+1,RunPlayMaxSteps,*seedText));return;
            }
            if(!TestTrue(TEXT("周回が敗北していない ")+seedText,state.Phase!=RunPhase::Lost))return;
            if(state.Phase==RunPhase::Reward){
                if(state.RewardRecipient<0){if(!Click("reward.recipient.0"))return;}
                else{int pick=0;for(int i=0;i<static_cast<int>(state.Rewards.size());++i)if(state.Rewards[i]==Reward::Attack)pick=i;const auto choice=FString::Printf(TEXT("reward.choice.%d"),pick);if(!Click(TCHAR_TO_UTF8(*choice)) || !Click("reward.confirm"))return;}
                continue;
            }
            if(state.Phase==RunPhase::Route){
                int pick=0;for(int i=0;i<static_cast<int>(state.Choices.size());++i)if(state.Choices[i].Kind==NodeKind::Rest)pick=i;
                const auto node=FString::Printf(TEXT("route.node.%d"),pick);if(!Click(TCHAR_TO_UTF8(*node)))return;continue;
            }
            if(state.Active.Kind==NodeKind::Rest){if(!Click("rest.heal") || !Click("rest.confirm"))return;}
            else if(state.Active.Kind==NodeKind::Shop){if(!Click("shop.exit"))return;}
            else if(state.Active.Kind==NodeKind::Social){const auto id=FString::Printf(TEXT("social.actor.%d"),state.Recruited.front()+1);if(!Click(TCHAR_TO_UTF8(*id)) || !Click("social.choice.0") || !Click("social.confirm"))return;}
            else if(state.Active.Kind==NodeKind::Anomaly){if(!Click("event.resolve"))return;}
            else{
                // 6日目・12日目のボス戦突入を、状態だけでなく画面のラベルでも確認する。
                const auto label=Driver->FindElement(By::Id("event.kind"))->GetText().ToString();
                if(state.Active.Kind==NodeKind::MidBoss){++midBosses;TestTrue(TEXT("6日目のボスを画面で確認"),label.Contains(TEXT("落城姫")));}
                if(state.Active.Kind==NodeKind::FinalBoss){++finalBosses;TestTrue(TEXT("12日目のボスを画面で確認"),label.Contains(TEXT("歴史喰らい")));}
                ++battles;if(!Click("battle.start"))return;
            }
        }
        AddError(FString::Printf(TEXT("12日を有限操作内に完了できませんでした。%s"),*seedText));
    });
    AfterEach([this]{Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}IAutomationDriverModule::Get().Disable();});
}

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
    It(TEXT("全滅の検証入力で敗北し再出発できる"), EAsyncExecution::ThreadPool, [this] {
        if (!TestTrue(TEXT("開始"), ClickAndDrain(Driver->FindElement(By::Id("title.start"))))) return;
        // 出陣の地の文で支度画面が長くなるため、戻る操作が画面外ならスクロールして押す。
        if(!Driver->FindElement(By::Id("setup.back"))->IsVisible()) {
            Driver->FindElement(By::Id("run.scroll"))->ScrollToEndUntil(By::Id("setup.back"));
            DrainSlateLayout();
        }
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
        GirlsSpire::Profile unlocked;unlocked.HighestDifficulty=2;
        auto prior=std::make_shared<GirlsSpire::Run>(42,GirlsSpire::Difficulty::Beginner,GirlsSpire::HeroRole::Sword,unlocked);
        prior->ChooseNode(prior->Snapshot().Revision,0);prior->CompleteEvent(prior->Snapshot().Revision,true);
        Async(EAsyncExecution::TaskGraphMainThread,[this,prior]{Window->SetContent(SNew(SRunScreen).InitialRun(prior));}).Wait();DrainSlateLayout();
        if(!TestTrue(TEXT("解放済みデータでタイトルへ"),ClickAndDrain(Driver->FindElement(By::Id("result.continue")))))return;
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
                const auto Folder = FPaths::ProjectSavedDir() / TEXT("Screenshots/RunPreview") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
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
