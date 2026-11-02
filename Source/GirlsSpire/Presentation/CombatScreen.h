#pragma once
#include "Widgets/SCompoundWidget.h"
#include "Domain/Battle.h"
#include "Domain/Bond.h"
#include "Domain/Preferences.h"
#include "AI/Intent.h"
#include "Application/CombatControl.h"
#include "Infrastructure/SavePayload.h"
#include <array>
#include <optional>

class SVerticalBox;
class SHorizontalBox;
class SUniformGridPanel;
class STextBlock;
class FCharacterPortraits;
class SCombatScreen : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCombatScreen) {}
        SLATE_ARGUMENT(TSharedPtr<GirlsSpire::Battle>, Combat)
        SLATE_ARGUMENT(FString, GenerationSummary)
        SLATE_ARGUMENT(std::optional<GirlsSpire::Profile>, BondProfile)
        SLATE_ARGUMENT(std::optional<GirlsSpire::CombatControlCheckpoint>, SavedControl)
        SLATE_ARGUMENT(TSharedPtr<FCharacterPortraits>, Portraits)
        SLATE_ARGUMENT(GirlsSpire::Preferences, DisplayPreferences)
        SLATE_EVENT(FSimpleDelegate, OnFinished)
        SLATE_ARGUMENT(TFunction<bool(const GirlsSpire::SavedCombat&)>, OnCheckpoint)
        SLATE_ARGUMENT(TFunction<bool(const GirlsSpire::SavedCombat&)>, OnSuspend)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    GirlsSpire::CombatControlCheckpoint ControlCheckpoint() const {return Control.Checkpoint();}
private:
    int FontSize(int base) const {return GirlsSpire::ScaledFontSize(DisplayPreferences.Text,base);}
    GirlsSpire::Preferences DisplayPreferences;
    void Refresh();
    void RefreshPortraits();
    void RefreshSkills();
    void RefreshMedicine();
    static const TCHAR* SkillLabel(GirlsSpire::Skill skill);
    FString OwnedSkillLabel(int actorId,GirlsSpire::Skill skill) const;
    void RefreshCombos();
    void RefreshUltimate();
    void RefreshTeamUltimate();
    void SelectCell(GirlsSpire::Position cell, std::uint64_t revision);
    void ConfirmAttack(std::uint64_t revision);
    void ConfirmMove(std::uint64_t revision);
    void Wait(GirlsSpire::Facing facing, std::uint64_t revision);
    void AdvanceEnemies();
    bool PersistCheckpoint();
    void ShowSaveFailure();
    void DescribeHits();
    void RefreshIntent();
    void SelectIntentCell(GirlsSpire::Position cell);
    void ExecuteIntent(const GirlsSpire::IntentForecast& forecast);
    void Text(const FString& label, const FName& id, int size=18);
    void Action(const FString& label, const FName& id, TFunction<void()> callback, bool enabled=true, TSharedPtr<SHorizontalBox> row=nullptr);
    TSharedPtr<GirlsSpire::Battle> Combat;
    TSharedPtr<SUniformGridPanel> Grid;
    TSharedPtr<SVerticalBox> Panel;
    TSharedPtr<SHorizontalBox> PortraitPanel;
    TSharedPtr<FCharacterPortraits> PortraitResources;
    TSharedPtr<STextBlock> ComboGauge;
    TSharedPtr<STextBlock> ComboResult;
    FSimpleDelegate OnFinished;
    TFunction<bool(const GirlsSpire::SavedCombat&)> OnCheckpoint,OnSuspend;
    bool SavingBlocked=false,SuspensionPending=false;
    std::optional<GirlsSpire::Position> Selected;
    std::optional<GirlsSpire::Skill> SelectedSkill;
    std::optional<GirlsSpire::Position> MoveTarget;
    bool SelectingUltimate=false;
    bool SelectingMedicine=false;
    bool MedicineVisible=false;
    bool SelectingTeamUltimate=false;
    FString Message;
    FString GenerationSummary;
    GirlsSpire::CombatControl Control;
    std::vector<int> KnownCombos;
    std::uint64_t SeenComboSequence=0;
    bool NewComboDiscovery=false;
};
