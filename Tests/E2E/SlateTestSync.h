#pragma once
#include "Async/Async.h"
#include "Containers/Ticker.h"

// UE 5.7のDriverは完了通知後に実行中フラグを解除する。
// 次の操作を開始する前に、そのゲームスレッド処理が戻ることを保証する。
inline void DrainSlateLayout();
template<typename Element>
bool ClickAndDrain(const Element& Target)
{
    DrainSlateLayout();
    const bool Result=Target->Click();
    Async(EAsyncExecution::TaskGraphMainThread, [] {}).Wait();
    DrainSlateLayout();
    return Result;
}

// ウィンドウ作成フレームの未配置ジオメトリへカーソルを動かさない。
inline void WaitForSlateLayout(const FDoneDelegate& Done)
{
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
        [Done, Frames=0](float) mutable {
            if (++Frames<3) return true;
            Done.Execute();
            return false;
        }));
}

// 画面再生成後の操作は、古いジオメトリを使わず配置完了を待つ。
// Automation Specのワーカースレッドからだけ呼び出す。
inline void DrainSlateLayout()
{
    auto complete=MakeShared<TPromise<void>,ESPMode::ThreadSafe>();
    auto ready=complete->GetFuture();
    Async(EAsyncExecution::TaskGraphMainThread,[complete] {
        WaitForSlateLayout(FDoneDelegate::CreateLambda([complete]{complete->SetValue();}));
    }).Wait();
    ready.Wait();
}
