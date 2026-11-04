#pragma once
#include "Widgets/SCompoundWidget.h"
#include "Application/Run.h"
#include "Infrastructure/SaveTransaction.h"
#include "Infrastructure/PreferencesStore.h"
#include <memory>

class SVerticalBox;
class SScrollBox;
class SHorizontalBox;
class SBox;
class IGameAudio;
class FBoardSprites;
struct FSlateDynamicImageBrush;
class SRunScreen : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SRunScreen) : _FullField(false),_DevelopmentTools(true) {}
        SLATE_ARGUMENT(bool,FullField)
        SLATE_ARGUMENT(std::shared_ptr<GirlsSpire::Run>, InitialRun)
        SLATE_ARGUMENT(std::shared_ptr<GirlsSpire::SaveTransaction>, SaveService)
        SLATE_ARGUMENT(bool, DevelopmentTools)
        SLATE_ARGUMENT(std::shared_ptr<GirlsSpire::PreferencesStore>, PreferencesService)
        SLATE_ARGUMENT(std::shared_ptr<IGameAudio>, Audio)
        SLATE_EVENT(FSimpleDelegate, OnExitGame)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args);
private:
    TSharedPtr<SBox> FieldHost;
    TSharedPtr<SScrollBox> RunScroll;
    void ResetPageScroll();
    bool FullField=false,FullFieldActive=false;
    void HideFieldCombat();
    TSharedPtr<FSlateDynamicImageBrush> TitleBackdrop;
    FSimpleDelegate ExitGame;
    void AddExitAction();
    std::shared_ptr<IGameAudio> Audio;
    void ShowPreferences(bool resetDraft=true);
    void RefreshPreferences();
    void SavePreferences();
    void ConfirmPreferencesReset();
    int FontSize(int base) const {return GirlsSpire::ScaledFontSize(DisplayPreferences.Text,base);}
    std::shared_ptr<GirlsSpire::PreferencesStore> PreferencesService;
    GirlsSpire::Preferences DisplayPreferences,DraftPreferences;
    GirlsSpire::PreferencesIssue PreferencesLoadIssue=GirlsSpire::PreferencesIssue::Empty;
    FString PreferencesMessage;
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
    void ConfirmGenerationRecovery(std::shared_ptr<const GirlsSpire::GenerationRecovery> confirmed);
    void RepairGenerationRecovery(std::shared_ptr<const GirlsSpire::GenerationRecovery> confirmed);
    void RefreshRun();
    void RefreshRewards();
    void ShowGrowth();
    void RefreshRest();
    void RefreshShop();
    void RefreshSocial();
    void RefreshRescueConversation();
    void ShowEpilogueChoices();
    void ShowEpilogueLibrary();
    void ShowComboLibrary(int recipeId=0);
    void ShowEnemyLibrary(int enemyId=0);
    TSharedPtr<FBoardSprites> EnemyLibrarySprites;
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
