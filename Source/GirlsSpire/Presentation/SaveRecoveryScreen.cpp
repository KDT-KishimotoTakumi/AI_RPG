#include "RunScreen.h"
#include "Widgets/SBoxPanel.h"

void SRunScreen::ShowLoadError(const GirlsSpire::TransactionLoad& loaded)
{
    using namespace GirlsSpire;
    const bool unsupported=loaded.Issue==SlotIssue::UnsupportedVersion || loaded.RunIssue==SlotIssue::UnsupportedVersion || loaded.ProfileIssue==SlotIssue::UnsupportedVersion;
    if(unsupported) {
        AddText(TEXT("この保存形式は読み込めません。対応するゲームのバージョンで開いてください。既存の記録は保持しています。"),"save.version.error");
        AddAction(TEXT("保存データを再読み込み"),"save.reload",[this]{ShowTitle();});return;
    }
    if(SaveService->HasPending()) {
        ShowSaveError([this]{SaveService->Retry();ShowTitle();});return;
    }
    const bool readFailure=loaded.Issue==SlotIssue::ReadFailed || loaded.RunIssue==SlotIssue::ReadFailed || loaded.ProfileIssue==SlotIssue::ReadFailed;
    AddText(readFailure?TEXT("保存先を読み込めませんでした。保存先の利用状態を確認して再試行してください。記録は保持しています。"):
        TEXT("保存データを復元できませんでした。既存の記録は保持しています。"),"save.load.error");
    if(loaded.JournalRepairable)
        AddAction(TEXT("取引記録の修復を確認"),"save.repair.journal",[this]{ConfirmJournalRepair();});
    if(!readFailure && !loaded.JournalRepairable && loaded.Issue==SlotIssue::Corrupt){
        auto preview=SaveService->PreviewGenerationRecovery();
        if(preview.Value){
            const auto candidate=std::make_shared<const GenerationRecovery>(MoveTemp(*preview.Value));
            AddAction(TEXT("同じ世代の周回・恒久データからの復旧を確認"),"save.generation.repair",[this,candidate]{ConfirmGenerationRecovery(candidate);});
        }else if(preview.Issue==SlotIssue::Corrupt)
            AddText(TEXT("正常な共通世代の周回・恒久ペアが見つかりません。原本は保持し、初期化せず停止しています。"),"save.generation.unavailable",16);
    }
    if(!readFailure && loaded.Issue==SlotIssue::None) {
        if(loaded.RunIssue==SlotIssue::Corrupt)
            AddAction(TEXT("破損した周回データの初期化を確認"),"save.reset.run",[this]{ConfirmReset(SaveKind::Run);});
        if(loaded.ProfileIssue==SlotIssue::Corrupt)
            AddAction(TEXT("破損した恒久データの初期化を確認"),"save.reset.profile",[this]{ConfirmReset(SaveKind::Profile);});
    }
    AddAction(TEXT("保存データを再読み込み"),"save.reload",[this]{ShowTitle();});
}
void SRunScreen::ConfirmGenerationRecovery(std::shared_ptr<const GirlsSpire::GenerationRecovery> confirmed)
{
    if(!confirmed)return;
    Body->ClearChildren();
    AddText(FString::Printf(TEXT("共通世代%lluへ戻して復旧しますか？ 最新観測世代は%lluです。共通世代より新しい片側だけの行動・成長・装備・所持品・絆・専用交流・発見・後日談記録は復旧結果に含まれません。異なる世代は混ぜません。"),
        static_cast<unsigned long long>(confirmed->Candidate.Run.Generation),static_cast<unsigned long long>(confirmed->Candidate.HighestGeneration)),"save.generation.warning");
    AddText(TEXT("周回・恒久の本体とバックアップ4原本、および取引記録をCorruptへ保全してから保存します。保全や保存に失敗した場合は進行を停止します。確認後に原本が変わった場合は再確認が必要です。"),"save.generation.preserve",16);
    AddAction(TEXT("損失と保全を確認して共通世代から復旧"),"save.generation.confirm",[this,confirmed]{RepairGenerationRecovery(confirmed);});
    AddAction(TEXT("復旧せず戻る"),"save.generation.cancel",[this]{ShowTitle();});
}
void SRunScreen::RepairGenerationRecovery(std::shared_ptr<const GirlsSpire::GenerationRecovery> confirmed)
{
    if(!confirmed)return;
    using namespace GirlsSpire;
    if(!SaveService->HasPending()){
        const auto prepared=SaveService->PrepareGenerationRecovery(*confirmed);
        if(prepared!=SlotIssue::None){
            if(prepared==SlotIssue::WriteFailed || prepared==SlotIssue::ReadFailed)
                ShowSaveError([this,confirmed]{RepairGenerationRecovery(confirmed);});
            else{
                Body->ClearChildren();
                AddText(TEXT("保存内容が変更されたか、復旧候補を利用できなくなりました。復旧保存は行っていません。再読込して内容を確認してください。"),"save.generation.changed");
                AddAction(TEXT("保存内容を再読込して再確認"),"save.generation.reload",[this]{ShowTitle();});
            }
            return;
        }
    }
    if(SaveService->Retry()!=SlotIssue::None){ShowSaveError([this,confirmed]{RepairGenerationRecovery(confirmed);});return;}
    HavePending=false;HaveStored=false;
    SaveNotice=TEXT("共通世代の周回・恒久ペアから復旧しました。元の4原本と取引記録はCorruptに保全しています。");
    ShowTitle();
}
void SRunScreen::ConfirmJournalRepair()
{
    Body->ClearChildren();
    AddText(TEXT("取引記録を修復しますか？ 正常で同じ確定世代の周回・絆データから再構築し、その内容を維持します。破損した取引記録にしか残っていない未完了の変更は復元できません。"),"save.repair.warning");
    AddText(TEXT("破損した取引記録をCorruptに保全してから修復します。保全や保存に失敗した場合は進行を停止します。"),"save.repair.preserve",16);
    AddAction(TEXT("内容を確認して修復"),"save.repair.confirm",[this]{RepairSavedJournal();});
    AddAction(TEXT("修復せず戻る"),"save.repair.cancel",[this]{ShowTitle();});
}
void SRunScreen::RepairSavedJournal()
{
    using namespace GirlsSpire;
    if(!SaveService->HasPending()) {
        const auto prepared=SaveService->PrepareJournalRepair();
        if(prepared==SlotIssue::Invalid || prepared==SlotIssue::Corrupt || prepared==SlotIssue::UnsupportedVersion) {ShowTitle();return;}
        if(prepared!=SlotIssue::None) {ShowSaveError([this]{RepairSavedJournal();});return;}
    }
    if(SaveService->Retry()!=SlotIssue::None) {ShowSaveError([this]{RepairSavedJournal();});return;}
    HavePending=false;HaveStored=false;
    SaveNotice=TEXT("取引記録を修復しました。破損記録はCorruptに保全し、正常な周回・絆データを維持しました。");
    ShowTitle();
}
void SRunScreen::ConfirmReset(GirlsSpire::SaveKind kind)
{
    Body->ClearChildren();
    const bool run=kind==GirlsSpire::SaveKind::Run;
    AddText(run?TEXT("復元できない周回データを初期化しますか？ 周回内の成長・装備・所持品・戦闘状態を失います。正常な絆データは保持します。"):
        TEXT("復元できない恒久データを初期化しますか？ 絆ランクが0になり、専用交流の完了記録と後日談の本を失います。正常な進行中の周回は保持します。"),"save.reset.warning");
    AddText(TEXT("初期化前に、破損した本体とバックアップを保存フォルダ内のCorruptへ保全します。保全に失敗した場合は上書きしません。"),"save.reset.preserve",16);
    AddAction(TEXT("損失を確認して対象側を初期化"),"save.reset.confirm",[this,kind]{ResetSavedSlot(kind);});
    AddAction(TEXT("初期化せず戻る"),"save.reset.cancel",[this]{ShowTitle();});
}
void SRunScreen::ResetSavedSlot(GirlsSpire::SaveKind kind)
{
    using namespace GirlsSpire;
    if(!SaveService->HasPending()) {
        const auto prepared=SaveService->PrepareReset(kind);
        if(prepared==SlotIssue::UnsupportedVersion || prepared==SlotIssue::Invalid || prepared==SlotIssue::Corrupt) {ShowTitle();return;}
        if(prepared!=SlotIssue::None) {ShowSaveError([this,kind]{ResetSavedSlot(kind);});return;}
        ResetInProgress=kind;
    }
    if(!ResetInProgress || *ResetInProgress!=kind || SaveService->Retry()!=SlotIssue::None) {
        ShowSaveError([this,kind]{ResetSavedSlot(kind);});return;
    }
    ResetInProgress.reset();HavePending=false;HaveStored=false;
    SaveNotice=kind==SaveKind::Run?TEXT("周回データを初期化しました。破損ファイルはCorruptに保全しています。"):
        TEXT("恒久データを初期化しました。破損ファイルはCorruptに保全しています。");
    ShowTitle();
}
