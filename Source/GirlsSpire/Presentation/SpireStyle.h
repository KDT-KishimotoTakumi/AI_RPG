#pragma once
#include "Styling/SlateTypes.h"
struct FSlateDynamicImageBrush;
class FSpireStyle {
public:
    static const FButtonStyle& Command();
    static const FButtonStyle& Tile();
    static TSharedPtr<FSlateDynamicImageBrush> Backdrop(const FString& path);
};
