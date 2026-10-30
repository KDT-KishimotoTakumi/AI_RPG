#include "Battle.h"
#include <algorithm>
namespace GirlsSpire {
void Battle::GeneratePeriodicCover()
{
    const auto* actor=Find(state_.ActiveId);
    if(state_.Outcome!=BattleOutcome::Ongoing || state_.Round%2!=0 || !actor ||
        actor->Side!=Faction::Enemy || !actor->CreatesCover)return;
    const Position directions[]={{0,-1},{1,0},{0,1},{-1,0}};
    for(const auto step:directions) {
        const Position at{actor->At.X+step.X,actor->At.Y+step.Y};
        if(!Inside(at))continue;
        const auto index=at.Y*state_.Width+at.X;const auto& tile=state_.Tiles[index];
        if(tile.Wall || tile.ObstacleHp>0 || tile.Trap || tile.Cover>0 ||
            std::any_of(state_.Units.begin(),state_.Units.end(),[at](const BattleUnit& unit){return unit.At==at;}))continue;
        auto trial=*this;trial.state_.Tiles[index].ObstacleHp=20;
        const auto reachable=trial.ConnectedTerrain(actor->At);
        if(std::any_of(state_.Units.begin(),state_.Units.end(),[&reachable](const BattleUnit& unit) {
            return unit.Hp>0 && std::find(reachable.begin(),reachable.end(),unit.At)==reachable.end();
        }))continue;
        // 手番移行と同じ確定結果へ含める。行動・移動・乱数は追加消費しない。
        state_.Tiles[index].ObstacleHp=20;return;
    }
}
}
