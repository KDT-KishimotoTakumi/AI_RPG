#include "SpireStyle.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Misc/Paths.h"

const FButtonStyle& FSpireStyle::Command()
{
    static const FButtonStyle style=FButtonStyle()
        .SetNormal(FSlateRoundedBoxBrush(FLinearColor(0.025f,0.055f,0.095f,0.94f),3.f,FLinearColor(0.48f,0.36f,0.16f),1.f))
        .SetHovered(FSlateRoundedBoxBrush(FLinearColor(0.08f,0.14f,0.22f),3.f,FLinearColor(0.93f,0.74f,0.36f),2.f))
        .SetPressed(FSlateRoundedBoxBrush(FLinearColor(0.18f,0.12f,0.055f),3.f,FLinearColor(0.93f,0.74f,0.36f),1.f))
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
