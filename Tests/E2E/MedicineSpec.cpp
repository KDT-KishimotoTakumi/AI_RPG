#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Widgets/SWindow.h"
#include "Presentation/CombatScreen.h"
#include "Presentation/RunScreen.h"

BEGIN_DEFINE_SPEC(FGirlsSpireMedicineUiSpec,"GirlsSpire.UI.Medicine",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
END_DEFINE_SPEC(FGirlsSpireMedicineUiSpec)
void FGirlsSpireMedicineUiSpec::Define()
{
    BeforeEach([this] {
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        IConsoleManager::Get().ProcessUserConsoleInput(TEXT("GirlsSpire.Preview"),*GLog,nullptr);
        for(const auto& w:FSlateApplication::Get().GetTopLevelWindows()) if(w->GetTitle().ToString()==TEXT("Girls Spire — 周回プレビュー")) Window=w;
        Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    for(int actor:{0,1}) It(*FString::Printf(TEXT("人物%dの傷薬予告・取消・無効対象・使用"),actor),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this,actor] {
        using namespace GirlsSpire;
        BattleUnit hero;hero.Id=actor;hero.At={1,1};hero.Hp=1;hero.Speed=30;
        BattleUnit ally;ally.Id=actor==0?1:0;ally.At={1,2};ally.Hp=5;
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={7,7};
        const auto combat=MakeShared<Battle>(8,8,std::vector<Tile>(64),std::vector<BattleUnit>{hero,ally,enemy},42,BattleGoal::Eliminate,3,1);
        Async(EAsyncExecution::TaskGraphMainThread,[this,combat]{Window->SetContent(SNew(SCombatScreen).Combat(combat));}).Wait();
        const auto click=[this](const char* id){return TestTrue(*FString::Printf(TEXT("薬操作 %hs"),id),ClickAndDrain(Driver->FindElement(By::Id(id))));};
        if(!TestTrue(TEXT("傷薬ボタン"),Driver->FindElement(By::Id("battle.medicine"))->Exists()) || !click("battle.medicine")) return;
        if(!click("battle.cell.7.7")) return;
        TestTrue(TEXT("敵へ使用不可"),Driver->FindElement(By::Id("battle.medicine.preview"))->GetText().ToString().Contains(TEXT("使用できません")));
        TestFalse(TEXT("無効対象は確定不可"),Driver->FindElement(By::Id("battle.medicine.confirm"))->IsInteractable());
        if(!click("battle.cell.1.2")) return;
        TestTrue(TEXT("最大HPまで予告"),Driver->FindElement(By::Id("battle.medicine.preview"))->GetText().ToString().Contains(TEXT("25回復")));
        if(!click("battle.medicine.cancel")) return;
        TestTrue(TEXT("取消で残数維持"),Driver->FindElement(By::Id("battle.medicine.count"))->GetText().ToString().Contains(TEXT("1個")));
        TestFalse(TEXT("取消では絆獲得なし"),Driver->FindElement(By::Id("battle.combo.result"))->GetText().ToString().Contains(TEXT("援護+1")));
        for(const char* id:{"battle.medicine","battle.cell.1.2","battle.medicine.confirm"}) if(!click(id)) return;
        TestTrue(TEXT("回復結果"),Driver->FindElement(By::Id("battle.celllabel.1.2"))->GetText().ToString().Contains(TEXT("30")));
        TestTrue(TEXT("薬を1個消費"),Driver->FindElement(By::Id("battle.medicine.count"))->GetText().ToString().Contains(TEXT("0個")));
        TestTrue(TEXT("行動済み"),Driver->FindElement(By::Id("battle.turn"))->GetText().ToString().Contains(TEXT("行動:済")));
        TestFalse(TEXT("薬の連打不可"),Driver->FindElement(By::Id("battle.medicine"))->IsInteractable());
        TestTrue(TEXT("援護の獲得分表示"),Driver->FindElement(By::Id("battle.combo.result"))->GetText().ToString().Contains(TEXT("味方1: 連携+0 / 援護+1")));
    });
    for(bool defeat:{false,true}) It(defeat?TEXT("敗北した戦闘の援護絆を結果から次周へ保持する"):TEXT("勝利した戦闘の援護絆を結果から成長画面へ反映する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this,defeat] {
        using namespace GirlsSpire;
        Profile profile;profile.Bonds.fill(99);
        auto run=std::make_shared<Run>(0,Difficulty::Beginner,HeroRole::Sword,profile);
        run->ChooseNode(run->Snapshot().Revision,0);run->CompleteBattle(run->Snapshot().Revision,BattleOutcome::Won);
        run->ChooseRecipient(run->Snapshot().Revision,0);run->ClaimReward(run->Snapshot().Revision,0);
        if(run->Snapshot().PendingRescue>=0)run->ConverseRescue(run->Snapshot().Revision,ConversationChoice::Neutral);
        for(int step=0;step<24;++step) {
            run->ChooseNode(run->Snapshot().Revision,0);
            const auto kind=run->Snapshot().Active.Kind;
            if(kind==NodeKind::Battle || kind==NodeKind::Elite || kind==NodeKind::Rescue || kind==NodeKind::MidBoss || kind==NodeKind::FinalBoss) break;
            run->CompleteEvent(run->Snapshot().Revision);
        }
        const int id=run->Snapshot().Recruited.front()+1;
        BattleUnit hero;hero.Id=0;hero.At={1,1};hero.Speed=40;hero.Hp=hero.MaxHp=run->MaxHp(0);hero.Skills.fill(true);
        BattleUnit ally;ally.Id=id;ally.At={2,1};ally.Speed=30;ally.Hp=1;ally.MaxHp=run->MaxHp(id);
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={5,1};enemy.Range=8;enemy.Attack=defeat?1000:0;enemy.Accuracy=100;
        auto combat=MakeShared<Battle>(8,8,std::vector<Tile>(64),std::vector<BattleUnit>{hero,ally,enemy},0,defeat?BattleGoal::Eliminate:BattleGoal::Survive,1);
        // 決着済みの実ルール状態を準備し、結果表示から周回反映をUIで確認する。
        combat->UseSkill(0,0,Skill::FirstAid,ally.At);
        for(int step=0;step<30 && combat->Snapshot().Outcome==BattleOutcome::Ongoing;++step) {
            const auto active=combat->Snapshot().ActiveId;
            if(active==100 && defeat) {
                for(const auto& unit:combat->Snapshot().Units) if(unit.Side==Faction::Player && unit.Hp>0) {
                    combat->Attack(combat->Snapshot().Revision,active,unit.At);break;
                }
            }
            combat->EndTurn(combat->Snapshot().Revision,active,Facing::South);
        }
        if(!TestTrue(TEXT("実ルールで決着した準備状態"),combat->Snapshot().Outcome==(defeat?BattleOutcome::Lost:BattleOutcome::Won))) return;
        Async(EAsyncExecution::TaskGraphMainThread,[this,combat,run] {
            Window->SetContent(SNew(SCombatScreen).Combat(combat).BondProfile(run->ProfileState())
                .OnFinished(FSimpleDelegate::CreateLambda([this,combat,run,revision=run->Snapshot().Revision] {
                    TestTrue(TEXT("結果確定"),run->CompleteBattle(revision,combat->Snapshot()));
                    Window->SetContent(SNew(SRunScreen).InitialRun(run));
                })));
        }).Wait();
        auto result=Driver->FindElement(By::Id("battle.bond.result"));
        if(!TestTrue(TEXT("絆結果表示"),result->Exists())) return;
        TestTrue(TEXT("上限までの実増加表示"),result->GetText().ToString().Contains(TEXT("絆99 → 100（実増加+1）")));
        if(!TestTrue(TEXT("戦闘結果を確定"),ClickAndDrain(Driver->FindElement(By::Id("battle.return"))))) return;
        if(defeat) {
            if(!TestTrue(TEXT("敗北からタイトル"),ClickAndDrain(Driver->FindElement(By::Id("result.continue"))))) return;
            TestTrue(TEXT("敗北でも絆100保持"),Driver->FindElement(By::Id("title.bonds"))->GetText().ToString().Contains(TEXT("100")));
        } else TestTrue(TEXT("報酬へ進行"),Driver->FindElement(By::Id("reward.heading"))->Exists());
    });
    AfterEach([this] {
        Driver.Reset();IAutomationDriverModule::Get().Disable();
        if(Window.IsValid()){FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}
#if defined(GIRLS_SPIRE_COVERAGE)
        TestEqual(TEXT("傷薬UI計測"),__llvm_profile_write_file(),0);
#endif
    });
}
