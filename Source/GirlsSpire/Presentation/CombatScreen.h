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
#include <memory>

class SVerticalBox;
class SHorizontalBox;
class SUniformGridPanel;
class STextBlock;
class SCanvas;
class SWrapBox;
class FCharacterPortraits;
class FBoardSprites;
class IGameAudio;
class SCombatScreen : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCombatScreen):_FullField(false) {}
        SLATE_ARGUMENT(bool,FullField)
        SLATE_ARGUMENT(TSharedPtr<GirlsSpire::Battle>, Combat)
        SLATE_ARGUMENT(FString, GenerationSummary)
        SLATE_ARGUMENT(std::optional<GirlsSpire::Profile>, BondProfile)
        SLATE_ARGUMENT(std::optional<GirlsSpire::CombatControlCheckpoint>, SavedControl)
        SLATE_ARGUMENT(std::optional<GirlsSpire::AppearanceSelection>, SavedAppearances)
        SLATE_ARGUMENT(TSharedPtr<FCharacterPortraits>, Portraits)
        SLATE_ARGUMENT(TSharedPtr<FBoardSprites>, Sprites)
        SLATE_ARGUMENT(GirlsSpire::Preferences, DisplayPreferences)
        SLATE_ARGUMENT(std::shared_ptr<IGameAudio>, Audio)
        SLATE_EVENT(FSimpleDelegate, OnFinished)
        SLATE_ARGUMENT(TFunction<bool(const GirlsSpire::SavedCombat&)>, OnCheckpoint)
        SLATE_ARGUMENT(TFunction<bool(const GirlsSpire::SavedCombat&)>, OnSuspend)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    GirlsSpire::CombatControlCheckpoint ControlCheckpoint() const {return Control.Checkpoint();}
private:
    GirlsSpire::AppearanceSelection AppearanceSnapshot{};
    void BuildField();
    bool SelectFieldCell(GirlsSpire::Position cell);
    enum class EFieldAction { None, Move, Attack, Intent };
    void BeginFieldAction(EFieldAction action);
    FString FieldSelectionLabel() const;
    bool FullField=false,FieldMenuOpen=true,FieldDetails=false;
    int FieldFocus=-1,FieldActor=-2;
    EFieldAction FieldAim=EFieldAction::None;
    struct FMovementVisual {std::vector<GirlsSpire::Position> Path;double Started=0;};
    TMap<int,FMovementVisual> Movements;
    TSharedPtr<SCanvas> MovingUnits;
    TSharedPtr<SCanvas> BoardObjects;
    TSharedPtr<FBoardSprites> Sprites;
    FVector2D BoardCellSize() const;
    const FSlateBrush* UnitSprite(const GirlsSpire::BattleUnit& unit) const;
    void RefreshObjects();
    bool MotionActive(int id) const;
    void RecordMovement(const GirlsSpire::Battle& before);
    void RefreshMovingUnits();
    EActiveTimerReturnType AnimateUnits(double now,float delta);
    std::shared_ptr<IGameAudio> Audio;
    int FontSize(int base) const {return GirlsSpire::ScaledFontSize(DisplayPreferences.Text,base);}
    GirlsSpire::Preferences DisplayPreferences;
    void Refresh(bool persist=true);
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
    void Action(const FString& label, const FName& id, TFunction<void()> callback, bool enabled=true, TSharedPtr<SHorizontalBox> row=nullptr,TSharedPtr<SWrapBox> wrap=nullptr);
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
    bool MonochromeBoard=false,ShowThreats=false;
};
