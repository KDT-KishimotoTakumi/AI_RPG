#include "Battle.h"
#include <set>
namespace GirlsSpire {
namespace {
bool Between(int value,int low,int high) { return value>=low && value<=high; }
}
bool Battle::ValidSetup(bool initial) const
{
    const auto& s=state_;
    if(!Between(s.Width,8,12) || !Between(s.Height,8,12) ||
       s.Tiles.size()!=static_cast<std::size_t>(s.Width*s.Height) || s.Units.empty() || s.Units.size()>64 ||
       !Between(static_cast<int>(goal_),0,3) || !Between(survivalRounds_,1,100) || !Between(s.Medicines,0,5)) return false;
    if(enemyHpPercent_!=80 && enemyHpPercent_!=100 && enemyHpPercent_!=120)return false;
    for(const auto& tile:s.Tiles)
        if(!Between(tile.Elevation,0,3) || !Between(tile.Cover,0,50) || !Between(tile.ObstacleHp,0,20) ||
           (tile.Wall && tile.ObstacleHp>0) || (tile.Trap && (tile.Wall || tile.ObstacleHp>0))) return false;
    std::set<int> ids;std::set<std::pair<int,int>> occupied;std::array<int,3> factions{};
    for(const auto& unit:s.Units) {
        if(!Between(unit.Id,0,9999) || !Between(static_cast<int>(unit.Side),0,2) ||
           !Between(static_cast<int>(unit.Role),0,11) || !Between(static_cast<int>(unit.Direction),0,3) ||
           !Inside(unit.At) || !ids.insert(unit.Id).second) return false;
        const auto& tile=s.Tiles[unit.At.Y*s.Width+unit.At.X];
        if(tile.Wall || tile.ObstacleHp>0 || tile.Trap ||
           ((initial || unit.Hp>0) && !occupied.insert({unit.At.X,unit.At.Y}).second)) return false;
        const std::pair<int,int> stats[]={{unit.Hp,unit.MaxHp},{unit.Attack,1000},{unit.Defense,1000},
            {unit.Speed,1000},{unit.Accuracy,200},{unit.Evasion,200},{unit.Movement,12}};
        for(const auto& [value,max]:stats) if(!Between(value,0,max)) return false;
        if(!Between(unit.MaxHp,1,10000) || !Between(unit.Range,1,24)) return false;
        if(unit.CreatesCover && unit.Side!=Faction::Enemy)return false;
        if(unit.Role==BattleRole::Cavalry && unit.Side!=Faction::Enemy)return false;
        if(unit.Role==BattleRole::Oni && unit.Side!=Faction::Enemy)return false;
        if(unit.Role==BattleRole::CurseMonk && unit.Side!=Faction::Enemy)return false;
        if(unit.Role==BattleRole::Fox && unit.Side!=Faction::Enemy)return false;
        if(unit.Role==BattleRole::Spider && unit.Side!=Faction::Enemy)return false;
        if(unit.Role==BattleRole::FallenPrincess && unit.Side!=Faction::Enemy)return false;
        if(unit.Role==BattleRole::CastleLord && unit.Side!=Faction::Enemy)return false;
        if(unit.SummonerId!=-1) {
            const auto owner=Find(unit.SummonerId);
            if(!owner || owner->Role!=BattleRole::CastleLord || unit.Side!=Faction::Enemy || unit.Role!=BattleRole::Standard)return false;
            int count=0;for(const auto& other:s.Units)if(other.SummonerId==unit.SummonerId && other.Hp>0)++count;
            if(count>3)return false;
        }
        for(int cooldown:unit.Cooldowns) if(!Between(cooldown,0,3)) return false;
        const std::pair<TimedBoost,int> boosts[]={{unit.AttackBoost,6},{unit.DefenseBoost,4},{unit.AccuracyBoost,15}};
        for(const auto& [boost,maximum]:boosts)
            if(!ValidTimedBoost(boost) || boost.Amount>maximum || (s.AbilityRules==AbilityRuleSet::Legacy && boost.Amount!=0))return false;
        ++factions[static_cast<int>(unit.Side)];
    }
    int lords=0;for(const auto& unit:s.Units)if(unit.Role==BattleRole::CastleLord)++lords;
    if(goal_==BattleGoal::DefeatLord && lords!=1)return false;
    return factions[0]>0 && factions[1]>0 && (goal_!=BattleGoal::Rescue || factions[2]==1);
}
}
