#include "CombatScreen.h"
#include "BoardSprites.h"
#include "BoardPawn.h"
#include "CharacterPortraits.h"
#include "Widgets/SCanvas.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Framework/MetaData/DriverMetaData.h"
#include <algorithm>
FVector2D SCombatScreen::BoardCellSize() const
{
    const bool large=Combat->Snapshot().Width>8 || Combat->Snapshot().Height>8;
    return FullField?FVector2D(large?48:64,large?48:60):FVector2D(FontSize(large?48:64),FontSize(large?48:60));
}
const FSlateBrush* SCombatScreen::UnitSprite(const GirlsSpire::BattleUnit& unit) const
{
    if(MonochromeBoard)return nullptr;
    if(unit.Id>=0 && unit.Id<=8)return PortraitResources->Get(unit.Id,AppearanceSnapshot[unit.Id]).Get();
    return unit.Side==GirlsSpire::Faction::Enemy?Sprites->Enemy(unit):nullptr;
}
void SCombatScreen::RefreshObjects()
{
    BoardObjects->ClearChildren();const auto& state=Combat->Snapshot();const auto cell=BoardCellSize(),pitch=cell+FVector2D(4,4);
    // 描画の差分だけを記録し、再開時の最初の盤面では被弾を再生しない。
    const auto now=FSlateApplication::Get().GetCurrentTime();
    for(const auto& unit:state.Units) {
        const auto* previous=DisplayedHp.Find(unit.Id);
        if(previous && unit.Hp<*previous)ImpactTimes.Add(unit.Id,now);
        DisplayedHp.Add(unit.Id,unit.Hp);
    }
    const float lift=FontSize(36);const auto size=cell+FVector2D(0,lift);
    // 奥の行から描き、足元が手前の物体を前面にする。全体が非入力レイヤー。
    for(int y=0;y<state.Height;++y)for(int x=0;x<state.Width;++x) {
        const auto sprite=FBoardSprites::ObjectFor(state.Tiles[y*state.Width+x],x+y);
        const auto art=MonochromeBoard?nullptr:Sprites->Terrain(sprite);
        const FVector2D at(x*pitch.X+2,y*pitch.Y+2-lift);
        if(art)BoardObjects->AddSlot().Position(at).Size(size)
            [SNew(SBox).Padding(FMargin(0,0,0,FontSize(30))).Visibility(EVisibility::HitTestInvisible)
                [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).VAlign(VAlign_Bottom)
                    [SNew(SImage).Image(art).AddMetaData(FDriverMetaData::Id(*FString::Printf(TEXT("battle.object.%d.%d"),x,y)))]]];
        const auto unit=std::find_if(state.Units.begin(),state.Units.end(),[x,y](const auto& value){return value.Hp>0 && value.At==GirlsSpire::Position{x,y};});
        if(unit==state.Units.end())continue;
        const int id=unit->Id;const auto image=UnitSprite(*unit);
        BoardObjects->AddSlot().Position(at).Size(size)
            [SNew(SBoardPawn).Id(id).Side(unit->Side).Image(image).Active(state.ActiveId==id || (FullField && FieldFocus==id) || (Selected && *Selected==unit->At))
                .Reduced(DisplayPreferences.ReducedEffects).Scale(FontSize(100)/100.f).ImpactTime(ImpactTimes.FindRef(id))
                .Strike(Strikes.FindRef(id))
                .Visibility_Lambda([this,id]{return MotionActive(id)?EVisibility::Collapsed:EVisibility::HitTestInvisible;})
                .AddMetaData(FDriverMetaData::Id(*FString::Printf(TEXT("battle.pawn.%d"),id)))];
        if(FullField){
            const auto cellAt=unit->At;const auto revision=state.Revision;
            // 胴体と足元を選択範囲にし、上の空きマスの中心を透明余白で遮らない。
            BoardObjects->AddSlot().Position(at+FVector2D(cell.X*.18f,lift-cell.Y*.5f+2)).Size(FVector2D(cell.X*.64f,cell.Y))
                [SNew(SButton).ButtonStyle(FCoreStyle::Get(),"NoBorder").ContentPadding(0)
                    .AddMetaData(FDriverMetaData::Id(*FString::Printf(TEXT("battle.unit.select.%d"),id)))
                    .OnClicked_Lambda([this,cellAt,revision]{SelectCell(cellAt,revision);return FReply::Handled();})];
        }
    }
}
