#pragma once
#include "Domain/Battle.h"
#include <stdexcept>
namespace GirlsSpire::Test {
inline Battle TeamUltimateBattle(int seed=1,int gauge=100,int variant=0,BattleRole role=BattleRole::Standard)
{
    std::vector<BattleUnit> units;
    const Position positions[]={{1,1},{2,1},{1,2},{2,2},{2,4}};
    for(int id=0;id<5;++id) {
        BattleUnit unit;unit.Id=id;unit.At=positions[id];unit.Speed=30-id;unit.Range=8;
        unit.Role=role;
        if(id==0 && variant==5) unit.Range=4;
        if(id==4 && variant==1) unit.At={1,6};
        if(id==4 && variant==2) unit.Hp=0;
        if(id==0 && variant==3) unit.Id=8;
        if(id==4 && variant==4) continue;
        units.push_back(unit);
    }
    for(int id=100;id<103;++id) {
        BattleUnit unit;unit.Id=id;unit.Side=Faction::Enemy;unit.At={6,id-99};
        unit.Hp=unit.MaxHp=10000;unit.Attack=0;unit.Movement=0;units.push_back(unit);
    }
    BattleUnit neutral;neutral.Id=200;neutral.Side=Faction::Neutral;neutral.At={5,1};units.push_back(neutral);
    std::vector<Tile> tiles(64);tiles[7].ObstacleHp=20;tiles[3*8+3].Wall=true;
    Battle battle(8,8,tiles,units,seed);
    for(int round=0;battle.Snapshot().Combos.Gauge<gauge && round<1000;++round) {
        const auto currentRound=battle.Snapshot().Round;
        while(battle.Snapshot().Round==currentRound) {
            const auto id=battle.Snapshot().ActiveId;
            if(id==1) battle.Attack(battle.Snapshot().Revision,id,{6,1});
            battle.EndTurn(battle.Snapshot().Revision,id,Facing::South);
        }
    }
    if(battle.Snapshot().Combos.Gauge!=gauge) throw std::runtime_error("全員必殺ゲージの準備失敗");
    return battle;
}
}
