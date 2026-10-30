#pragma once
#include "Battle.h"
namespace GirlsSpire {
enum class Reward { MaxHp, Attack, Defense, Speed, Sweep, FarStrike, Heal };
struct GrowthProgress {
    std::array<int,4> Bonuses{};
    std::array<bool,3> Learned{};
    bool operator==(const GrowthProgress&) const = default;
};
std::vector<Reward> OfferRewards(const GrowthProgress& progress,Random& random);
bool TakeReward(GrowthProgress& progress,Reward reward);
void ApplyGrowth(BattleUnit& unit,const GrowthProgress& progress);
}
