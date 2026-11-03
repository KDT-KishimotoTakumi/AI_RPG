#pragma once
#include "Infrastructure/SavePayload.h"
namespace GirlsSpire::TestData {
inline RunSaveState EnemyThinkingCase(EnemyThinking mode,bool pending=false)
{
    const auto level=mode==EnemyThinking::Legacy?Difficulty::Beginner:static_cast<Difficulty>(static_cast<int>(mode)-1);
    Run run(42,level,HeroRole::Sword);auto progress=run.Checkpoint();
    progress.State.Day=5;progress.State.Phase=RunPhase::Event;progress.State.Active={NodeKind::Battle};progress.State.Choices.clear();progress.State.Recruited={0};
    BattleUnit hero;hero.At={3,1};hero.Hp=hero.MaxHp=100;hero.Defense=4;hero.Speed=40;if(mode==EnemyThinking::Tactical)hero.Hp=8;
    auto ally=hero;ally.Id=1;ally.At={1,4};ally.Hp=100;ally.Defense=0;ally.Speed=10;
    BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={1,1};enemy.Speed=30;enemy.Range=3;
    auto checkpoint=Battle(8,8,std::vector<Tile>(64),{enemy,hero,ally},42).Checkpoint();
    checkpoint.State.AbilityRules=AbilityRuleSet::CompleteCharacters;checkpoint.State.StartingBonds.fill(75);checkpoint.State.PersonalityAI=true;checkpoint.State.EnemyAI=mode;
    // 待機で新たな起を追加せず、保存済み履歴の解決境界だけを比較する。
    checkpoint.State.Acted=true;checkpoint.State.Revision=1;checkpoint.LastHits={{100,false,0}};
    checkpoint.State.Combos=pending?ComboSnapshot{1,0,1,0,{{1,0,Continuation,{100}}}}:ComboSnapshot{1,0,1,1,{}};
    CombatControl control;control.ProfileDriven=true;control.Trust.fill(75);
    return {progress,SavedCombat{checkpoint,control.Checkpoint()}};
}
}
