#include "SpireStyle.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Misc/Paths.h"

namespace {
constexpr int AtmosphereBands=8;
constexpr float HpSafeAbove=0.5f,HpWarnAbove=0.25f;
constexpr float RuleHeight=2.f;
}

const FButtonStyle& FSpireStyle::Command()
{
    static const FButtonStyle style=FButtonStyle()
        .SetNormal(FSlateRoundedBoxBrush(FLinearColor(0.025f,0.055f,0.095f,0.94f),3.f,FLinearColor(0.48f,0.36f,0.16f),1.f))
        .SetHovered(FSlateRoundedBoxBrush(FLinearColor(0.08f,0.14f,0.22f),3.f,FLinearColor(0.93f,0.74f,0.36f),2.f))
        .SetPressed(FSlateRoundedBoxBrush(FLinearColor(0.18f,0.12f,0.055f),3.f,FLinearColor(0.93f,0.74f,0.36f),1.f))
        .SetDisabled(FSlateRoundedBoxBrush(FLinearColor(0.03f,0.035f,0.04f,0.8f),3.f,FLinearColor(0.2f,0.19f,0.17f),1.f))
        .SetNormalPadding(FMargin(2)).SetPressedPadding(FMargin(2));
    return style;
}
const FButtonStyle& FSpireStyle::Tile()
{
    static const FButtonStyle style=FButtonStyle()
        .SetNormal(FSlateRoundedBoxBrush(FLinearColor::White,2.f,FLinearColor(0.32f,0.37f,0.42f),1.f))
        .SetHovered(FSlateRoundedBoxBrush(FLinearColor::White,2.f,FLinearColor(1.f,0.85f,0.4f),2.f))
        .SetPressed(FSlateRoundedBoxBrush(FLinearColor::White,2.f,FLinearColor::White,2.f));
    return style;
}
TSharedPtr<FSlateDynamicImageBrush> FSpireStyle::Backdrop(const FString& path)
{
    const auto full=FPaths::ConvertRelativePathToFull(path);
    if(!FPaths::FileExists(full))return nullptr;
    const FName resource(*full);
    static TMap<FName,TWeakPtr<FSlateDynamicImageBrush>> images;
    if(const auto cached=images.Find(resource))if(const auto image=cached->Pin())return image;
    const auto size=FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(resource);
    if(size.X<=0 || size.Y<=0)return nullptr;
    const auto image=MakeShared<FSlateDynamicImageBrush>(resource,FVector2D(size.X,size.Y));
    images.Add(resource,image);return image;
}
bool FSpireStyle::IsHeading(int size){return size>=HeadingMinSize;}
FLinearColor FSpireStyle::HeadingColor(){return FLinearColor(0.96f,0.8f,0.42f);}
FLinearColor FSpireStyle::HpSafe(){return FLinearColor(0.55f,0.9f,0.55f);}
FLinearColor FSpireStyle::HpWarn(){return FLinearColor(0.95f,0.75f,0.25f);}
FLinearColor FSpireStyle::HpDanger(){return FLinearColor(0.95f,0.3f,0.25f);}
FLinearColor FSpireStyle::HpColor(float ratio)
{
    if(ratio>HpSafeAbove)return HpSafe();
    return ratio>HpWarnAbove?HpWarn():HpDanger();
}
TSharedRef<SWidget> FSpireStyle::Atmosphere(const FLinearColor& top,const FLinearColor& bottom)
{
    const auto bands=SNew(SVerticalBox).Visibility(EVisibility::HitTestInvisible);
    for(int band=0;band<AtmosphereBands;++band)
        bands->AddSlot().FillHeight(1)
        [SNew(SBorder).Padding(0).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor(FMath::Lerp(top,bottom,static_cast<float>(band)/(AtmosphereBands-1)))];
    return bands;
}
TSharedRef<SWidget> FSpireStyle::Rule()
{
    return SNew(SBox).HeightOverride(RuleHeight).Visibility(EVisibility::HitTestInvisible)
        [SNew(SBorder).Padding(0).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.62f,0.45f,0.18f))];
}
