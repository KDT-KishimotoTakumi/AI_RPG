#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/CombatScreen.h"
BEGIN_DEFINE_SPEC(FGirlsSpireBattleBoostSpec,"GirlsSpire.UI.BattleBoost",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
END_DEFINE_SPEC(FGirlsSpireBattleBoostSpec)
void FGirlsSpireBattleBoostSpec::Define()
{
    BeforeEach([this]{FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();Window=SNew(SWindow).ClientSize(FVector2D(1200,1000));FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();});
    LatentBeforeEach(WaitForSlateLayout);
    for(int mode=0;mode<3;++mode)It(*FString::Printf(TEXT("強化付きの行動%dを予告・確定・保存する"),mode),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(90),[this,mode]{
        using namespace GirlsSpire;
        BattleUnit hero;hero.At={1,1};hero.Attack=20;hero.Accuracy=60;hero.Speed=20;hero.Hp=hero.MaxHp=100;hero.Skills={true,true};
        auto enemy=hero;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={2,1};enemy.Speed=10;enemy.Defense=3;enemy.Evasion=0;enemy.Hp=enemy.MaxHp=200;
        Battle initial(8,8,std::vector<Tile>(64),{hero,enemy},42);auto checkpoint=initial.Checkpoint();checkpoint.State.AbilityRules=AbilityRuleSet::Character;checkpoint.State.Combos.Gauge=50;
        checkpoint.State.Units[0].AttackBoost={6,true};checkpoint.State.Units[0].DefenseBoost={4,true};checkpoint.State.Units[0].AccuracyBoost={15,true};checkpoint.State.Units[1].DefenseBoost={4,false};
        const auto restored=Battle::Restore(checkpoint);if(!TestTrue(TEXT("有効な強化状態"),restored.has_value()))return;
        auto combat=MakeShared<Battle>(*restored);auto expected=*combat;
        const auto saved=std::make_shared<std::optional<SavedCombat>>();
        Async(EAsyncExecution::TaskGraphMainThread,[this,combat,saved]{Window->SetContent(SNew(SCombatScreen).Combat(combat).BondProfile(Profile{}).OnCheckpoint([saved](const SavedCombat& value){*saved=value;return true;}));}).Wait();DrainSlateLayout();
        const auto info=Driver->FindElement(By::Id("battle.boost.0"))->GetText().ToString();
        TestTrue(TEXT("強度と自己使用手番の除外期限を表示"),info.Contains(TEXT("攻撃+6 / 防御+4 / 命中+15")) && info.Contains(TEXT("使用手番を除外")));
        TestTrue(TEXT("外部付与の期限も表示"),Driver->FindElement(By::Id("battle.boost.100"))->GetText().ToString().Contains(TEXT("自身の手番終了まで")));
        const auto click=[&](const char* id){const auto target=Driver->FindElement(By::Id(id));if(!target->IsVisible()){const auto scroll=Driver->FindElement(By::Id("battle.scroll"));scroll->ScrollToBeginning();DrainSlateLayout();if(!target->IsVisible()){scroll->ScrollToEndUntil(By::Id(id));DrainSlateLayout();}}return TestTrue(UTF8_TO_TCHAR(id),ClickAndDrain(target));};
        if(mode && !click(mode==1?"battle.skill.0":"battle.ultimate.select"))return;
        const auto before=combat->Checkpoint();if(!click("battle.cell.2.1"))return;
        TestTrue(TEXT("予告で乱数/強度/期限不変"),combat->Checkpoint()==before);
        const auto prediction=mode==0?combat->PreviewAttack(0,enemy.At):mode==1?combat->PreviewSkill(0,Skill::PowerStrike,enemy.At).Attack:combat->PreviewUltimate(0,enemy.At);
        TestTrue(TEXT("強化と防御を含む命中75/被害19・27・39"),prediction.Legal && prediction.Hits[0].Chance==75 && prediction.Hits[0].Damage==(mode==0?19:mode==1?27:39));
        if(mode==0)expected.Attack(expected.Snapshot().Revision,0,enemy.At);else if(mode==1)expected.UseSkill(expected.Snapshot().Revision,0,Skill::PowerStrike,enemy.At);else expected.UseUltimate(expected.Snapshot().Revision,0,enemy.At);
        if(!click(mode==0?"battle.confirm":mode==1?"battle.skill.confirm":"battle.ultimate.confirm"))return;
        TestTrue(TEXT("実行/乱数/強化期限と保存コールバックが一致"),combat->Checkpoint()==expected.Checkpoint() && saved->has_value() && saved->value().Battle==expected.Checkpoint());
        Run run(42,Difficulty::Beginner,HeroRole::Sword);auto progress=run.Checkpoint();progress.State.Day=5;progress.State.Phase=RunPhase::Event;progress.State.Active={NodeKind::Battle};progress.State.Choices.clear();
        if(saved->has_value()){const RunSaveState state{progress,saved->value()};const auto bytes=EncodeRunPayload(state);TestTrue(TEXT("UI確定後の全強化/作戦/RNGをv13で往復"),bytes && DecodeRunPayload(*bytes)==state);}
    });
    AfterEach([this]{Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}IAutomationDriverModule::Get().Disable();});
}
