#pragma once
#include "Domain/Battle.h"
#include <stdexcept>

namespace GirlsSpire::Test {
inline Battle UltimateBattle(int seed=0,int gauge=100,BattleRole role=BattleRole::Standard,int enemyHp=10000,int range=8)
{
    BattleUnit hero;hero.Id=0;hero.At={1,1};hero.Speed=30;hero.Range=range;hero.Role=role;
    BattleUnit ally=hero;ally.Id=1;ally.At={2,1};ally.Speed=20;
    BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={6,1};
    enemy.Hp=enemy.MaxHp=enemyHp;enemy.Movement=0;enemy.Attack=0;
    std::vector<Tile> tiles(64);tiles[3*8+3].Wall=true;tiles[4*8+4].ObstacleHp=20;
    Battle battle(8,8,tiles,{hero,ally,enemy},seed);
    // ゲージを直接書き換えず、通常の連携で初期盤面を準備する。
    for(int rounds=0;battle.Snapshot().Combos.Gauge<gauge && rounds<1000;++rounds) {
        battle.EndTurn(battle.Snapshot().Revision,0,Facing::South);
        battle.Attack(battle.Snapshot().Revision,1,enemy.At);
        battle.EndTurn(battle.Snapshot().Revision,1,Facing::South);
        battle.EndTurn(battle.Snapshot().Revision,100,Facing::South);
    }
    if(battle.Snapshot().Combos.Gauge!=gauge) throw std::runtime_error("必殺テストの初期ゲージを準備できません");
    return battle;
}
}
