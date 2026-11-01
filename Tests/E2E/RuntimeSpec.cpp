#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Infrastructure/DiskSlotFiles.h"
#include <algorithm>

BEGIN_DEFINE_SPEC(FGirlsSpireRuntimeSpec,"GirlsSpire.Runtime.GameEntry",EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
END_DEFINE_SPEC(FGirlsSpireRuntimeSpec)
void FGirlsSpireRuntimeSpec::Define()
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("GirlsSpireRuntimeTest"))) return;
    BeforeEach([this]{FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();Driver=IAutomationDriverModule::Get().CreateDriver();});
    LatentBeforeEach(WaitForSlateLayout);
    It(TEXT("通常ゲームの画面から出発・実戦・保存・中断できる"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(180),[this] {
        using namespace GirlsSpire;
        if(!TestTrue(TEXT("ゲーム用ビューポート"),GEngine && GEngine->GameViewport)) return;
        if(!TestTrue(TEXT("通常起動でタイトルを表示"),Driver->FindElement(By::Id("title.start"))->Exists())) return;
        const auto click=[&](const char* id) {
            const auto target=Driver->FindElement(By::Id(id));
            if(!target->IsVisible()) {
                const auto outer=Driver->FindElement(By::Id("run.scroll"));
                if(outer->Exists()){outer->ScrollToBeginning();DrainSlateLayout();}
                const auto panel=Driver->FindElement(By::Id("battle.scroll"));
                if(!target->IsVisible()) {
                    if(panel->Exists()){panel->ScrollToBeginning();DrainSlateLayout();}
                    if(!target->IsVisible()){(panel->Exists()?panel:outer)->ScrollToEndUntil(By::Id(id));DrainSlateLayout();}
                }
            }
            if(!TestTrue(*FString::Printf(TEXT("ゲーム操作 %hs"),id),ClickAndDrain(target)))return false;
            // 実IOが失敗した場合も、結果を作り直さず製品の再試行操作を検証する。
            if(Driver->FindElement(By::Id("save.error"))->Exists()) {
                AddWarning(FString::Printf(TEXT("%hs 後に保存エラーを表示。同じ内容の再試行を1回検証する。"),id));
                if(!TestTrue(TEXT("保存を画面から再試行"),ClickAndDrain(Driver->FindElement(By::Id("save.retry")))))return false;
                if(!TestFalse(TEXT("再試行後に保存停止を解消"),Driver->FindElement(By::Id("save.error"))->Exists()))return false;
            }
            return true;
        };
        if(!click("title.start") || !click("role.confirm") || !click("route.node.0")) return;
        TestFalse(TEXT("進行検証の勝利入力を表示しない"),Driver->FindElement(By::Id("preview.complete"))->Exists());
        if(!click("battle.start")) return;
        FString root;FParse::Value(FCommandLine::Get(),TEXT("GirlsSpireRuntimeRoot="),root);root=FPaths::ConvertRelativePathToFull(root);
        const auto prefix=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("Saved/Tests/Runtime"));
        if(!TestTrue(TEXT("専用の保存先"),root!=prefix && FPaths::IsUnderDirectory(root,prefix))) return;
        DiskSlotFiles files{std::filesystem::path(*root)};SaveTransaction service(files,files);
        const auto entered=service.Load();
        if(!TestTrue(TEXT("実戦の開始を保存"),entered.Run && entered.Run->Combat)) return;
        const auto battle=Battle::Restore(entered.Run->Combat->Battle);
        if(!TestTrue(TEXT("実戦復元"),battle.has_value())) return;
        const auto reachable=battle->Reachable(battle->Snapshot().ActiveId);
        const auto target=std::find_if(reachable.begin(),reachable.end(),[&](Position at){const auto preview=battle->PreviewMove(battle->Snapshot().ActiveId,at);return preview.Legal && preview.Damage==0;});
        if(!TestTrue(TEXT("安全な移動先"),target!=reachable.end())) return;
        const auto cell=FString::Printf(TEXT("battle.cell.%d.%d"),target->X,target->Y);
        if(!click(TCHAR_TO_UTF8(*cell)) || !click("battle.move.confirm")) return;
        const auto moved=service.Load();
        TestTrue(TEXT("通常起動の行動も保存される"),moved.Run && moved.Run->Combat && moved.Run->Combat->Battle.State.Revision>entered.Run->Combat->Battle.State.Revision);
        if(!click("save.suspend")) return;
        TestTrue(TEXT("通常起動の中断と再開操作"),Driver->FindElement(By::Id("title.resume"))->Exists());
    });
    AfterEach([this]{Driver.Reset();IAutomationDriverModule::Get().Disable();});
}
