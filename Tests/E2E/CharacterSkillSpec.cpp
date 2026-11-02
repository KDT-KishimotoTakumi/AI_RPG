#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/CombatScreen.h"
#include "Domain/AbilityCatalog.h"
BEGIN_DEFINE_SPEC(FGirlsSpireCharacterSkillSpec,"GirlsSpire.UI.CharacterSkills",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
END_DEFINE_SPEC(FGirlsSpireCharacterSkillSpec)
void FGirlsSpireCharacterSkillSpec::Define()
{
    BeforeEach([this]{FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();Window=SNew(SWindow).ClientSize(FVector2D(1200,1000));FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();});
    LatentBeforeEach(WaitForSlateLayout);
    for(int id=0;id<9;++id)for(int slot=0;slot<2;++slot)It(*FString::Printf(TEXT("人物%dの技能%dを正式名で予告・確定・保存する"),id,slot),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(90),[this,id,slot]{
        using namespace GirlsSpire;const auto skill=static_cast<Skill>(slot);const auto ability=*FormalSkill(id,skill);
        BattleUnit actor;actor.Id=id;actor.At={1,1};actor.Hp=actor.MaxHp=100;actor.Attack=20;actor.Accuracy=200;actor.Speed=30;actor.Skills[slot]=true;
        auto ally=actor;ally.Id=id?0:1;ally.At={1,2};ally.Speed=20;ally.Hp=50;ally.AttackCursed=true;ally.MovementWebbed=true;
        auto enemy=actor;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={2,1};enemy.Speed=10;enemy.Hp=enemy.MaxHp=200;
        Battle arena(8,8,std::vector<Tile>(64),{actor,ally,enemy},42);auto checkpoint=arena.Checkpoint();checkpoint.State.AbilityRules=AbilityRuleSet::CharacterSkills;checkpoint.State.StartingBonds.fill(100);
        const auto restored=Battle::Restore(checkpoint);if(!TestTrue(TEXT("正式技能の戦闘復元"),restored.has_value()))return;
        auto combat=MakeShared<Battle>(*restored);auto expected=*combat;CombatControlCheckpoint control;control.ProfileDriven=true;control.ChoiceRandomState=42;for(int i=1;i<=8;++i)control.Trust[i]=100;
        const auto saved=std::make_shared<std::optional<SavedCombat>>();
        Async(EAsyncExecution::TaskGraphMainThread,[this,combat,control,saved]{Window->SetContent(SNew(SCombatScreen).Combat(combat).SavedControl(control).OnCheckpoint([saved](const SavedCombat& value){*saved=value;return true;}));}).Wait();DrainSlateLayout();
        const auto click=[&](const char* name){const auto target=Driver->FindElement(By::Id(name));if(!target->IsVisible()){const auto scroll=Driver->FindElement(By::Id("battle.scroll"));scroll->ScrollToBeginning();DrainSlateLayout();if(!target->IsVisible()){scroll->ScrollToEndUntil(By::Id(name));DrainSlateLayout();}}return TestTrue(UTF8_TO_TCHAR(name),ClickAndDrain(target));};
        const auto target=ability.Target==AbilityTarget::Enemy?enemy.At:ability.Target==AbilityTarget::Self?actor.At:ally.At;
        const auto name=FString(UTF8_TO_TCHAR(ability.Name.data()));
        TestTrue(TEXT("再使用表示が正式名を含む"),Driver->FindElement(By::Id("battle.skill.status"))->GetText().ToString().Contains(name));
        if(id){const int kind=ability.Target!=AbilityTarget::Enemy?2:ability.Syntax==Twist?3:0;const auto key=FString::Printf(TEXT("intent.kind.%d"),kind);if(!click(TCHAR_TO_UTF8(*key)))return;}
        else {const auto key=FString::Printf(TEXT("battle.skill.%d"),slot);if(!click(TCHAR_TO_UTF8(*key)))return;}
        const auto before=combat->Checkpoint();const auto cell=FString::Printf(TEXT("battle.cell.%d.%d"),target.X,target.Y);if(!click(TCHAR_TO_UTF8(*cell)))return;
        TestTrue(TEXT("対象予告で盤面/乱数不変"),combat->Checkpoint()==before);
        if(id)TestTrue(TEXT("作戦予告が正式技能名を含む"),Driver->FindElement(By::Id("intent.forecast"))->GetText().ToString().Contains(name));
        else TestTrue(TEXT("主人公の対象予告を表示"),Driver->FindElement(By::Id("battle.skill.preview"))->Exists());
        expected.UseSkill(expected.Snapshot().Revision,id,skill,target);
        if(!click(id?"intent.execute":"battle.skill.confirm"))return;
        TestTrue(TEXT("確定は正式効果/期限/CD/構文/乱数と一致"),combat->Checkpoint()==expected.Checkpoint() && saved->has_value() && saved->value().Battle==expected.Checkpoint());
        Run run(42,Difficulty::Beginner,HeroRole::Sword);auto progress=run.Checkpoint();progress.State.Day=5;progress.State.Phase=RunPhase::Event;progress.State.Active={NodeKind::Battle};progress.State.Choices.clear();progress.State.Recruited={id?id-1:0};
        if(saved->has_value()){const RunSaveState state{progress,saved->value()};const auto bytes=EncodeRunPayload(state);TestTrue(TEXT("正式効果をv14で全状態往復"),bytes && DecodeRunPayload(*bytes)==state);}
    });
    AfterEach([this]{Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}IAutomationDriverModule::Get().Disable();});
}
