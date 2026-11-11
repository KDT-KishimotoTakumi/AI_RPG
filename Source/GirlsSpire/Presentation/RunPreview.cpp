#include "RunScreen.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Widgets/SWindow.h"

// 製品起動とは分離した、進行と画面入力の検証用エントリー。
static FAutoConsoleCommand RunPreviewCommand(
    TEXT("GirlsSpire.Preview"), TEXT("周回進行の検証用画面を開く"),
    FConsoleCommandDelegate::CreateLambda([] {
        FSlateApplication::Get().AddWindow(
            SNew(SWindow).Title(FText::FromString(TEXT("Spiral RPG — 周回プレビュー")))
            .ClientSize(FVector2D(1100, 760)).SupportsMaximize(false)
            [ SNew(SRunScreen) ]);
    }));
