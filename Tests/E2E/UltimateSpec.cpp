#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Widgets/SWindow.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Presentation/CombatScreen.h"
#include "../Fixtures/UltimateBattle.h"
#include "../Fixtures/TeamUltimateBattle.h"

BEGIN_DEFINE_SPEC(FGirlsSpireUltimateUiSpec,"GirlsSpire.UI.Ultimate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
    int32 CaptureIndex=0;
END_DEFINE_SPEC(FGirlsSpireUltimateUiSpec)
void FGirlsSpireUltimateUiSpec::Define()
{
    BeforeEach([this] {
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        IConsoleManager::Get().ProcessUserConsoleInput(TEXT("GirlsSpire.Preview"),*GLog,nullptr);
        for(const auto& candidate:FSlateApplication::Get().GetTopLevelWindows())
            if(candidate->GetTitle().ToString()==TEXT("Spiral RPG — 周回プレビュー")) Window=candidate;
        Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    for(int scenario:{0,1,2}) {
        const TCHAR* names[]={TEXT("連携で50を蓄積し主人公の必殺を取消・無効対象・確定で検証する"),
            TEXT("仲間の手動必殺で最後の敵を倒し消費後に連携加算する"),TEXT("必殺が外れても行動とゲージを消費する")};
        It(names[scenario],EAsyncExecution::ThreadPool,FTimespan::FromSeconds(180),[this,scenario] {
            Async(EAsyncExecution::TaskGraphMainThread,[this,scenario] {
                using namespace GirlsSpire;
                auto battle=Test::UltimateBattle(scenario==2?0:1,scenario==0?0:scenario==1?100:50,
                    BattleRole::Standard,scenario==1?130:10000);
                if(scenario==1) battle.EndTurn(battle.Snapshot().Revision,0,Facing::South);
                Window->SetContent(SNew(SCombatScreen).Combat(MakeShared<Battle>(battle)));
            }).Wait();
            const auto click=[this](const char* id){return TestTrue(TEXT("マウス操作"),ClickAndDrain(Driver->FindElement(By::Id(id))));};
            if(!TestTrue(TEXT("必殺の選択肢"),Driver->FindElement(By::Id("battle.ultimate.select"))->Exists())) return;
            if(scenario==0) {
                TestFalse(TEXT("ゲージ不足で選択不可"),Driver->FindElement(By::Id("battle.ultimate.select"))->IsInteractable());
                bool charged=false;
                for(int round=0;round<30 && !charged;++round) {
                    if(!click("battle.wait.south") || !click("intent.trust.75") || !click("intent.execute")) return;
                    charged=Driver->FindElement(By::Id("battle.combo.gauge"))->GetText().ToString().Contains(TEXT("50/100"));
                    if(!click("battle.wait.south")) return;
                }
                if(!TestTrue(TEXT("UIで連携を繰り返し50まで蓄積"),charged)) return;
            }
            if(!click("battle.ultimate.select") || !click("battle.cell.6.1")) return;
            const auto preview=Driver->FindElement(By::Id("battle.ultimate.preview"))->GetText().ToString();
            TestTrue(TEXT("必殺の命中・威力・消費を予告"),preview.Contains(TEXT("85%")) && preview.Contains(TEXT("30")) && preview.Contains(TEXT("50")));
            if(scenario==0) {
                if(!click("battle.ultimate.cancel")) return;
                TestTrue(TEXT("取消でゲージ不変"),Driver->FindElement(By::Id("battle.combo.gauge"))->GetText().ToString().Contains(TEXT("50/100")));
                if(!click("battle.ultimate.select")) return;
                for(const char* id:{"battle.cell.1.1","battle.cell.0.0","battle.cell.4.4"}) {
                    if(!click(id)) return;
                    TestFalse(TEXT("味方・空マス・障害物は無効"),Driver->FindElement(By::Id("battle.ultimate.confirm"))->IsInteractable());
                }
                if(!click("battle.cell.6.1")) return;
            }
            if(!click("battle.ultimate.confirm")) return;
            const auto gauge=Driver->FindElement(By::Id("battle.combo.gauge"))->GetText().ToString();
            TestTrue(TEXT("消費後のゲージ"),gauge.Contains(scenario==1?TEXT("60/100"):TEXT("0/100")));
            if(scenario==1) {
                TestTrue(TEXT("撃破勝利"),Driver->FindElement(By::Id("battle.won"))->Exists());
                TestTrue(TEXT("必殺でも2人連携成立"),Driver->FindElement(By::Id("battle.combo.result"))->GetText().ToString().Contains(TEXT("2人")));
            } else {
                TestTrue(TEXT("結果表示"),Driver->FindElement(By::Id("battle.log"))->GetText().ToString().Contains(scenario==2?TEXT("回避"):TEXT("30ダメージ")));
                TestTrue(TEXT("行動消費"),Driver->FindElement(By::Id("battle.turn"))->GetText().ToString().Contains(TEXT("行動:済")));
                TestFalse(TEXT("連打不可"),Driver->FindElement(By::Id("battle.ultimate.select"))->IsInteractable());
            }
        });
    }
    for(int scenario:{0,1,2,3}) {
        const TCHAR* names[]={TEXT("全員必殺の主人公による範囲予告・取消・無効対象・確定"),
            TEXT("全員必殺の仲間手動発動と連携加算"),TEXT("全員必殺の人数・生存・配置・ゲージ不足を表示"),
            TEXT("全員必殺の一部外れで消費して連携を中断")};
        It(names[scenario],EAsyncExecution::ThreadPool,FTimespan::FromSeconds(180),[this,scenario] {
            const auto setup=[this,scenario](int variant,int gauge,bool companion) {
                Async(EAsyncExecution::TaskGraphMainThread,[this,variant,gauge,companion,scenario] {
                    using namespace GirlsSpire;
                    auto b=Test::TeamUltimateBattle(scenario==3?7:0,gauge,variant);
                    if(companion) b.EndTurn(b.Snapshot().Revision,0,Facing::South);
                    Window->SetContent(SNew(SCombatScreen).Combat(MakeShared<Battle>(b)));
                }).Wait();
            };
            const auto click=[this](const char* id){return TestTrue(TEXT("全員必殺マウス操作"),ClickAndDrain(Driver->FindElement(By::Id(id))));};
            setup(0,100,scenario==1 || scenario==3);
            if(!TestTrue(TEXT("全員必殺ボタン"),Driver->FindElement(By::Id("battle.teamultimate.select"))->Exists())) return;
            if(scenario==2) {
                for(int variant:{1,2,3,4,0}) {
                    setup(variant,variant==0?90:100,false);
                    TestFalse(TEXT("不足条件で選択不可"),Driver->FindElement(By::Id("battle.teamultimate.select"))->IsInteractable());
                }
                return;
            }
            if(!click("battle.teamultimate.select") || !click("battle.cell.6.1")) return;
            const auto preview=Driver->FindElement(By::Id("battle.teamultimate.preview"))->GetText().ToString();
            TestTrue(TEXT("2体分の予告と消費"),preview.Contains(TEXT("敵100")) && preview.Contains(TEXT("敵101")) && preview.Contains(TEXT("50ダメージ")) && preview.Contains(TEXT("100消費")));
            if(!click("battle.teamultimate.cancel")) return;
            TestTrue(TEXT("取消で100維持"),Driver->FindElement(By::Id("battle.combo.gauge"))->GetText().ToString().Contains(TEXT("100/100")));
            if(!click("battle.teamultimate.select")) return;
            for(const char* id:{"battle.cell.0.0","battle.cell.1.1","battle.cell.5.1","battle.cell.7.0"}) {
                if(!click(id)) return;
                TestFalse(TEXT("無効中心の確定不可"),Driver->FindElement(By::Id("battle.teamultimate.confirm"))->IsInteractable());
            }
            if(!click("battle.cell.6.1") || !click("battle.teamultimate.confirm")) return;
            TestTrue(TEXT("全員必殺の結果表示"),Driver->FindElement(By::Id("battle.log"))->GetText().ToString().Contains(TEXT("全員必殺 / ゲージ100消費")));
            TestTrue(TEXT("発動者の行動消費"),Driver->FindElement(By::Id("battle.turn"))->GetText().ToString().Contains(TEXT("行動:済")));
            TestFalse(TEXT("全員必殺の連打不可"),Driver->FindElement(By::Id("battle.teamultimate.select"))->IsInteractable());
            TestTrue(TEXT("消費後に成立分だけ加算"),Driver->FindElement(By::Id("battle.combo.gauge"))->GetText().ToString().Contains(scenario==1?TEXT("10/100"):TEXT("0/100")));
            if(scenario==3) TestTrue(TEXT("外れを表示"),Driver->FindElement(By::Id("battle.log"))->GetText().ToString().Contains(TEXT("回避")));
            if(!click("battle.wait.south")) return;
            TestTrue(TEXT("次の仲間は未行動"),Driver->FindElement(By::Id("battle.turn"))->GetText().ToString().Contains(TEXT("行動:未")));
        });
    }
    AfterEach([this] {
        FSlateApplication::Get().CloseToolTip();
        if(Window.IsValid()) {
            TArray<FColor> pixels;FIntVector size;
            if(TestTrue(TEXT("必殺画面撮影"),FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),pixels,size))) {
                TArray64<uint8> png;FImageUtils::PNGCompressImageArray(size.X,size.Y,pixels,png);
                const auto folder=FPaths::ProjectSavedDir()/TEXT("Screenshots");IFileManager::Get().MakeDirectory(*folder,true);
                TestTrue(TEXT("画像保存"),FFileHelper::SaveArrayToFile(png,*(folder/FString::Printf(TEXT("Ultimate-%d.png"),++CaptureIndex))));
            }
        }
        Driver.Reset();IAutomationDriverModule::Get().Disable();
        if(Window.IsValid()) {FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}
#if defined(GIRLS_SPIRE_COVERAGE)
        TestEqual(TEXT("必殺UI計測"),__llvm_profile_write_file(),0);
#endif
    });
}
