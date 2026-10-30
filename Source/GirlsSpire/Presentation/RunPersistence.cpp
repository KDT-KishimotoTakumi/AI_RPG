#include "RunScreen.h"
#include "CombatScreen.h"
#include "Widgets/SBoxPanel.h"

bool SRunScreen::PersistState(const std::optional<GirlsSpire::RunSaveState>& state,const GirlsSpire::Profile& profile,bool consume)
{
    using namespace GirlsSpire;
    if(!SaveService) return true;
    if(HaveStored && state==StoredRun && profile==StoredProfile && !SaveService->HasPending()) return true;
    if(SaveService->HasPending()) {
        if(!HavePending || state!=PendingRun || profile!=PendingProfile) return false;
    } else {
        if(consume) {
            const auto current=SaveService->Load();
            if(!current.Run) return false;
            auto expected=*current.Run;expected.ResumeAvailable=false;
            if(state!=expected || profile!=current.Permanent) return false;
        }
        const auto prepared=consume?SaveService->PrepareResume():SaveService->Prepare(state,profile);
        if(prepared!=SlotIssue::None) return false;
        PendingRun=state;PendingProfile=profile;HavePending=true;
    }
    if(SaveService->Retry()!=SlotIssue::None) return false;
    StoredRun=state;StoredProfile=profile;HaveStored=true;HavePending=false;return true;
}
bool SRunScreen::SaveCurrent(bool suspend)
{
    if(!CurrentRun) return PersistState({},RetainedProfile);
    GirlsSpire::RunSaveState state{CurrentRun->Checkpoint(),{}};
    if(!suspend && StoredRun && !StoredRun->Combat && StoredRun->Run==state.Run)
        state.ResumeAvailable=StoredRun->ResumeAvailable;
    return PersistState(state,CurrentRun->ProfileState());
}
bool SRunScreen::SaveCombat(const GirlsSpire::SavedCombat& combat,bool suspend,std::uint64_t revision)
{
    if(!CurrentRun || CurrentRun->Snapshot().Revision!=revision) return false;
    return PersistState(GirlsSpire::RunSaveState{CurrentRun->Checkpoint(),combat,suspend},CurrentRun->ProfileState());
}
void SRunScreen::ShowSaveError(TFunction<void()> retry)
{
    Body->ClearChildren();
    AddText(TEXT("保存に失敗しました。進行を停止しています。同じ結果の保存を再試行してください。"),"save.error");
    AddAction(TEXT("同じ内容で保存を再試行"),"save.retry",MoveTemp(retry));
}
bool SRunScreen::LoadTitleState()
{
    using namespace GirlsSpire;
    if(!SaveService) return true;
    const auto loaded=SaveService->Load();
    const auto valid=[](SlotIssue issue){return issue==SlotIssue::None || issue==SlotIssue::Empty;};
    if(loaded.Issue!=SlotIssue::None || !valid(loaded.RunIssue) || !valid(loaded.ProfileIssue)) {
        ShowLoadError(loaded);return false;
    }
    StoredRun=loaded.Run;StoredProfile=loaded.Permanent;RetainedProfile=loaded.Permanent;
    HaveStored=true;StorageRecovered=loaded.Recovered;return true;
}
void SRunScreen::ResumeSaved()
{
    if(!SaveService || !StoredRun) return;
    CommitResume(*StoredRun,StoredProfile,StoredRun->ResumeAvailable);
}
void SRunScreen::CommitResume(GirlsSpire::RunSaveState saved,GirlsSpire::Profile profile,bool consume)
{
    saved.ResumeAvailable=false;
    if(!PersistState(saved,profile,consume)) {
        ShowSaveError([this,saved,profile,consume]{CommitResume(saved,profile,consume);});return;
    }
    const auto restored=GirlsSpire::Run::Restore(saved.Run,profile);
    if(!restored) {Body->ClearChildren();AddText(TEXT("周回を復元できません。記録は保持しています。"),"save.load.error");return;}
    CurrentRun=std::make_shared<GirlsSpire::Run>(*restored);
    if(saved.Combat) {
        const auto battle=GirlsSpire::Battle::Restore(saved.Combat->Battle);
        if(!battle) {Body->ClearChildren();AddText(TEXT("戦闘を復元できません。記録は保持しています。"),"save.load.error");return;}
        ShowCombat(MakeShared<GirlsSpire::Battle>(*battle),TEXT("保存した戦場から再開"),saved.Combat->Control);
    } else RefreshRun();
}
void SRunScreen::SuspendRun()
{
    if(!SaveCurrent(true)) {ShowSaveError([this]{SuspendRun();});return;}
    RetainedProfile=CurrentRun->ProfileState();CurrentRun.reset();ShowTitle();
}
void SRunScreen::FinishRunToTitle()
{
    if(!PersistState({},RetainedProfile)) {ShowSaveError([this]{FinishRunToTitle();});return;}
    ShowTitle();
}
void SRunScreen::ConfirmAbandon()
{
    Body->ClearChildren();
    AddText(TEXT("進行中の周回を放棄しますか？ 周回内の成長は失われます。保存済みの絆・専用交流の完了・後日談は保持されます。"),"save.abandon.warning");
    AddAction(TEXT("周回を放棄して新しく始める"),"save.abandon.confirm",[this]{AbandonSaved();});
    AddAction(TEXT("放棄せず戻る"),"save.abandon.cancel",[this]{ShowTitle();});
}
void SRunScreen::AbandonSaved()
{
    if(!PersistState({},RetainedProfile)) {ShowSaveError([this]{AbandonSaved();});return;}
    ShowSetup();
}
void SRunScreen::ShowCombat(TSharedPtr<GirlsSpire::Battle> combat,const FString& summary,std::optional<GirlsSpire::CombatControlCheckpoint> control)
{
    const auto revision=CurrentRun->Snapshot().Revision;
    TFunction<bool(const GirlsSpire::SavedCombat&)> checkpoint,suspend;
    if(SaveService) {
        checkpoint=[this,revision](const GirlsSpire::SavedCombat& saved){return SaveCombat(saved,false,revision);};
        suspend=[this,revision](const GirlsSpire::SavedCombat& saved) {
            if(!SaveCombat(saved,true,revision)) return false;
            RetainedProfile=CurrentRun->ProfileState();CurrentRun.reset();ShowTitle();return true;
        };
    }
    Body->ClearChildren();Body->AddSlot().AutoHeight()
    [ SNew(SCombatScreen).Combat(combat).GenerationSummary(summary).BondProfile(CurrentRun->ProfileState()).SavedControl(control)
        .OnCheckpoint(MoveTemp(checkpoint)).OnSuspend(MoveTemp(suspend))
        .OnFinished(FSimpleDelegate::CreateLambda([this,combat,revision] {
            if(!CurrentRun || !CurrentRun->CompleteBattle(revision,combat->Snapshot())) return;
            SelectedReward=-1;RefreshRun();
        })) ];
}
