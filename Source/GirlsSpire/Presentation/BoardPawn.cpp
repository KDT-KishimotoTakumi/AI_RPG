#include "BoardPawn.h"
#include "UnitMotion.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MetaData/DriverMetaData.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Text/STextBlock.h"
void SBoardPawn::Construct(const FArguments& args)
{
    static const FSlateRoundedBoxBrush fill(FLinearColor::White,100.f);
    static const FSlateRoundedBoxBrush ring(FLinearColor::Transparent,100.f,FLinearColor::White,2.f);
    const auto side=args._Side;const float scale=args._Scale;
    const auto tint=side==GirlsSpire::Faction::Player?FLinearColor(.12f,.52f,.9f):side==GirlsSpire::Faction::Enemy?FLinearColor(.9f,.18f,.16f):FLinearColor(.85f,.7f,.25f);
    const auto glyph=side==GirlsSpire::Faction::Player?TEXT("○"):side==GirlsSpire::Faction::Enemy?TEXT("△"):TEXT("◇");
    const auto imageTag=FString(args._Moving?TEXT("battle.motion.art."):TEXT("battle.unit.art."))+FString::FromInt(args._Id);
    ChildSlot[SNew(SOverlay)
        +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0,0,0,30*scale)
            [SNew(SBox).WidthOverride(42*scale).HeightOverride(8*scale)[SNew(SImage).Image(&fill).ColorAndOpacity(FLinearColor(0,0,0,.65f))]]
        +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0,0,0,32*scale)
            [SNew(SBox).WidthOverride(38*scale).HeightOverride(5*scale)[SNew(SImage).Image(&fill).ColorAndOpacity(args._Image?tint:FLinearColor(.6f,.6f,.6f))
                .AddMetaData(FDriverMetaData::Id(*FString::Printf(TEXT("battle.pawn.base.%d"),args._Id)))]]
        +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0,0,0,30*scale)
            [SNew(SBox).WidthOverride(44*scale).HeightOverride(10*scale).Visibility(args._Active?EVisibility::HitTestInvisible:EVisibility::Collapsed)
                [SNew(SImage).Image(&ring).ColorAndOpacity(args._Image?FLinearColor(1,.84f,.38f):FLinearColor::White)
                    .AddMetaData(FDriverMetaData::Id(*FString::Printf(TEXT("battle.pawn.active.%d"),args._Id)))]]
        +SOverlay::Slot().Padding(2*scale,0,2*scale,30*scale)
            [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).VAlign(VAlign_Bottom)
                .Visibility(args._Image?EVisibility::HitTestInvisible:EVisibility::Collapsed)
                .RenderTransform_Lambda([id=args._Id,reduced=args._Reduced,impact=args._ImpactTime,strike=args._Strike]{
                    const auto now=FSlateApplication::Get().GetCurrentTime();
                    const auto shift=impact.IsSet()?FUnitMotion::ImpactShift(now-impact.GetValue(),reduced):0.;
                    auto offset=FVector2D(shift,-FUnitMotion::IdleLift(now+id*.11,reduced));
                    if(strike.IsSet())offset+=strike->Direction*FUnitMotion::ImpactShift(now-strike->Started,reduced);
                    return TOptional<FSlateRenderTransform>(FSlateRenderTransform(offset));
                })
                .AddMetaData(FDriverMetaData::Id(*imageTag))
                [SNew(SImage).Image(args._Image)]]
        +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0,0,0,30*scale)
            [SNew(STextBlock).Text(FText::FromString(glyph)).Font(FCoreStyle::GetDefaultFontStyle("Bold",FMath::RoundToInt(26*scale)))
                .Visibility(args._Image?EVisibility::Collapsed:EVisibility::HitTestInvisible)
                .AddMetaData(FDriverMetaData::Id(*FString::Printf(TEXT("battle.pawn.fallback.%d"),args._Id)))]
    ];
}
