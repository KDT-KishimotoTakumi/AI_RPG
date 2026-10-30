#pragma once
#include "CoreMinimal.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include <array>

// 立ち絵の対応と寿命を画面が所有する。ルールや保存状態へ画像を持ち込まない。
class FCharacterPortraits
{
public:
    FCharacterPortraits();
    explicit FCharacterPortraits(FString root);
    static const TCHAR* Name(int actor);
    TSharedPtr<FSlateDynamicImageBrush> Get(int actor);
private:
    FString Root;
    std::array<TSharedPtr<FSlateDynamicImageBrush>,9> Images;
};
