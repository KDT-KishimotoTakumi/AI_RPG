#include "Battle.h"
#include "EnemyCatalog.h"
#include <algorithm>
namespace GirlsSpire {
void Battle::GeneratePeriodicSummons()
{
    const auto* actor=Find(state_.ActiveId);
    if(state_.Outcome!=BattleOutcome::Ongoing || state_.Round%3!=0 || !actor ||
        actor->Role!=BattleRole::CastleLord)return;
    const int ownerId=actor->Id;
    const auto count=std::count_if(state_.Units.begin(),state_.Units.end(),[ownerId](const BattleUnit& unit) {
        return unit.SummonerId==ownerId && unit.Hp>0;
    });
    if(count>=3)return;
    const Position directions[]={{0,-1},{1,0},{0,1},{-1,0}};
    for(const auto step:directions) {
        const Position at{actor->At.X+step.X,actor->At.Y+step.Y};
        if(!Inside(at))continue;
        const auto& tile=state_.Tiles[at.Y*state_.Width+at.X];
        if(tile.Wall || tile.ObstacleHp>0 || tile.Trap || Occupant(at)!=-1)continue;
        auto reusable=std::find_if(state_.Units.begin(),state_.Units.end(),[ownerId](const BattleUnit& unit) {
            return unit.SummonerId==ownerId && unit.Hp==0;
        });
        if(reusable==state_.Units.end() && state_.Units.size()>=64)return;
        auto soldier=CreateEnemy(EnemyKind::Footman,1000,at);soldier.SummonerId=ownerId;
        soldier.Hp=soldier.MaxHp=soldier.MaxHp*enemyHpPercent_/100;
        if(reusable!=state_.Units.end()) {
            soldier.Id=reusable->Id;*reusable=soldier;
        } else {
            soldier.Id=1000;while(Find(soldier.Id))++soldier.Id;
            state_.Units.push_back(soldier);
        }
        // 並び直した後も城主の手番を維持し、召喚だけで追加手番や乱数消費を作らない。
        RebuildOrder();
        turnIndex_=static_cast<std::size_t>(std::find(state_.Order.begin(),state_.Order.end(),ownerId)-state_.Order.begin());
        state_.ActiveId=ownerId;
        return;
    }
}
}
