#include "CombatScreen.h"
#include "CharacterPortraits.h"
#include "GameAudio.h"
#include "SpireStyle.h"
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
namespace {
// 行動メニューの幅と、画面端・駒との余白。駒の隣へ浮かべる位置の計算に使う。
constexpr float FieldMenuWidth=346.f,FieldMenuEdge=12.f,FieldMenuGap=8.f;
// 駒の隣へ浮かべる短いコマンド一覧の幅。盤面を覆う面積を抑える。
constexpr float FieldMenuFloatWidth=260.f;
// 右上の「行動」ボタンの下端。メニューがこのボタンへ重ならない最上端。
constexpr float FieldMenuTop=62.f;
// 短いコマンド一覧の文字サイズと、ボタン内の余白。縦を詰めて盤面を覆いにくくする。
constexpr int FieldCommandFont=14;
constexpr float FieldCommandPadX=8.f,FieldCommandPadY=2.f;
}
void SCombatScreen::BuildField()
{
    const auto label=[this](const TCHAR* text){return SNew(STextBlock).Text(FText::FromString(text)).AutoWrapText(true).Font(FCoreStyle::GetDefaultFontStyle("Regular",FontSize(16)));};
    // 駒の隣へ浮かべる短いコマンド一覧用。文字と余白を小さくして縦を詰める。
    const auto command=[this](const TCHAR* text){return SNew(STextBlock).Text(FText::FromString(text)).Font(FCoreStyle::GetDefaultFontStyle("Regular",FontSize(FieldCommandFont)));};
    const auto mainVisible=[this]{return FieldListHidden()?EVisibility::Collapsed:EVisibility::Visible;};
    const auto heroVisible=[this]{return Combat->Snapshot().ActiveId==0 && !FieldListHidden()?EVisibility::Visible:EVisibility::Collapsed;};
    const auto* frame=FCoreStyle::Get().GetBrush("WhiteBrush");
    const FLinearColor brass(.42f,.29f,.12f);
    const FLinearColor ink(.048f,.034f,.022f);
    ChildSlot[SNew(SBorder).Padding(0).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.025f,.065f,.05f))
        .AddMetaData(FDriverMetaData::Id("battle.field.root"))
        [SAssignNew(FieldRoot,SOverlay)
            +SOverlay::Slot()
            [SNew(SBox).Visibility_Lambda([this]{return MonochromeBoard?EVisibility::Collapsed:EVisibility::HitTestInvisible;})
                [FSpireStyle::Atmosphere(FLinearColor(.02f,.05f,.06f,.85f),FLinearColor(.045f,.02f,.03f,.85f))]]
            +SOverlay::Slot().Padding(16,16,374,16)
            [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
                [SNew(SBox).Padding(FMargin(0,FontSize(36),0,0))
                    [SNew(SOverlay)
                        +SOverlay::Slot()[SAssignNew(Grid,SUniformGridPanel).SlotPadding_Lambda([this]{return FMargin(BoardCellPadding());})]
                        +SOverlay::Slot()[SAssignNew(BoardObjects,SCanvas).Visibility(EVisibility::SelfHitTestInvisible)]
                        +SOverlay::Slot()[SAssignNew(MovingUnits,SCanvas).Visibility(EVisibility::HitTestInvisible)]]]]
            +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(16)
            [SNew(SButton).ButtonStyle(&FSpireStyle::Command()).AddMetaData(FDriverMetaData::Id("battle.field.open"))
                .OnClicked_Lambda([this]{FieldFocus=Combat->Snapshot().ActiveId;FieldAim=EFieldAction::None;FieldSub=EFieldSub::None;FieldMenuOpen=true;return FReply::Handled();})[label(TEXT("行動"))]]
            +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(TAttribute<FMargin>::CreateLambda([this]{return FieldMenuPadding();}))
            [SAssignNew(FieldMenu,SBox).WidthOverride_Lambda([this]{return FOptionalSize(FieldMenuFloating()?FieldMenuFloatWidth:FieldMenuWidth);}).AddMetaData(FDriverMetaData::Id("battle.field.menu"))
                .Visibility_Lambda([this]{return FieldMenuOpen || SavingBlocked || Combat->Snapshot().Outcome!=GirlsSpire::BattleOutcome::Ongoing?EVisibility::Visible:EVisibility::Collapsed;})
                [SNew(SBorder).Padding(2).BorderImage(frame).BorderBackgroundColor(brass)
                    [SNew(SBorder).Padding(12).BorderImage(frame).BorderBackgroundColor(ink)
                    [SNew(SVerticalBox)
                        +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)
                        [SNew(STextBlock).Text_Lambda([this]{return FText::FromString(FieldSelectionLabel());}).AutoWrapText(true).Font(FCoreStyle::GetDefaultFontStyle("Bold",FontSize(16))).AddMetaData(FDriverMetaData::Id("battle.field.selection"))]
                        +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,6)[SAssignNew(ComboGauge,STextBlock).Visibility_Lambda(mainVisible).AutoWrapText(true).Font(FCoreStyle::GetDefaultFontStyle("Regular",FontSize(16))).AddMetaData(FDriverMetaData::Id("battle.combo.gauge"))]
                        +SVerticalBox::Slot().AutoHeight()[SNew(SButton).ButtonStyle(&FSpireStyle::Command()).ContentPadding(FMargin(FieldCommandPadX,FieldCommandPadY)).AddMetaData(FDriverMetaData::Id("battle.field.move"))
                            .Visibility_Lambda(heroVisible)
                            .IsEnabled_Lambda([this]{return !SavingBlocked && !Combat->Reachable(0).empty();})
                            .OnClicked_Lambda([this]{BeginFieldAction(EFieldAction::Move);return FReply::Handled();})[command(TEXT("移動"))]]
                        +SVerticalBox::Slot().AutoHeight()[SNew(SButton).ButtonStyle(&FSpireStyle::Command()).ContentPadding(FMargin(FieldCommandPadX,FieldCommandPadY)).AddMetaData(FDriverMetaData::Id("battle.field.attack"))
                            .Visibility_Lambda(heroVisible)
                            .IsEnabled_Lambda([this]{return !SavingBlocked && !Combat->Snapshot().Acted;})
                            .OnClicked_Lambda([this]{BeginFieldAction(EFieldAction::Attack);return FReply::Handled();})[command(TEXT("攻撃"))]]
                        +SVerticalBox::Slot().AutoHeight()[SNew(SButton).ButtonStyle(&FSpireStyle::Command()).ContentPadding(FMargin(FieldCommandPadX,FieldCommandPadY)).AddMetaData(FDriverMetaData::Id("battle.field.skills"))
                            .Visibility_Lambda(heroVisible)
                            .IsEnabled_Lambda([this]{const auto& state=Combat->Snapshot();const auto actor=std::find_if(state.Units.begin(),state.Units.end(),[&state](const auto& unit){return unit.Id==state.ActiveId;});return !SavingBlocked && actor!=state.Units.end() && std::any_of(actor->Skills.begin(),actor->Skills.end(),[](bool owned){return owned;});})
                            .OnClicked_Lambda([this]{FieldSub=EFieldSub::Skills;Refresh(false);return FReply::Handled();})[command(TEXT("技"))]]
                        +SVerticalBox::Slot().AutoHeight()[SNew(SButton).ButtonStyle(&FSpireStyle::Command()).ContentPadding(FMargin(FieldCommandPadX,FieldCommandPadY)).AddMetaData(FDriverMetaData::Id("battle.field.wait"))
                            .Visibility_Lambda(heroVisible).IsEnabled_Lambda([this]{return !SavingBlocked;})
                            .OnClicked_Lambda([this]{FieldSub=EFieldSub::Wait;Refresh(false);return FReply::Handled();})[command(TEXT("待機"))]]
                        +SVerticalBox::Slot().AutoHeight()[SNew(SButton).ButtonStyle(&FSpireStyle::Command()).ContentPadding(FMargin(FieldCommandPadX,FieldCommandPadY)).AddMetaData(FDriverMetaData::Id("battle.field.ultimate"))
                            .Visibility_Lambda(heroVisible)
                            .IsEnabled_Lambda([this]{const auto& state=Combat->Snapshot();return !SavingBlocked && ((!state.Acted && state.Combos.Gauge>=PersonalUltimateGauge) || Combat->CanTeamUltimate(state.ActiveId));})
                            .OnClicked_Lambda([this]{FieldSub=EFieldSub::Ultimate;Refresh(false);return FReply::Handled();})[command(TEXT("必殺"))]]
                        +SVerticalBox::Slot().AutoHeight()[SNew(SButton).ButtonStyle(&FSpireStyle::Command()).ContentPadding(FMargin(FieldCommandPadX,FieldCommandPadY)).AddMetaData(FDriverMetaData::Id("battle.field.back"))
                            .Visibility_Lambda([this]{return FieldListHidden()?EVisibility::Visible:EVisibility::Collapsed;})
                            .OnClicked_Lambda([this]{FieldAim=EFieldAction::None;FieldSub=EFieldSub::None;Selected.reset();MoveTarget.reset();SelectedSkill.reset();SelectingUltimate=false;SelectingTeamUltimate=false;SelectingMedicine=false;Refresh(false);return FReply::Handled();})[command(TEXT("戻る"))]]
                        +SVerticalBox::Slot().AutoHeight()[SNew(SButton).ButtonStyle(&FSpireStyle::Command()).AddMetaData(FDriverMetaData::Id("battle.field.intent"))
                            .Visibility_Lambda([this]{return Combat->Snapshot().ActiveId>0?EVisibility::Visible:EVisibility::Collapsed;})
                            .OnClicked_Lambda([this]{BeginFieldAction(EFieldAction::Intent);return FReply::Handled();})[label(TEXT("作戦の対象"))]]
                        +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SNew(SButton).ButtonStyle(&FSpireStyle::Command()).ContentPadding(FMargin(FieldCommandPadX,FieldCommandPadY)).AddMetaData(FDriverMetaData::Id("battle.field.close"))
                            .Visibility_Lambda(mainVisible)
                            .OnClicked_Lambda([this]{FieldMenuOpen=false;return FReply::Handled();})[command(TEXT("閉じる"))]]
                        +SVerticalBox::Slot().AutoHeight()[SNew(SButton).ButtonStyle(&FSpireStyle::Command()).ContentPadding(FMargin(FieldCommandPadX,FieldCommandPadY)).AddMetaData(FDriverMetaData::Id("battle.field.details")).Visibility_Lambda(mainVisible)
                            .OnClicked_Lambda([this]{FieldDetails=!FieldDetails;Refresh(false);return FReply::Handled();})
                            [SNew(STextBlock).Text_Lambda([this]{return FText::FromString(FieldDetails?TEXT("詳細を閉じる"):TEXT("詳細"));}).Font(FCoreStyle::GetDefaultFontStyle("Regular",FontSize(14)))]]
                        +SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox).AddMetaData(FDriverMetaData::Id("battle.scroll"))+SScrollBox::Slot()
                            [SNew(SVerticalBox)
                                +SVerticalBox::Slot().AutoHeight()[SAssignNew(Panel,SVerticalBox)]
                                +SVerticalBox::Slot().AutoHeight()[SAssignNew(ComboResult,STextBlock).AutoWrapText(true).Font(FCoreStyle::GetDefaultFontStyle("Regular",FontSize(12))).AddMetaData(FDriverMetaData::Id("battle.combo.result"))
                                    .Visibility_Lambda([this]{return FieldDetails || Combat->Snapshot().LastCombo?EVisibility::Visible:EVisibility::Collapsed;})]
                                +SVerticalBox::Slot().AutoHeight()[SNew(SScrollBox).Orientation(Orient_Horizontal).Visibility_Lambda([this]{return FieldPortraitsVisible()?EVisibility::Visible:EVisibility::Collapsed;})+SScrollBox::Slot()[SAssignNew(PortraitPanel,SHorizontalBox)]]]]]]]]
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
    FieldAim=action;FieldSub=EFieldSub::None;FieldMenuOpen=Combat->Snapshot().ActiveId==0;Selected.reset();MoveTarget.reset();SelectedSkill.reset();SelectingUltimate=false;SelectingTeamUltimate=false;SelectingMedicine=false;Refresh(false);
}
bool SCombatScreen::SelectFieldCell(GirlsSpire::Position cell)
{
    if(SavingBlocked)return true;
    const auto& state=Combat->Snapshot();FieldMenuOpen=true;
    const auto unit=std::find_if(state.Units.begin(),state.Units.end(),[cell](const auto& value){return value.Hp>0 && value.At==cell;});
    if(unit!=state.Units.end())FieldFocus=unit->Id;
    const bool special=SelectedSkill || SelectingUltimate || SelectingMedicine || SelectingTeamUltimate;
    // 下位メニューを開いたまま別の駒（敵を含む）を選んだら、一覧へ戻す。
    if(!special && unit!=state.Units.end())FieldSub=EFieldSub::None;
    if(!special && FieldAim!=EFieldAction::Intent && unit!=state.Units.end() && (unit->Id==state.ActiveId || (state.ActiveId==0 && unit->Side==GirlsSpire::Faction::Player))){FieldAim=EFieldAction::None;FieldSub=EFieldSub::None;Selected.reset();MoveTarget.reset();Refresh(false);return true;}
    if(!special && ((FieldAim==EFieldAction::Move && unit!=state.Units.end()) || (FieldAim==EFieldAction::Attack && unit==state.Units.end() && state.Tiles[cell.Y*state.Width+cell.X].ObstacleHp==0))){Message=FieldAim==EFieldAction::Move?TEXT("移動先の空きマスを選んでください。"):TEXT("攻撃対象を選んでください。");Refresh(false);return true;}
    FieldAim=EFieldAction::None;return false;
}
// 手番の主人公を選んだ直後だけ、駒の隣へメニューを浮かべる。移動先・対象・技の選択中は盤面を隠さないよう右端へ置く。
bool SCombatScreen::FieldMenuFloating() const
{
    const auto& state=Combat->Snapshot();
    return !SavingBlocked && state.Outcome==GirlsSpire::BattleOutcome::Ongoing && state.ActiveId==0 && FieldFocus==state.ActiveId
        && !FieldAiming();
}
// 移動先・対象・技・必殺・傷薬を盤面で選んでいる最中。
bool SCombatScreen::FieldAiming() const
{
    return FieldAim!=EFieldAction::None || Selected || MoveTarget || SelectedSkill || SelectingUltimate || SelectingTeamUltimate || SelectingMedicine;
}
// 主人公の手番の全画面表示。長い本文を出さず、短いコマンド一覧と下位メニューで操作する。
bool SCombatScreen::FieldCompactMode() const
{
    const auto& state=Combat->Snapshot();
    return FullField && !SavingBlocked && state.Outcome==GirlsSpire::BattleOutcome::Ongoing && state.ActiveId==0;
}
bool SCombatScreen::FieldMainList() const
{
    return FieldCompactMode() && !FieldAiming() && FieldSub==EFieldSub::None;
}
// 主人公の手番で、一覧が下位メニューや照準中で隠れているとき。
bool SCombatScreen::FieldListHidden() const
{
    return FieldCompactMode() && !FieldMainList();
}
bool SCombatScreen::FieldPortraitsVisible() const
{
    return !FieldCompactMode() || (FieldDetails && FieldMainList());
}
FMargin SCombatScreen::FieldMenuPadding() const
{
    const auto root=FieldRoot->GetCachedGeometry();const auto rootSize=root.GetLocalSize();
    const FMargin dock(FMath::Max(FieldMenuEdge,rootSize.X-FieldMenuWidth-FieldMenuEdge),FieldMenuTop,0,FieldMenuEdge);
    if(!FieldMenuFloating() || rootSize.IsZero())return dock;
    const auto& state=Combat->Snapshot();
    const auto actor=std::find_if(state.Units.begin(),state.Units.end(),[&state](const auto& unit){return unit.Id==state.ActiveId && unit.Hp>0;});
    const auto grid=Grid->GetCachedGeometry();const auto gridSize=grid.GetLocalSize();
    if(actor==state.Units.end() || gridSize.IsZero() || state.Width<=0 || state.Height<=0)return dock;
    const FVector2D cell(gridSize.X/state.Width,gridSize.Y/state.Height);
    const auto toRoot=[&](const FVector2D& local){return root.AbsoluteToLocal(grid.LocalToAbsolute(local));};
    const auto topLeft=toRoot(FVector2D(actor->At.X*cell.X,actor->At.Y*cell.Y));
    const auto bottomRight=toRoot(FVector2D((actor->At.X+1)*cell.X,(actor->At.Y+1)*cell.Y));
    // 右に収まらない駒では左側へ切り替え、どちらも無理なら右端へ置く。
    float left=bottomRight.X+FieldMenuGap;
    if(left+FieldMenuFloatWidth+FieldMenuEdge>rootSize.X){left=topLeft.X-FieldMenuGap-FieldMenuFloatWidth;if(left<FieldMenuEdge)return dock;}
    const float menuHeight=FieldMenu->GetDesiredSize().Y;
    const float top=FMath::Clamp(topLeft.Y,FieldMenuTop,FMath::Max(FieldMenuTop,rootSize.Y-menuHeight-FieldMenuEdge));
    return FMargin(left,top,0,FieldMenuEdge);
}
// 予想ダメージが残りHPを超えるときは、表示だけ残りHPで丸める。計算結果は変えない。
FString SCombatScreen::PredictedDamage(int targetId,int damage) const
{
    const auto& units=Combat->Snapshot().Units;
    const auto target=std::find_if(units.begin(),units.end(),[targetId](const auto& unit){return unit.Id==targetId;});
    if(!FullField || target==units.end() || damage<=target->Hp)return FString::Printf(TEXT("予想%dダメージ"),damage);
    return FString::Printf(TEXT("予想%dダメージ（残りHPで丸め）"),target->Hp);
}
