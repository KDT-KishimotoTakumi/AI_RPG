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

BEGIN_DEFINE_SPEC(FGirlsSpireComboUiSpec, "GirlsSpire.UI.Combo",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
    int32 CaptureIndex=0;
END_DEFINE_SPEC(FGirlsSpireComboUiSpec)

void FGirlsSpireComboUiSpec::Define()
{
    BeforeEach([this] {
        FSlateApplication::Get().CloseToolTip();
        IAutomationDriverModule::Get().Enable();
        IConsoleManager::Get().ProcessUserConsoleInput(TEXT("GirlsSpire.Preview"),*GLog,nullptr);
        for(const auto& candidate:FSlateApplication::Get().GetTopLevelWindows())
            if(candidate->GetTitle().ToString()==TEXT("Girls Spire — 周回プレビュー")) Window=candidate;
        Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    for(int scenario:{2,3,5,-3}) {
        const int count=scenario<0?-scenario:scenario;
        It(scenario<0?FString(TEXT("最後の攻撃が外れると連携が成立せずゲージを加算しない")):
            FString::Printf(TEXT("%d人連携をマウスで成立させ参加者とゲージを表示する"),count),
            EAsyncExecution::ThreadPool,FTimespan::FromSeconds(180),[this,count,scenario] {
            Async(EAsyncExecution::TaskGraphMainThread,[this,count,scenario] {
                using namespace GirlsSpire;
                std::vector<BattleUnit> units;
                const Position positions[]={{1,4},{2,3},{2,5},{5,4},{4,4}};
                for(int i=0;i<count;++i) {
                    BattleUnit unit; unit.Id=i;unit.Speed=50-i;unit.Range=8;unit.Skills={false,false};
                    unit.At=count==5?positions[i]:Position{1+i*2,1};
                    if(count==2) unit.At={1+i,1};
                    if(count==3 && i==1) unit.Skills={true,false};
                    if(count==5 && (i==1 || i==2)) {unit.Skills={false,true};unit.Hp=10;}
                    if(count==5 && i==3) unit.Skills={true,false};
                    units.push_back(unit);
                }
                BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={6,4};
                if(count!=5) enemy.At={6,1};
                enemy.Attack=0;enemy.Movement=0;enemy.Hp=enemy.MaxHp=count==2?1:1000;
                units.push_back(enemy);
                Window->SetContent(SNew(SCombatScreen).Combat(MakeShared<Battle>(8,8,std::vector<Tile>(64),units,scenario<0?42:0)));
            }).Wait();
            if(!TestTrue(TEXT("主人公の待機で起"),ClickAndDrain(Driver->FindElement(By::Id("battle.wait.south"))))) return;
            for(int actor=1;actor<count;++actor) {
                if(count==5 && actor<3)
                    if(!TestTrue(TEXT("支援で承"),ClickAndDrain(Driver->FindElement(By::Id("intent.kind.2"))))) return;
                if(!TestTrue(TEXT("確定予告"),ClickAndDrain(Driver->FindElement(By::Id("intent.trust.75"))))) return;
                if(!TestTrue(TEXT("予告した技を実行"),ClickAndDrain(Driver->FindElement(By::Id("intent.execute"))))) return;
                if(actor+1<count)
                    if(!TestTrue(TEXT("次の仲間へ"),ClickAndDrain(Driver->FindElement(By::Id("battle.wait.south"))))) return;
            }
            const auto gauge=Driver->FindElement(By::Id("battle.combo.gauge"));
            if(!TestTrue(TEXT("ゲージ表示"),gauge->Exists())) return;
            if(scenario<0) {
                TestTrue(TEXT("外れでゲージなし"),gauge->GetText().ToString().Contains(TEXT("0/100")));
                TestTrue(TEXT("成立なし"),Driver->FindElement(By::Id("battle.combo.result"))->GetText().ToString().Contains(TEXT("なし")));
                return;
            }
            TestTrue(TEXT("人数別の加算"),gauge->GetText().ToString().Contains(count==2?TEXT("10/100"):count==3?TEXT("20/100"):TEXT("40/100")));
            const auto result=Driver->FindElement(By::Id("battle.combo.result"))->GetText().ToString();
            TestTrue(TEXT("参加者と成立理由"),result.Contains(TEXT("参加者")) && result.Contains(TEXT("成立理由")));
            for(int id=1;id<count;++id)
                TestTrue(TEXT("連携絆の人数別加算"),result.Contains(FString::Printf(TEXT("味方%d: 連携+%d"),id,count==2?1:count==3?2:3)));
            if(count==2) TestTrue(TEXT("最後の敵を撃破しても連携表示"),Driver->FindElement(By::Id("battle.won"))->Exists());
        });
    }
    AfterEach([this] {
        FSlateApplication::Get().CloseToolTip();
        if(Window.IsValid()) {
            TArray<FColor> pixels;FIntVector size;
            if(TestTrue(TEXT("連携画面撮影"),FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),pixels,size))) {
                TArray64<uint8> png;FImageUtils::PNGCompressImageArray(size.X,size.Y,pixels,png);
                const auto folder=FPaths::ProjectSavedDir()/TEXT("Screenshots");
                IFileManager::Get().MakeDirectory(*folder,true);
                TestTrue(TEXT("画像保存"),FFileHelper::SaveArrayToFile(png,*(folder/FString::Printf(TEXT("Combo-%d.png"),++CaptureIndex))));
            }
        }
        Driver.Reset();IAutomationDriverModule::Get().Disable();
        if(Window.IsValid()) {FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}
#if defined(GIRLS_SPIRE_COVERAGE)
        TestEqual(TEXT("連携UI計測"),__llvm_profile_write_file(),0);
#endif
    });
}
