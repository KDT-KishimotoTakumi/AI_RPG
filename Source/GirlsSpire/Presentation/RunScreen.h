#pragma once
#include "Widgets/SCompoundWidget.h"
#include "Application/Run.h"
#include "Infrastructure/SaveTransaction.h"
#include <memory>

class SVerticalBox;
class SHorizontalBox;
class SRunScreen : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SRunScreen) : _DevelopmentTools(true) {}
        SLATE_ARGUMENT(std::shared_ptr<GirlsSpire::Run>, InitialRun)
        SLATE_ARGUMENT(std::shared_ptr<GirlsSpire::SaveTransaction>, SaveService)
        SLATE_ARGUMENT(bool, DevelopmentTools)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args);
private:
    void ShowTitle();
    void ShowSetup();
    void StartRun();
    void StartBattle(bool forceGenerationFailure=false);
    void ShowCombat(TSharedPtr<GirlsSpire::Battle> combat,const FString& summary,std::optional<GirlsSpire::CombatControlCheckpoint> control={});
    bool PersistState(const std::optional<GirlsSpire::RunSaveState>& state,const GirlsSpire::Profile& profile,bool consume=false);
    bool SaveCurrent(bool suspend=false);
    bool SaveCombat(const GirlsSpire::SavedCombat& combat,bool suspend,std::uint64_t revision);
    void ShowSaveError(TFunction<void()> retry);
    bool LoadTitleState();
    void ResumeSaved();
    void CommitResume(GirlsSpire::RunSaveState saved,GirlsSpire::Profile profile,bool consume);
    void SuspendRun();
    void FinishRunToTitle();
    void ConfirmAbandon();
    void AbandonSaved();
    void ShowLoadError(const GirlsSpire::TransactionLoad& loaded);
    void ConfirmReset(GirlsSpire::SaveKind kind);
    void ResetSavedSlot(GirlsSpire::SaveKind kind);
    void ConfirmJournalRepair();
    void RepairSavedJournal();
    void RefreshRun();
    void RefreshRewards();
    void ShowGrowth();
    void RefreshRest();
    void RefreshShop();
    void RefreshSocial();
    void RefreshRescueConversation();
    void ShowEpilogueChoices();
    void ShowEpilogueLibrary();
    void ShowEpiloguePage(int actorId,bool archived);
    void ConfirmEpilogue(int actorId);
    FString ProfileLabel() const;
    static FString ActorName(int id);
    void AddText(const FString& Text, const FName& Id, int Size = 20);
    void AddAction(const FString& Text, const FName& Id, TFunction<void()> Action, TSharedPtr<SHorizontalBox> Row = nullptr);
    TSharedPtr<SVerticalBox> Body;
    std::shared_ptr<GirlsSpire::Run> CurrentRun;
    std::shared_ptr<GirlsSpire::SaveTransaction> SaveService;
    std::optional<GirlsSpire::RunSaveState> StoredRun,PendingRun;
    GirlsSpire::Profile StoredProfile,PendingProfile;
    bool HaveStored=false,HavePending=false,StorageRecovered=false;
    std::optional<GirlsSpire::SaveKind> ResetInProgress;
    FString SaveNotice;
    bool DevelopmentTools=true;
    GirlsSpire::Profile RetainedProfile;
    GirlsSpire::Difficulty Difficulty = GirlsSpire::Difficulty::Beginner;
    GirlsSpire::HeroRole Role = GirlsSpire::HeroRole::Sword;
    int SelectedReward=-1;
    int SelectedRest=-2;
    int SelectedOffer=-1;
    int ShopActor=-1;
    FString ShopMessage;
    int SocialActor=-1;
    int SocialChoice=-1;
    FString SocialMessage;
    std::optional<int> PendingEpilogue;
};
