#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Presentation/RunScreen.h"
#include "Infrastructure/DiskSlotFiles.h"
#include "AI/Intent.h"
#include <algorithm>
#include <set>

namespace {
class CampaignMemory : public GirlsSpire::SlotFiles,public GirlsSpire::JournalFiles {
public:
    std::array<std::array<std::optional<std::string>,2>,2> Slots{};
    std::optional<std::string> Journal;
    GirlsSpire::SlotBytes Read(GirlsSpire::SaveKind kind,bool backup)override{return {false,Slots[static_cast<int>(kind)-1][backup]};}
    GirlsSpire::SlotBytes ReadJournal()override{return {false,Journal};}
    bool Replace(GirlsSpire::SaveKind kind,bool backup,std::string_view bytes)override{Slots[static_cast<int>(kind)-1][backup]=std::string(bytes);return true;}
    bool ReplaceJournal(std::string_view bytes)override{Journal=std::string(bytes);return true;}
};
}
BEGIN_DEFINE_SPEC(FGirlsSpireCampaignSpec,"GirlsSpire.UI.Campaign",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
    bool Click(const char* id){
        UE_LOG(LogTemp,Display,TEXT("CampaignUI click begin %hs"),id);
        const auto target=Driver->FindElement(By::Id(id));
        const std::string_view key(id);
        const bool direct=key.starts_with("battle.cell.") || key.starts_with("battle.unit.select.") || key.starts_with("battle.field.");
        if(!direct){
            // 一括スクロールの行き過ぎを避け、欄の内側へホイールで収める。
            const auto scroll=Driver->FindElement(By::Id(Driver->FindElement(By::Id("battle.field.root"))->Exists()?"battle.scroll":"run.scroll"));
            if(!TestTrue(TEXT("操作対象が存在"),target->Exists()))return false;
            for(int wheel=0;wheel<80;++wheel){
                const auto top=scroll->GetAbsolutePosition().Y,bottom=top+scroll->GetSize().Y;
                const auto y=target->GetAbsolutePosition().Y,h=target->GetSize().Y;
                if(y>=top+2 && y+h<=bottom-2 && target->IsVisible())break;
                scroll->ScrollBy(y<top+2?1.f:-1.f);DrainSlateLayout();
            }
        }
        const auto at=target->GetAbsolutePosition(),size=target->GetSize();
        UE_LOG(LogTemp,Display,TEXT("CampaignUI geometry %hs at=%.0f,%.0f size=%.0f,%.0f visible=%d interactable=%d"),id,double(at.X),double(at.Y),double(size.X),double(size.Y),int(target->IsVisible()),int(target->IsInteractable()));
        const bool clicked=TestTrue(UTF8_TO_TCHAR(id),ClickAndDrain(target));
        if(!clicked)Async(EAsyncExecution::TaskGraphMainThread,[this]{
            TArray<FColor> pixels;FIntVector size;
            if(FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),pixels,size)){
                TArray64<uint8> png;FImageUtils::PNGCompressImageArray(size.X,size.Y,pixels,png);
                const auto folder=FPaths::ProjectSavedDir()/TEXT("Screenshots/Campaign")/FGuid::NewGuid().ToString(EGuidFormats::Digits);
                IFileManager::Get().MakeDirectory(*folder,true);FFileHelper::SaveArrayToFile(png,*(folder/TEXT("click-failure.png")));
            }
        }).Wait();
        UE_LOG(LogTemp,Display,TEXT("CampaignUI click end %hs result=%d"),id,int(clicked));return clicked;
    }
END_DEFINE_SPEC(FGirlsSpireCampaignSpec)
void FGirlsSpireCampaignSpec::Define()
{
    // 長時間の通し検証は専用コマンドで明示実行し、通常の画面回帰と分ける。
    if(!FParse::Param(FCommandLine::Get(),TEXT("GirlsSpireCampaignTest")))return;
    BeforeEach([this]{
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        Window=SNew(SWindow).ClientSize(FVector2D(1280,900));
        FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    for(int scenario=0;scenario<3;++scenario)It(scenario==2?TEXT("外見選択データを実ディスクへ保持して12日完走する"):scenario==1?TEXT("実ディスク保存と戦闘内外の再構築を通して12日完走する"):TEXT("通常能力の12日を実戦とマウス操作で完走する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(900),[this,scenario]{
        using namespace GirlsSpire;
        const bool disk=scenario!=0;
        // 初期シードだけ固定し、その後は状態注入・検証用勝利入力を使わない。
        Profile initialProfile;
        if(scenario==2){
            // 過去周回で条件達成済みの恒久記録だけを準備する。画像の表示は未接続。
            initialProfile.VictoryRecorded=true;initialProfile.HighestDifficulty=1;initialProfile.SpecialConversations.fill(true);
            for(int id=0;id<=8;++id)if(!TestTrue(TEXT("初期解放済み外見をルールAPIで選ぶ"),ChooseAppearance(initialProfile,id,AppearanceVariant::Alternate)))return;
        }
        const auto expectedAppearances=initialProfile.SelectedAppearances;
        const auto run=std::make_shared<Run>(1,Difficulty::Beginner,HeroRole::CommandFan,initialProfile);
        const auto files=std::make_shared<CampaignMemory>();
        std::vector<std::shared_ptr<DiskSlotFiles>> backends;
        const auto folder=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Tests/Campaign")/FGuid::NewGuid().ToString(EGuidFormats::Digits));
        std::shared_ptr<SaveTransaction> service;
        const auto openDisk=[&]{
            // 各ストアの参照先を、画面破棄まで保持する。個人セーブは使用しない。
            backends.push_back(std::make_shared<DiskSlotFiles>(std::filesystem::path(*folder)));
            service=std::make_shared<SaveTransaction>(*backends.back(),*backends.back());
        };
        if(disk){
            if(!TestTrue(TEXT("隔離した実ディスク保存先を作成"),IFileManager::Get().MakeDirectory(*folder,true)))return;
            openDisk();AddInfo(FString(TEXT("通し検証保存先: "))+folder);
        }else service=std::make_shared<SaveTransaction>(*files,*files);
        Async(EAsyncExecution::TaskGraphMainThread,[this,run,service]{Window->SetContent(SNew(SRunScreen).InitialRun(run).SaveService(service).DevelopmentTools(false).FullField(true));}).Wait();DrainSlateLayout();
        std::set<int> days;int battles=0,midBosses=0,finalBosses=0,playerActions=0;
        bool outsideResumed=false,insideResumed=false;
        for(int step=0;step<1024;++step){
            const auto saved=service->Load();
            if(!TestTrue(TEXT("各操作後に確定済み取引を読み取れる"),saved.Issue==SlotIssue::None && saved.Run.has_value()))return;
            if(!TestTrue(TEXT("全操作で人物別の外見選択データを保持"),saved.Permanent.SelectedAppearances==expectedAppearances))return;
            const auto& state=saved.Run->Run.State;days.insert(state.Day);
            UE_LOG(LogTemp,Display,TEXT("CampaignUI step=%d day=%d phase=%d battle=%d"),step,state.Day,int(state.Phase),int(saved.Run->Combat.has_value()));
            const bool outside=disk && !outsideResumed && state.Day==5 && state.Phase==RunPhase::Route;
            const bool inside=disk && !insideResumed && state.Day==6 && saved.Run->Combat && saved.Run->Combat->Battle.State.Outcome==BattleOutcome::Ongoing;
            if(outside || inside){
                if(!Click("save.suspend"))return;
                if(!TestTrue(TEXT("中断記録を実ディスクへ保存"),service->Load().Run && service->Load().Run->ResumeAvailable))return;
                openDisk();
                Async(EAsyncExecution::TaskGraphMainThread,[this,service]{Window->SetContent(SNew(SRunScreen).SaveService(service).DevelopmentTools(false).FullField(true));}).Wait();DrainSlateLayout();
                if(!Click("title.resume"))return;
                const auto resumed=service->Load();
                auto expectedResume=*saved.Run;expectedResume.ResumeAvailable=false;
                if(!TestTrue(TEXT("新ストアと新画面の再開で全周回/戦闘/抽選/恒久が一致し中断権を消費"),resumed.Issue==SlotIssue::None && resumed.Run==expectedResume && resumed.Permanent==saved.Permanent && resumed.Generation>saved.Generation))return;
                if(outside)outsideResumed=true;else insideResumed=true;
                continue;
            }
            if(saved.Run->Combat){
                if(!TestTrue(TEXT("戦闘中は開始時の人物別衣装を保持"),saved.Run->Combat->Appearances==expectedAppearances))return;
                const auto restored=Battle::Restore(saved.Run->Combat->Battle);
                if(!TestTrue(TEXT("通常戦闘の保存を復元"),restored.has_value()))return;
                auto expected=*restored;
                const auto restoredControl=CombatControl::Restore(saved.Run->Combat->Control,expected);
                if(!TestTrue(TEXT("固定信頼と抽選状態を復元"),restoredControl.has_value()))return;
                auto control=*restoredControl;
                const auto& current=expected.Snapshot();
                if(current.Outcome!=BattleOutcome::Ongoing){
                    if(!TestTrue(TEXT("全戦闘を実際に勝利"),current.Outcome==BattleOutcome::Won) || !Click("battle.return"))return;
                    continue;
                }
                const auto actor=std::find_if(current.Units.begin(),current.Units.end(),[&](const auto& unit){return unit.Id==current.ActiveId;});
                if(!TestTrue(TEXT("画面はプレイヤー側の手番で止まる"),actor!=current.Units.end() && actor->Side==Faction::Player))return;
                if(actor->Id==0){
                    const auto ranked=RankIntentActions(expected,{});
                    if(!TestTrue(TEXT("主人公の合法操作候補"),!ranked.empty()))return;
                    const auto action=ranked.front();
                    const auto occupied=std::find_if(current.Units.begin(),current.Units.end(),[&](const auto& unit){return unit.Hp>0 && unit.At==action.Target;});
                    const auto cell=occupied==current.Units.end()?FString::Printf(TEXT("battle.cell.%d.%d"),action.Target.X,action.Target.Y):FString::Printf(TEXT("battle.unit.select.%d"),occupied->Id);
                    if(action.Kind==BasicActionKind::Wait){if(!Click("battle.wait.south"))return;}
                    else {
                        if(action.Kind==BasicActionKind::Move){if(!Click("battle.field.move"))return;}
                        else if(action.Kind==BasicActionKind::Attack){if(!Click("battle.field.attack"))return;}
                        else {const auto skill=FString::Printf(TEXT("battle.skill.%d"),static_cast<int>(action.Ability));if(!Click(TCHAR_TO_UTF8(*skill)))return;}
                        if(!Click(TCHAR_TO_UTF8(*cell)))return;
                        const char* confirm=action.Kind==BasicActionKind::Move?"battle.move.confirm":action.Kind==BasicActionKind::Attack?"battle.confirm":"battle.skill.confirm";
                        if(!Click(confirm))return;
                    }
                    if(!TestTrue(TEXT("主人公の確定操作を共通ルールで照合"),ApplyBasicAction(expected,action)))return;
                }else{
                    const auto forecast=IntentForecast::Build(expected,RankIntentActions(expected,control.Orders[actor->Id]),control.Trust[actor->Id]);
                    if(!Click("intent.execute") || !TestTrue(TEXT("仲間の信頼別抽選を共通ルールで照合"),forecast.Execute(expected,control.ChoiceRandom)))return;
                }
                ++playerActions;
                for(int enemyStep=0;expected.Snapshot().Outcome==BattleOutcome::Ongoing && enemyStep<128;++enemyStep){
                    const auto& next=expected.Snapshot();
                    const auto enemy=std::find_if(next.Units.begin(),next.Units.end(),[&](const auto& unit){return unit.Id==next.ActiveId;});
                    if(enemy->Side==Faction::Player)break;
                    if(!TestTrue(TEXT("通常の敵自動行動"),ApplyBasicAction(expected,ChooseBasicAction(expected))))return;
                }
                const auto after=service->Load();
                if(!TestTrue(TEXT("UI確定後の戦闘全状態/乱数と仲間抽選が一致"),after.Run && after.Run->Combat && after.Run->Combat->Battle==expected.Checkpoint() && after.Run->Combat->Control==control.Checkpoint()))return;
                continue;
            }
            if(state.PendingRescue>=0){if(!Click("rescue.choice.0") || !Click("rescue.confirm"))return;continue;}
            if(state.Phase==RunPhase::Won){
                if(!TestTrue(TEXT("12日/加入4人/両ボス/中級解放"),days.size()==12 && state.Day==12 && state.Recruited.size()==4 && midBosses==1 && finalBosses==1 && saved.Permanent.HighestDifficulty==1 && saved.Permanent.VictoryRecorded))return;
                if(!Click("result.continue") || !Click("epilogue.select.0") || !Click("epilogue.confirm") || !Click("epilogue.back"))return;
                TestTrue(TEXT("後日談記録からタイトルへ戻る"),Driver->FindElement(By::Id("title.start"))->Exists() && service->Load().Permanent.Epilogues[0]);
                if(disk){
                    openDisk();const auto finished=service->Load();
                    TestTrue(TEXT("内外の再構築を完了し、新ストアでも後日談/難易度/図鑑/外見選択を保持し周回は終了"),outsideResumed && insideResumed && finished.Issue==SlotIssue::None && !finished.Run && finished.Permanent.Epilogues[0] && finished.Permanent.HighestDifficulty==1 && finished.Permanent.VictoryRecorded && finished.Permanent.SelectedAppearances==expectedAppearances && finished.Permanent.DiscoveredEnemies==saved.Permanent.DiscoveredEnemies && finished.Permanent.DiscoveredCombos==saved.Permanent.DiscoveredCombos);
                }
                AddInfo(FString::Printf(TEXT("初級/軍配/seed1: %d戦、プレイヤー操作%d、全12日。初期値以外の状態注入なし。"),battles,playerActions));return;
            }
            if(!TestTrue(TEXT("周回が敗北していない"),state.Phase!=RunPhase::Lost))return;
            if(state.Phase==RunPhase::Reward){
                if(state.RewardRecipient<0){if(!Click("reward.recipient.0"))return;}
                else {int pick=0;for(int i=0;i<static_cast<int>(state.Rewards.size());++i)if(state.Rewards[i]==Reward::Attack)pick=i;const auto choice=FString::Printf(TEXT("reward.choice.%d"),pick);if(!Click(TCHAR_TO_UTF8(*choice)) || !Click("reward.confirm"))return;}
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
            else {if(state.Active.Kind==NodeKind::MidBoss)++midBosses;if(state.Active.Kind==NodeKind::FinalBoss)++finalBosses;++battles;if(!Click("battle.start"))return;}
        }
        AddError(TEXT("12日を有限操作内に完了できませんでした。"));
    });
    AfterEach([this]{Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}IAutomationDriverModule::Get().Disable();});
}
