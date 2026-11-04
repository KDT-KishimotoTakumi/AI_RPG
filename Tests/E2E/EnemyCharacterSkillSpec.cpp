#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/CombatScreen.h"
#include "Domain/CharacterCatalog.h"
#include "Domain/AbilityCatalog.h"
#include "AI/BasicTurn.h"
#include "Infrastructure/SavePayload.h"
#include <algorithm>
BEGIN_DEFINE_SPEC(FGirlsSpireEnemyCharacterSkillSpec,"GirlsSpire.UI.EnemyCharacterSkills",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;TSharedPtr<SWindow> Window;
END_DEFINE_SPEC(FGirlsSpireEnemyCharacterSkillSpec)
void FGirlsSpireEnemyCharacterSkillSpec::Define()
{
    BeforeEach([this]{FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();Window=SNew(SWindow).ClientSize(FVector2D(1280,900));FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();});
    LatentBeforeEach(WaitForSlateLayout);
    for(int id=1;id<=8;++id)for(int slot=0;slot<2;++slot)It(*FString::Printf(TEXT("敵人物%dが正式技能%dを使用し保存する"),id,slot),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this,id,slot]{
        using namespace GirlsSpire;const bool healing=id==2 || id==8;const bool preparing=slot==1 && (healing || id==5 || id==7);
        BattleUnit hero;hero.At=preparing?Position{7,7}:Position{2,1};hero.Speed=50;hero.Hp=hero.MaxHp=100;
        auto actor=CreateCharacter(id,{1,1});actor.Side=Faction::Enemy;actor.Speed=40;actor.Hp=actor.MaxHp=100;
        BattleUnit ally;ally.Id=100;ally.Side=Faction::Enemy;ally.At={1,2};ally.Speed=10;ally.Hp=slot==0 && healing?40:100;ally.MaxHp=100;ally.Attack=preparing?30:0;ally.Movement=0;
        Battle arena(8,8,std::vector<Tile>(64),{hero,actor,ally},42);auto checkpoint=arena.Checkpoint();
        checkpoint.State.AbilityRules=AbilityRuleSet::CompleteCharacters;checkpoint.State.EnemyAI=EnemyThinking::Basic;checkpoint.State.EnemyCharacterSkills=true;
        if(slot==1)checkpoint.State.Units[1].Cooldowns[0]=1;
        const auto restored=Battle::Restore(checkpoint);if(!TestTrue(TEXT("新しい敵技能AIの戦闘を復元"),restored.has_value()))return;
        auto combat=MakeShared<Battle>(*restored);auto expected=*combat;CombatControlCheckpoint control;control.ProfileDriven=true;control.ChoiceRandomState=42;
        auto saved=std::make_shared<std::vector<SavedCombat>>();Preferences prefs;prefs.ReducedEffects=true;
        Async(EAsyncExecution::TaskGraphMainThread,[this,combat,control,saved,prefs]{Window->SetContent(SNew(SCombatScreen).Combat(combat).FullField(true).SavedControl(control).DisplayPreferences(prefs).OnCheckpoint([saved](const SavedCombat& value){saved->push_back(value);return true;}));}).Wait();DrainSlateLayout();
        const auto wait=Driver->FindElement(By::Id("battle.wait.north"));if(!wait->IsVisible()){Driver->FindElement(By::Id("battle.scroll"))->ScrollToEndUntil(By::Id("battle.wait.north"));DrainSlateLayout();}
        if(!TestTrue(TEXT("マウスで主人公の手番を終える"),ClickAndDrain(wait)))return;
        expected.EndTurn(expected.Snapshot().Revision,0,Facing::North);
        for(int step=0;step<16 && expected.Snapshot().Outcome==BattleOutcome::Ongoing && expected.Snapshot().ActiveId!=0;++step)if(!ApplyBasicAction(expected,ChooseBasicAction(expected)))break;
        TestTrue(TEXT("敵の技能/退避/手番/状態/RNGが共通ルールと一致"),combat->Checkpoint()==expected.Checkpoint());
        const auto used=std::find_if(saved->begin(),saved->end(),[id,slot](const auto& value){return value.Battle.State.ActiveId==id && value.Battle.State.Acted && value.Battle.State.UsedSkill==slot;});
        if(!TestTrue(TEXT("指定した正式技能を実際の敵行動で使用"),used!=saved->end()))return;
        TestTrue(TEXT("技能直後の識別とCDを保存"),used->Battle.State.EnemyCharacterSkills && used->Battle.State.Units[1].Cooldowns[slot]>0);
        Run run(42,Difficulty::Beginner,HeroRole::Sword);auto progress=run.Checkpoint();progress.State.Day=5;progress.State.Phase=RunPhase::Event;progress.State.Active={NodeKind::Battle};progress.State.Choices.clear();
        const RunSaveState state{progress,*used};const auto bytes=EncodeRunPayload(state);
        TestTrue(TEXT("技能直後をRun v19で全状態往復"),bytes && DecodeRunPayload(*bytes)==state);
    });
    AfterEach([this]{Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}IAutomationDriverModule::Get().Disable();});
}
