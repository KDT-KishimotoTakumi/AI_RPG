#pragma once
#include "Infrastructure/SavePayload.h"
namespace GirlsSpire::TestData {
inline RunSaveState PersonalityCase(bool enabled)
{
    Run run(42,Difficulty::Beginner,HeroRole::Sword);auto progress=run.Checkpoint();
    progress.State.Day=5;progress.State.Phase=RunPhase::Event;progress.State.Active={NodeKind::Battle};progress.State.Choices.clear();progress.State.Recruited={2};
    BattleUnit actor;actor.Id=3;actor.At={1,1};actor.Speed=30;actor.Range=3;
    BattleUnit hero;hero.Id=0;hero.At={1,2};
    BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={3,1};enemy.Hp=enemy.MaxHp=100;enemy.AttackCursed=true;
    auto second=enemy;second.Id=101;second.At={1,3};second.AttackCursed=false;
    auto checkpoint=Battle(8,8,std::vector<Tile>(64),{actor,hero,enemy,second},42).Checkpoint();
    checkpoint.State.AbilityRules=AbilityRuleSet::CompleteCharacters;checkpoint.State.StartingBonds.fill(75);checkpoint.State.PersonalityAI=enabled;
    CombatControl control;control.ProfileDriven=true;control.Trust.fill(75);control.Orders[3]={TacticalIntent::Escort,{},{}};
    return {progress,SavedCombat{checkpoint,control.Checkpoint()}};
}
}
