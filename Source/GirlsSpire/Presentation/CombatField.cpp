#include "CombatScreen.h"
#include "CharacterPortraits.h"
#include "GameAudio.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MetaData/DriverMetaData.h"
#include "Styling/CoreStyle.h"
#include <algorithm>
void SCombatScreen::BuildField()
{
    const auto label=[this](const TCHAR* text){return SNew(STextBlock).Text(FText::FromString(text)).AutoWrapText(true).Font(FCoreStyle::GetDefaultFontStyle("Regular",FontSize(16)));};
    const auto* frame=FCoreStyle::Get().GetBrush("WhiteBrush");
    const FLinearColor brass(.42f,.29f,.12f);
    const FLinearColor ink(.048f,.034f,.022f);
    ChildSlot[SNew(SBorder).Padding(0).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.025f,.065f,.05f))
        .AddMetaData(FDriverMetaData::Id("battle.field.root"))
        [SNew(SOverlay)
            +SOverlay::Slot().Padding(16,16,374,16)
            [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
                [SNew(SBox).Padding(FMargin(0,FontSize(36),0,0))
                    [SNew(SOverlay)
                        +SOverlay::Slot()[SAssignNew(Grid,SUniformGridPanel).SlotPadding_Lambda([this]{return FMargin(BoardCellPadding());})]
                        +SOverlay::Slot()[SAssignNew(BoardObjects,SCanvas).Visibility(EVisibility::SelfHitTestInvisible)]
                        +SOverlay::Slot()[SAssignNew(MovingUnits,SCanvas).Visibility(EVisibility::HitTestInvisible)]]]]
            +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(16)
            [SNew(SButton).AddMetaData(FDriverMetaData::Id("battle.field.open"))
                .OnClicked_Lambda([this]{FieldFocus=Combat->Snapshot().ActiveId;FieldAim=EFieldAction::None;FieldMenuOpen=true;return FReply::Handled();})[label(TEXT("行動"))]]
            +SOverlay::Slot().HAlign(HAlign_Right).Padding(12,62,12,12)
            [SNew(SBox).WidthOverride(346).AddMetaData(FDriverMetaData::Id("battle.field.menu"))
                .Visibility_Lambda([this]{return FieldMenuOpen || SavingBlocked || Combat->Snapshot().Outcome!=GirlsSpire::BattleOutcome::Ongoing?EVisibility::Visible:EVisibility::Collapsed;})
                [SNew(SBorder).Padding(2).BorderImage(frame).BorderBackgroundColor(brass)
                    [SNew(SBorder).Padding(12).BorderImage(frame).BorderBackgroundColor(ink)
                    [SNew(SVerticalBox)
                        +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)
                        [SNew(STextBlock).Text_Lambda([this]{return FText::FromString(FieldSelectionLabel());}).AutoWrapText(true).Font(FCoreStyle::GetDefaultFontStyle("Bold",FontSize(16))).AddMetaData(FDriverMetaData::Id("battle.field.selection"))]
                        +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,6)[SAssignNew(ComboGauge,STextBlock).AutoWrapText(true).Font(FCoreStyle::GetDefaultFontStyle("Regular",FontSize(16))).AddMetaData(FDriverMetaData::Id("battle.combo.gauge"))]
                        +SVerticalBox::Slot().AutoHeight()[SNew(SButton).AddMetaData(FDriverMetaData::Id("battle.field.move"))
                            .Visibility_Lambda([this]{return Combat->Snapshot().ActiveId==0?EVisibility::Visible:EVisibility::Collapsed;})
                            .IsEnabled_Lambda([this]{return !SavingBlocked && !Combat->Reachable(0).empty();})
                            .OnClicked_Lambda([this]{BeginFieldAction(EFieldAction::Move);return FReply::Handled();})[label(TEXT("移動"))]]
                        +SVerticalBox::Slot().AutoHeight()[SNew(SButton).AddMetaData(FDriverMetaData::Id("battle.field.attack"))
                            .Visibility_Lambda([this]{return Combat->Snapshot().ActiveId==0?EVisibility::Visible:EVisibility::Collapsed;})
                            .IsEnabled_Lambda([this]{return !SavingBlocked && !Combat->Snapshot().Acted;})
                            .OnClicked_Lambda([this]{BeginFieldAction(EFieldAction::Attack);return FReply::Handled();})[label(TEXT("攻撃"))]]
                        +SVerticalBox::Slot().AutoHeight()[SNew(SButton).AddMetaData(FDriverMetaData::Id("battle.field.intent"))
                            .Visibility_Lambda([this]{return Combat->Snapshot().ActiveId>0?EVisibility::Visible:EVisibility::Collapsed;})
                            .OnClicked_Lambda([this]{BeginFieldAction(EFieldAction::Intent);return FReply::Handled();})[label(TEXT("作戦の対象"))]]
                        +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SNew(SButton).AddMetaData(FDriverMetaData::Id("battle.field.close"))
                            .OnClicked_Lambda([this]{FieldMenuOpen=false;return FReply::Handled();})[label(TEXT("閉じる"))]]
                        +SVerticalBox::Slot().AutoHeight()[SNew(SButton).AddMetaData(FDriverMetaData::Id("battle.field.details"))
                            .OnClicked_Lambda([this]{FieldDetails=!FieldDetails;Refresh(false);return FReply::Handled();})
                            [SNew(STextBlock).Text_Lambda([this]{return FText::FromString(FieldDetails?TEXT("詳細を閉じる"):TEXT("詳細"));}).Font(FCoreStyle::GetDefaultFontStyle("Regular",FontSize(14)))]]
                        +SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox).AddMetaData(FDriverMetaData::Id("battle.scroll"))+SScrollBox::Slot()
                            [SNew(SVerticalBox)
                                +SVerticalBox::Slot().AutoHeight()[SAssignNew(Panel,SVerticalBox)]
                                +SVerticalBox::Slot().AutoHeight()[SAssignNew(ComboResult,STextBlock).AutoWrapText(true).Font(FCoreStyle::GetDefaultFontStyle("Regular",FontSize(12))).AddMetaData(FDriverMetaData::Id("battle.combo.result"))
                                    .Visibility_Lambda([this]{return FieldDetails || Combat->Snapshot().LastCombo?EVisibility::Visible:EVisibility::Collapsed;})]
                                +SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(FontSize(156))[SNew(SScrollBox).Orientation(Orient_Horizontal)+SScrollBox::Slot()[SAssignNew(PortraitPanel,SHorizontalBox)]]]]]]]]]
            +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(16)
            [SNew(STextBlock).Text(FText::FromString(TEXT("音声を利用できません。音なしでも続けられます。"))).AutoWrapText(true).Font(FCoreStyle::GetDefaultFontStyle("Regular",FontSize(14)))
                .Visibility_Lambda([this]{return Audio && !Audio->Ready()?EVisibility::HitTestInvisible:EVisibility::Collapsed;}).AddMetaData(FDriverMetaData::Id("battle.field.audio"))]]];
}
FString SCombatScreen::FieldSelectionLabel() const
{
    const auto& state=Combat->Snapshot();const auto selected=std::find_if(state.Units.begin(),state.Units.end(),[this](const auto& unit){return unit.Id==FieldFocus;});
    const FString owner=state.ActiveId>=0 && state.ActiveId<9?FCharacterPortraits::Name(state.ActiveId):TEXT("戦闘結果");
    if(selected==state.Units.end())return owner;
    if(selected->Id==state.ActiveId)return FString::Printf(TEXT("%s　HP%d/%d"),*owner,selected->Hp,selected->MaxHp);
    const FString name=selected->Id>=0 && selected->Id<9?FCharacterPortraits::Name(selected->Id):FString::Printf(TEXT("敵%d"),selected->Id);
    return FString::Printf(TEXT("手番：%s\n選択：%s　HP%d/%d"),*owner,*name,selected->Hp,selected->MaxHp);
}
void SCombatScreen::BeginFieldAction(EFieldAction action)
{
    if(SavingBlocked || Combat->Snapshot().Outcome!=GirlsSpire::BattleOutcome::Ongoing)return;
    FieldAim=action;FieldMenuOpen=false;Selected.reset();MoveTarget.reset();SelectedSkill.reset();SelectingUltimate=false;SelectingTeamUltimate=false;SelectingMedicine=false;Refresh(false);
}
bool SCombatScreen::SelectFieldCell(GirlsSpire::Position cell)
{
    if(SavingBlocked)return true;
    const auto& state=Combat->Snapshot();FieldMenuOpen=true;
    const auto unit=std::find_if(state.Units.begin(),state.Units.end(),[cell](const auto& value){return value.Hp>0 && value.At==cell;});
    if(unit!=state.Units.end())FieldFocus=unit->Id;
    const bool special=SelectedSkill || SelectingUltimate || SelectingMedicine || SelectingTeamUltimate;
    if(!special && FieldAim!=EFieldAction::Intent && unit!=state.Units.end() && (unit->Id==state.ActiveId || (state.ActiveId==0 && unit->Side==GirlsSpire::Faction::Player))){FieldAim=EFieldAction::None;Selected.reset();MoveTarget.reset();Refresh(false);return true;}
    if(!special && ((FieldAim==EFieldAction::Move && unit!=state.Units.end()) || (FieldAim==EFieldAction::Attack && unit==state.Units.end() && state.Tiles[cell.Y*state.Width+cell.X].ObstacleHp==0))){Message=FieldAim==EFieldAction::Move?TEXT("移動先の空きマスを選んでください。"):TEXT("攻撃対象を選んでください。");Refresh(false);return true;}
    FieldAim=EFieldAction::None;return false;
}
