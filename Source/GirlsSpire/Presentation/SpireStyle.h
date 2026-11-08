#pragma once
#include "Styling/SlateTypes.h"
struct FSlateDynamicImageBrush;
class SWidget;
class FSpireStyle {
public:
    // この文字サイズ以上の文は見出しとして金色で表示する。
    static constexpr int HeadingMinSize=22;
    static const FButtonStyle& Command();
    static const FButtonStyle& Tile();
    static TSharedPtr<FSlateDynamicImageBrush> Backdrop(const FString& path);
    static bool IsHeading(int size);
    static FLinearColor HeadingColor();
    // HP割合から色を選ぶ。数値は別に表示するため、色だけに依存しない。
    static FLinearColor HpSafe();
    static FLinearColor HpWarn();
    static FLinearColor HpDanger();
    static FLinearColor HpColor(float ratio);
    // 上から下へ色が変わる半透明の帯。入力は通す。
    static TSharedRef<SWidget> Atmosphere(const FLinearColor& top,const FLinearColor& bottom);
    // 見出しの下に引く金色の細い区切り線。
    static TSharedRef<SWidget> Rule();
};
