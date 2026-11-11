#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Widgets/SWindow.h"
#include "Presentation/CombatScreen.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

BEGIN_DEFINE_SPEC(FGirlsSpireGrowthUiSpec,"GirlsSpire.UI.Growth",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
    int CaptureIndex=0;
END_DEFINE_SPEC(FGirlsSpireGrowthUiSpec)
void FGirlsSpireGrowthUiSpec::Define()
{
    BeforeEach([this] {
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        IConsoleManager::Get().ProcessUserConsoleInput(TEXT("GirlsSpire.Preview"),*GLog,nullptr);
        for(const auto& w:FSlateApplication::Get().GetTopLevelWindows()) if(w->GetTitle().ToString()==TEXT("Spiral RPG — 周回プレビュー")) Window=w;
        Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    It(TEXT("実戦勝利から受取人を固定し三択を予告して取得する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(180),[this] {
        const auto click=[this](const char* id){
            const auto target=Driver->FindElement(By::Id(id));
            if(!target->IsVisible()) {
                const auto scroll=Driver->FindElement(By::Id("run.scroll"));
                const bool top=scroll->ScrollToBeginning();
                Async(EAsyncExecution::TaskGraphMainThread,[] {}).Wait();
                if(!TestTrue(TEXT("マウスで上へスクロール"),top)) return false;
                if(!target->IsVisible()) {
                    const bool found=scroll->ScrollToEndUntil(By::Id(id));
                    Async(EAsyncExecution::TaskGraphMainThread,[] {}).Wait();
                    if(!TestTrue(TEXT("マウスで対象までスクロール"),found)) return false;
                }
            }
            return TestTrue(*FString::Printf(TEXT("報酬マウス操作 %hs"),id),ClickAndDrain(target));
        };
        for(const char* id:{"title.start","role.confirm","route.node.0","preview.battle","battle.cell.3.1","battle.move.confirm"}) if(!click(id)) return;
        for(int turn=0;turn<12;++turn) {
            if(Driver->FindElement(By::Id("battle.won"))->Exists()) break;
            if(!click("battle.target.100") || !click("battle.confirm")) return;
            if(Driver->FindElement(By::Id("battle.won"))->Exists()) break;
            if(!click("battle.wait.south")) return;
        }
        if(!click("battle.return")) return;
        if(Driver->FindElement(By::Id("rescue.help"))->Exists())if(!click("rescue.choice.1") || !click("rescue.confirm"))return;
        if(!TestTrue(TEXT("受取人の選択"),Driver->FindElement(By::Id("reward.recipient.0"))->Exists())) return;
        TestFalse(TEXT("受取人確定前は三択を見せない"),Driver->FindElement(By::Id("reward.choice.0"))->Exists());
        if(!click("reward.recipient.0")) return;
        TestFalse(TEXT("受取人の選び直し不可"),Driver->FindElement(By::Id("reward.recipient.0"))->Exists());
        TestFalse(TEXT("取得前に経路へ進めない"),Driver->FindElement(By::Id("route.node.0"))->Exists());
        for(const char* id:{"reward.choice.0","reward.choice.1","reward.choice.2"}) TestTrue(TEXT("三択表示"),Driver->FindElement(By::Id(id))->Exists());
        if(!click("reward.choice.1")) return;
        TestTrue(TEXT("効果予告"),Driver->FindElement(By::Id("reward.preview"))->GetText().ToString().Contains(TEXT("＋")));
        if(!click("reward.cancel")) return;
        TestTrue(TEXT("取消は同じ三択へ戻る"),Driver->FindElement(By::Id("reward.choice.0"))->Exists());
        if(!click("reward.choice.0") || !click("reward.confirm")) return;
        TestFalse(TEXT("二度取得不可"),Driver->FindElement(By::Id("reward.confirm"))->Exists());
        TestTrue(TEXT("成長記録に追加技"),Driver->FindElement(By::Id("run.growth"))->GetText().ToString().Contains(TEXT("追加技1")));
        TestTrue(TEXT("次の経路へ"),Driver->FindElement(By::Id("route.node.0"))->Exists());
    });
    for(int skill:{2,3,4}) It(*FString::Printf(TEXT("追加技%dの予告取消確定と再使用待ち"),skill),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this,skill] {
        Async(EAsyncExecution::TaskGraphMainThread,[this] {
            using namespace GirlsSpire;
            BattleUnit hero;hero.At={1,1};hero.Range=3;hero.Speed=30;hero.MaxHp=100;hero.Hp=10;hero.Skills.fill(true);
            BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={4,1};enemy.MaxHp=enemy.Hp=1000;
            auto other=enemy;other.Id=101;other.At={4,2};
            Window->SetContent(SNew(SCombatScreen).Combat(MakeShared<Battle>(8,8,std::vector<Tile>(64),std::vector<BattleUnit>{hero,enemy,other},0)));
        }).Wait();
        const auto click=[this](const char* id){
            const auto target=Driver->FindElement(By::Id(id));
            if(!target->IsVisible()) {
                const auto scroll=Driver->FindElement(By::Id("battle.scroll"));
                scroll->ScrollToBeginning();DrainSlateLayout();
                if(!target->IsVisible()){scroll->ScrollToEndUntil(By::Id(id));DrainSlateLayout();}
            }
            return TestTrue(TEXT("追加技マウス操作"),ClickAndDrain(target));
        };
        const auto id=FString::Printf(TEXT("battle.skill.%d"),skill);
        if(!TestTrue(TEXT("追加技の選択肢"),Driver->FindElement(By::Id(*id))->Exists())) return;
        if(!click(TCHAR_TO_UTF8(*id))) return;
        if(!click(skill==4?"battle.cell.1.1":"battle.cell.4.1")) return;
        const auto preview=Driver->FindElement(By::Id("battle.skill.preview"))->GetText().ToString();
        TestTrue(TEXT("効果予告"),preview.Contains(skill==4?TEXT("回復40"):TEXT("ダメージ10")));
        if(skill==2) TestTrue(TEXT("全対象を予告"),preview.Contains(TEXT("敵101")));
        if(!click("battle.skill.cancel")) return;
        if(!click(TCHAR_TO_UTF8(*id))) return;
        if(!click("battle.cell.0.0")) return;
        TestFalse(TEXT("無効対象は確定不可"),Driver->FindElement(By::Id("battle.skill.confirm"))->IsInteractable());
        if(!click(skill==4?"battle.cell.1.1":"battle.cell.4.1") || !click("battle.skill.confirm")) return;
        const auto status=Driver->FindElement(By::Id("battle.skill.status"))->GetText().ToString();
        TestTrue(TEXT("再使用待ち表示"),status.Contains(skill==3?TEXT("遠当て:2"):skill==4?TEXT("癒しの術:3"):TEXT("薙ぎ払い:3")));
        TestFalse(TEXT("連打不可"),Driver->FindElement(By::Id(*id))->IsInteractable());
    });
    for(bool heal:{false,true}) It(heal?TEXT("仲間の追加回復を作戦で使用する"):TEXT("仲間の範囲技候補で全対象を予告する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this,heal] {
        Async(EAsyncExecution::TaskGraphMainThread,[this,heal] {
            using namespace GirlsSpire;
            BattleUnit hero;hero.At={1,2};hero.Hp=10;hero.MaxHp=100;
            BattleUnit ally;ally.Id=1;ally.At={1,1};ally.Range=3;ally.Speed=30;ally.Skills[heal?4:2]=true;
            BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={4,1};enemy.Hp=enemy.MaxHp=1000;
            auto other=enemy;other.Id=101;other.At={4,2};
            Window->SetContent(SNew(SCombatScreen).Combat(MakeShared<Battle>(8,8,std::vector<Tile>(64),std::vector<BattleUnit>{hero,ally,enemy,other},0)));
        }).Wait();
        const auto click=[this](const char* id){return TestTrue(TEXT("仲間の追加技操作"),ClickAndDrain(Driver->FindElement(By::Id(id))));};
        if(!click(heal?"intent.trust.75":"intent.trust.25") || !click(heal?"intent.kind.2":"intent.kind.0")) return;
        const auto preview=Driver->FindElement(By::Id("intent.forecast"))->GetText().ToString();
        if(heal) {
            TestTrue(TEXT("追加回復の効果予告"),preview.Contains(TEXT("癒しの術")) && preview.Contains(TEXT("回復40")));
            if(!click("intent.execute")) return;
            const auto hpText=Driver->FindElement(By::Id("battle.celllabel.1.2"))->GetText().ToString();
            TestEqual(TEXT("主人公を40回復"),hpText,FString(TEXT("味\n50")));
            TestTrue(TEXT("追加技CD表示"),Driver->FindElement(By::Id("battle.skill.additional"))->GetText().ToString().Contains(TEXT("癒しの術:3")));
        } else {
            TestTrue(TEXT("範囲技候補を表示"),preview.Contains(TEXT("薙ぎ払い")));
            TestTrue(TEXT("全被攻撃対象を表示"),preview.Contains(TEXT("敵100")) && preview.Contains(TEXT("敵101")));
        }
    });
    AfterEach([this] {
        FSlateApplication::Get().CloseToolTip();
        if(Window.IsValid()) {
            TArray<FColor> pixels;FIntVector size;
            if(TestTrue(TEXT("成長画面撮影"),FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),pixels,size))) {
                TArray64<uint8> png;FImageUtils::PNGCompressImageArray(size.X,size.Y,pixels,png);
                const auto folder=FPaths::ProjectSavedDir()/TEXT("Screenshots");IFileManager::Get().MakeDirectory(*folder,true);
                TestTrue(TEXT("撮影保存"),FFileHelper::SaveArrayToFile(png,*(folder/FString::Printf(TEXT("Growth-%d.png"),++CaptureIndex))));
            }
        }
        Driver.Reset();IAutomationDriverModule::Get().Disable();
        if(Window.IsValid()){FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}
#if defined(GIRLS_SPIRE_COVERAGE)
        TestEqual(TEXT("成長UI計測"),__llvm_profile_write_file(),0);
#endif
    });
}
