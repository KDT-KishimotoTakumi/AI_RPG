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
#include "CampaignHandoff.h"
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
    const bool accessibility=FParse::Param(FCommandLine::Get(),TEXT("GirlsSpireCampaignAccessibility"));
    int width=1920;FParse::Value(FCommandLine::Get(),TEXT("GirlsSpireCampaignWidth="),width);
    BeforeEach([this,accessibility]{
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        if(accessibility)Window=SNew(SWindow).ClientSize(FVector2D(1280,900)).AutoCenter(EAutoCenter::None).ScreenPosition(FVector2D::ZeroVector).SaneWindowPlacement(false).AdjustInitialSizeAndPositionForDPIScale(false);
        else Window=SNew(SWindow).ClientSize(FVector2D(1280,900));
        FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    const bool matrix=FParse::Param(FCommandLine::Get(),TEXT("GirlsSpireCampaignMatrix"));
    FString stage,processRoot;const bool process=FParse::Value(FCommandLine::Get(),TEXT("GirlsSpireCampaignStage="),stage);
    FParse::Value(FCommandLine::Get(),TEXT("GirlsSpireCampaignRoot="),processRoot);
    processRoot=FPaths::ConvertRelativePathToFull(processRoot);
    int processScenario=2;FParse::Value(FCommandLine::Get(),TEXT("GirlsSpireCampaignScenario="),processScenario);
    const int firstScenario=process?FMath::Clamp(processScenario,2,14):accessibility?2:0;
    const int lastScenario=process?firstScenario+1:accessibility?3:matrix?15:3;
    for(int scenario=firstScenario;scenario<lastScenario;++scenario)It(accessibility?FString::Printf(TEXT("併用設定%dの12日通し"),width):process?FString::Printf(TEXT("別プロセス12日通し%d-%s"),processScenario,*stage):scenario>=3?FString::Printf(TEXT("難易度%d・役割%dの実ディスク12日通し"),(scenario-3)/4,(scenario-3)%4):FString(scenario==2?TEXT("外見選択データを実ディスクへ保持して12日完走する"):scenario==1?TEXT("実ディスク保存と戦闘内外の再構築を通して12日完走する"):TEXT("通常能力の12日を実戦とマウス操作で完走する")),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(900),[this,scenario,process,stage,processRoot,accessibility,width,processScenario]{
        using namespace GirlsSpire;
        if(accessibility && !TestTrue(TEXT("併用設定の検証寸法"),(width==1920 || width==2560) && !process))return;
        if(process){
            if(!TestTrue(TEXT("有効な通しシナリオ2～14"),processScenario>=2 && processScenario<=14))return;
            const auto prefix=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Tests/ProcessCampaign"));
            if(!TestTrue(TEXT("専用のプロセス通し保存先"),processRoot!=prefix && FPaths::IsUnderDirectory(processRoot,prefix)) ||
               !TestTrue(TEXT("有効な通し段階"),stage==TEXT("Start") || stage==TEXT("Continue") || stage==TEXT("Finish")))return;
        }
        const bool disk=scenario!=0;
        const auto difficulty=static_cast<Difficulty>(scenario>=3?(scenario-3)/4:0);
        const auto role=scenario>=3?static_cast<HeroRole>((scenario-3)%4):HeroRole::CommandFan;
        const int expectedDifficulty=std::min(2,static_cast<int>(difficulty)+1);
        // 初期周回だけ準備し、その後は状態注入・検証用勝利入力を使わない。
        Profile initialProfile;
        if(scenario==2){
            // 過去周回で条件達成済みの恒久記録だけを準備する。
            initialProfile.VictoryRecorded=true;initialProfile.HighestDifficulty=1;initialProfile.SpecialConversations.fill(true);
            for(int id=0;id<=8;++id)if(!TestTrue(TEXT("初期解放済み外見をルールAPIで選ぶ"),ChooseAppearance(initialProfile,id,AppearanceVariant::Alternate)))return;
        }
        if(scenario>=3)initialProfile.HighestDifficulty=static_cast<int>(difficulty);
        const auto expectedAppearances=initialProfile.SelectedAppearances;
        const auto run=process && stage!=TEXT("Start")?std::shared_ptr<Run>{}:std::make_shared<Run>(1,difficulty,role,initialProfile);
        const auto files=std::make_shared<CampaignMemory>();
        std::vector<std::shared_ptr<DiskSlotFiles>> backends;
        const auto folder=process?processRoot/TEXT("slots"):FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Tests/Campaign")/FGuid::NewGuid().ToString(EGuidFormats::Digits));
        std::shared_ptr<SaveTransaction> service;
        std::shared_ptr<PreferencesStore> preferences;
        const Preferences combined{TextSize::Largest,0,0,true,true,true};
        const auto openDisk=[&]{
            // 各ストアの参照先を、画面破棄まで保持する。個人セーブは使用しない。
            backends.push_back(std::make_shared<DiskSlotFiles>(std::filesystem::path(*folder)));
            service=std::make_shared<SaveTransaction>(*backends.back(),*backends.back());
            if(accessibility)preferences=std::make_shared<PreferencesStore>(*backends.back());
        };
        if(disk){
            if(process && stage==TEXT("Start") && !TestFalse(TEXT("既存の通し保存を上書きしない"),IFileManager::Get().DirectoryExists(*folder)))return;
            if(!TestTrue(TEXT("隔離した実ディスク保存先を作成"),IFileManager::Get().MakeDirectory(*folder,true)))return;
            openDisk();AddInfo(FString(TEXT("通し検証保存先: "))+folder);
        }else service=std::make_shared<SaveTransaction>(*files,*files);
        if(accessibility){
            bool dimensions=false;Async(EAsyncExecution::TaskGraphMainThread,[this,service,preferences,width,&dimensions]{Window->Resize(FVector2D(width,width==1920?1080:1440));Window->SetContent(SNew(SRunScreen).SaveService(service).PreferencesService(preferences).DevelopmentTools(false));const auto size=Window->GetClientSizeInScreen();dimensions=FMath::IsNearlyEqual(double(size.X),double(width),.5) && FMath::IsNearlyEqual(double(size.Y),double(width==1920?1080:1440),.5);}).Wait();DrainSlateLayout();
            if(!TestTrue(TEXT("併用設定の実クライアント寸法"),dimensions) || !Click("title.settings") || !Click("settings.text.2") || !Click("settings.bgm.mute") || !Click("settings.sfx.mute") || !Click("settings.subtitles") || !Click("settings.subtitles") || !Click("settings.effects") || !Click("settings.intimacy") || !Click("settings.intimacy") || !Click("settings.save"))return;
            if(!TestTrue(TEXT("最大文字/字幕/演出軽減/抑制/消音を実保存"),preferences->Load().Issue==PreferencesIssue::None && preferences->Load().Value==combined && !service->Load().Run))return;
        }
        Async(EAsyncExecution::TaskGraphMainThread,[this,run,service,preferences]{Window->SetContent(SNew(SRunScreen).InitialRun(run).SaveService(service).PreferencesService(preferences).DevelopmentTools(false).FullField(true));}).Wait();DrainSlateLayout();
        std::set<int> days;int battles=0,midBosses=0,finalBosses=0,playerActions=0;
        bool outsideResumed=false,insideResumed=false;
        bool captured=false;
        if(process && stage!=TEXT("Start")){
            const auto transfer=ReadCampaignTransfer(processRoot);const auto saved=service->Load();
            if(!TestTrue(TEXT("前プロセスの全保存/恒久/確定世代と独立期待記録が一致"),transfer && saved.Issue==SlotIssue::None && saved.Run==transfer->State && saved.Permanent==transfer->Permanent && saved.Generation==transfer->Progress.Generation))return;
            if(!TestTrue(TEXT("引継ぎの難易度・役割・外見が指定シナリオと一致"),transfer->State.Run.State.Level==difficulty && transfer->State.Run.State.Role==role && transfer->Permanent.SelectedAppearances==expectedAppearances))return;
            const bool inside=stage==TEXT("Finish");
            if(!TestTrue(TEXT("前プロセスは所定の境界で中断済み"),transfer->State.ResumeAvailable && transfer->State.Run.State.Day==(inside?6:5) && transfer->State.Combat.has_value()==inside && transfer->Progress.Days==(inside?63:31)))return;
            for(int day=1;day<=12;++day)if(transfer->Progress.Days&(1<<(day-1)))days.insert(day);
            battles=transfer->Progress.Battles;midBosses=transfer->Progress.MidBosses;finalBosses=transfer->Progress.FinalBosses;playerActions=transfer->Progress.PlayerActions;
            outsideResumed=true;insideResumed=inside;
            if(!Click("title.resume"))return;
            auto expected=transfer->State;expected.ResumeAvailable=false;const auto resumed=service->Load();
            if(!TestTrue(TEXT("別UEプロセスのUI再開で中断権以外の全状態/RNG/恒久を維持"),resumed.Run==expected && resumed.Permanent==transfer->Permanent && resumed.Generation>saved.Generation))return;
        }
        for(int step=0;step<1024;++step){
            const auto saved=service->Load();
            if(!TestTrue(TEXT("各操作後に確定済み取引を読み取れる"),saved.Issue==SlotIssue::None && saved.Run.has_value()))return;
            if(accessibility && !TestTrue(TEXT("通し全操作で併用設定を維持"),preferences->Load().Issue==PreferencesIssue::None && preferences->Load().Value==combined))return;
            if(!TestTrue(TEXT("全操作で人物別の外見選択データを保持"),saved.Permanent.SelectedAppearances==expectedAppearances))return;
            const auto& state=saved.Run->Run.State;days.insert(state.Day);
            UE_LOG(LogTemp,Display,TEXT("CampaignUI step=%d day=%d phase=%d battle=%d"),step,state.Day,int(state.Phase),int(saved.Run->Combat.has_value()));
            const bool outside=disk && !outsideResumed && state.Day==5 && state.Phase==RunPhase::Route;
            const bool inside=disk && !insideResumed && state.Day==6 && saved.Run->Combat && saved.Run->Combat->Battle.State.Outcome==BattleOutcome::Ongoing;
            if(outside || inside){
                if(!Click("save.suspend"))return;
                if(process){
                    if(!TestTrue(TEXT("段階に対応した中断境界"),(outside && stage==TEXT("Start")) || (inside && stage==TEXT("Continue"))))return;
                    const auto suspended=service->Load();auto expected=*saved.Run;expected.ResumeAvailable=true;
                    if(!TestTrue(TEXT("中断時に全状態を保ち再開可能にする"),suspended.Issue==SlotIssue::None && suspended.Run==expected && suspended.Permanent==saved.Permanent && suspended.Generation>=saved.Generation && (saved.Run->ResumeAvailable || suspended.Generation>saved.Generation)))return;
                    int mask=0;for(int day:days)mask|=1<<(day-1);
                    if(!TestTrue(TEXT("次プロセス用の独立した全保存と進捗を書出"),WriteCampaignTransfer(processRoot,{*suspended.Run,suspended.Permanent,{mask,battles,midBosses,finalBosses,playerActions,suspended.Generation}})))return;
                    TestTrue(TEXT("段階完了記録"),FFileHelper::SaveStringToFile(TEXT("pass"),*(processRoot/(stage+TEXT(".done")))));return;
                }
                if(!TestTrue(TEXT("中断記録を実ディスクへ保存"),service->Load().Run && service->Load().Run->ResumeAvailable))return;
                openDisk();
                Async(EAsyncExecution::TaskGraphMainThread,[this,service,preferences]{Window->SetContent(SNew(SRunScreen).SaveService(service).PreferencesService(preferences).DevelopmentTools(false).FullField(true));}).Wait();DrainSlateLayout();
                if(!Click("title.resume"))return;
                const auto resumed=service->Load();
                auto expectedResume=*saved.Run;expectedResume.ResumeAvailable=false;
                if(!TestTrue(TEXT("新ストアと新画面の再開で全周回/戦闘/抽選/恒久が一致し中断権を消費"),resumed.Issue==SlotIssue::None && resumed.Run==expectedResume && resumed.Permanent==saved.Permanent && resumed.Generation>saved.Generation))return;
                if(outside)outsideResumed=true;else insideResumed=true;
                continue;
            }
            if(saved.Run->Combat){
                if(accessibility && saved.Run->Combat->Battle.State.Outcome==BattleOutcome::Ongoing){
                    if(!Driver->FindElement(By::Id("battle.field.menu"))->IsVisible() && !Click("battle.field.open"))return;
                    if(Driver->FindElement(By::Id("battle.cues.monochrome"))->GetText().ToString().Contains(TEXT("色付き")) && !Click("battle.cues.monochrome"))return;
                    if(Driver->FindElement(By::Id("battle.cues.threats"))->GetText().ToString().Contains(TEXT("非表示")) && !Click("battle.cues.threats"))return;
                    if(!Driver->FindElement(By::Id("battle.cues.scope"))->Exists() && !Click("battle.field.details"))return;
                    const auto selection=Driver->FindElement(By::Id("battle.field.selection"))->GetText().ToString();const bool scope=Driver->FindElement(By::Id("battle.cues.scope"))->Exists();
                    if(!selection.Contains(TEXT("HP")) || !scope)AddInfo(FString::Printf(TEXT("併用表示: selection=%s scope=%d details=%s threats=%s active=%d"),*selection,int(scope),*Driver->FindElement(By::Id("battle.field.details"))->GetText().ToString(),*Driver->FindElement(By::Id("battle.cues.threats"))->GetText().ToString(),saved.Run->Combat->Battle.State.ActiveId));
                    if(!TestTrue(TEXT("単色でもHPを取得"),selection.Contains(TEXT("HP"))) || !TestTrue(TEXT("危険域の文字説明を取得"),scope) || !TestTrue(TEXT("表示操作は全保存と乱数/世代を変更しない"),service->Load().Run==saved.Run && service->Load().Permanent==saved.Permanent && service->Load().Generation==saved.Generation))return;
                    for(const auto& unit:saved.Run->Combat->Battle.State.Units)if(unit.Hp>0){
                        const auto key=FString::Printf(TEXT("battle.cues.%d.%d"),unit.At.X,unit.At.Y);const auto label=Driver->FindElement(By::Id(TCHAR_TO_UTF8(*key)))->GetText().ToString();
                        if(!TestTrue(TEXT("色以外の陣営記号を維持"),label.Contains(unit.Side==Faction::Player?TEXT("○"):unit.Side==Faction::Enemy?TEXT("△"):TEXT("◇"))))return;
                    }
                    if(!captured){Async(EAsyncExecution::TaskGraphMainThread,[this,width]{TArray<FColor> pixels;FIntVector size;if(!TestTrue(TEXT("併用設定の戦場を撮影"),FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),pixels,size)))return;TestTrue(TEXT("撮影寸法でも実寸を確認"),size.X>=width && size.Y>=(width==1920?1080:1440));TArray64<uint8> png;FImageUtils::PNGCompressImageArray(size.X,size.Y,pixels,png);const auto dir=FPaths::ProjectSavedDir()/TEXT("Screenshots/CampaignAccessibility")/FGuid::NewGuid().ToString(EGuidFormats::Digits);IFileManager::Get().MakeDirectory(*dir,true);TestTrue(TEXT("併用設定の撮影を保全"),FFileHelper::SaveArrayToFile(png,*(dir/TEXT("battle.png"))));}).Wait();captured=true;}
                }
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
                if(!TestTrue(TEXT("12日/加入4人/両ボス/難易度に応じた解放"),days.size()==12 && state.Day==12 && state.Recruited.size()==4 && midBosses==1 && finalBosses==1 && saved.Permanent.HighestDifficulty==expectedDifficulty && saved.Permanent.VictoryRecorded))return;
                if(!Click("result.continue") || !Click("epilogue.select.0") || !Click("epilogue.confirm") || !Click("epilogue.back"))return;
                TestTrue(TEXT("後日談記録からタイトルへ戻る"),Driver->FindElement(By::Id("title.start"))->Exists() && service->Load().Permanent.Epilogues[0]);
                if(disk){
                    openDisk();const auto finished=service->Load();
                    TestTrue(TEXT("内外の再構築を完了し、新ストアでも後日談/難易度/図鑑/外見選択を保持し周回は終了"),outsideResumed && insideResumed && finished.Issue==SlotIssue::None && !finished.Run && finished.Permanent.Epilogues[0] && finished.Permanent.HighestDifficulty==expectedDifficulty && finished.Permanent.VictoryRecorded && finished.Permanent.SelectedAppearances==expectedAppearances && finished.Permanent.DiscoveredEnemies==saved.Permanent.DiscoveredEnemies && finished.Permanent.DiscoveredCombos==saved.Permanent.DiscoveredCombos);
                }
                if(process){
                    if(!TestTrue(TEXT("最終プロセスでのみ完走する"),stage==TEXT("Finish")))return;
                    TestTrue(TEXT("別プロセス12日完走の記録"),FFileHelper::SaveStringToFile(TEXT("pass"),*(processRoot/TEXT("Finish.done"))));
                }
                AddInfo(FString::Printf(TEXT("難易度%d/役割%d/seed1: %d戦、プレイヤー操作%d、全12日。初期値以外の状態注入なし。"),int(difficulty),int(role),battles,playerActions));return;
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
