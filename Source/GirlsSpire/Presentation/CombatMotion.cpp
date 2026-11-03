#include "CombatScreen.h"
#include "UnitMotion.h"
#include "CharacterPortraits.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MetaData/DriverMetaData.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SCanvas.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include <algorithm>

bool SCombatScreen::MotionActive(int id) const
{
    const auto motion=Movements.Find(id);
    return !DisplayPreferences.ReducedEffects && motion && FSlateApplication::Get().GetCurrentTime()-motion->Started<FUnitMotion::Duration(motion->Path);
}
void SCombatScreen::RecordMovement(const GirlsSpire::Battle& before)
{
    if(DisplayPreferences.ReducedEffects)return;
    const auto& old=before.Snapshot();const auto& state=Combat->Snapshot();
    const auto source=std::find_if(old.Units.begin(),old.Units.end(),[&](const auto& unit){return unit.Id==old.ActiveId;});
    const auto target=std::find_if(state.Units.begin(),state.Units.end(),[&](const auto& unit){return unit.Id==old.ActiveId;});
    if(source==old.Units.end() || target==state.Units.end() || source->At==target->At)return;
    const auto preview=before.PreviewMove(source->Id,target->At);
    if(!preview.Legal || preview.Path.empty() || preview.Path.back()!=target->At)return;
    FMovementVisual motion;motion.Path.push_back(source->At);motion.Path.insert(motion.Path.end(),preview.Path.begin(),preview.Path.end());
    motion.Started=FSlateApplication::Get().GetCurrentTime();Movements.Add(source->Id,MoveTemp(motion));
}
EActiveTimerReturnType SCombatScreen::AnimateUnits(double,float)
{
    Invalidate(EInvalidateWidgetReason::Paint);return EActiveTimerReturnType::Continue;
}
void SCombatScreen::RefreshMovingUnits()
{
    MovingUnits->ClearChildren();
    const auto& state=Combat->Snapshot();const bool large=state.Width>8 || state.Height>8;
    const FVector2D size(FontSize(large?36:48),FontSize(large?48:60)),pitch=size+FVector2D(4,4);
    for(const auto& entry:Movements) {
        const auto id=entry.Key;if(!MotionActive(id))continue;
        const auto motion=entry.Value;const auto destination=motion.Path.back();
        const auto art=MonochromeBoard?nullptr:PortraitResources->Get(id);
        // 文字と入力は確定位置に残し、画像だけを盤面より上の非入力レイヤーで動かす。
        TSharedPtr<SWidget> sprite;
        if(art)sprite=SNew(SImage).Image(art.Get()).ColorAndOpacity(FLinearColor(1,1,1,.9f));
        else sprite=SNew(STextBlock).Text(FText::FromString(TEXT("◆"))).Justification(ETextJustify::Center)
            .Font(FCoreStyle::GetDefaultFontStyle("Bold",FontSize(30))).ColorAndOpacity(FLinearColor::White);
        sprite->AddMetadata(FDriverMetaData::Id(*FString::Printf(TEXT("battle.motion.unit.%d"),id)));
        sprite->SetVisibility(TAttribute<EVisibility>::CreateLambda([this,id]{return MotionActive(id)?EVisibility::HitTestInvisible:EVisibility::Collapsed;}));
        sprite->SetRenderTransform(TAttribute<TOptional<FSlateRenderTransform>>::CreateLambda([this,motion,destination,pitch]{
            const auto age=FSlateApplication::Get().GetCurrentTime()-motion.Started;
            const auto at=FUnitMotion::Sample(motion.Path,age,DisplayPreferences.ReducedEffects);
            const FVector2D offset((at.X-destination.X)*pitch.X,(at.Y-destination.Y)*pitch.Y-FUnitMotion::IdleLift(age,DisplayPreferences.ReducedEffects));
            return TOptional<FSlateRenderTransform>(FSlateRenderTransform(offset));
        }));
        MovingUnits->AddSlot().Position(FVector2D(destination.X*pitch.X+2,destination.Y*pitch.Y+2)).Size(size)[sprite.ToSharedRef()];
    }
}
