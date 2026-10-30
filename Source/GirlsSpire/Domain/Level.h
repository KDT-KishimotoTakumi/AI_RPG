#pragma once
#include "Battle.h"
namespace GirlsSpire {
struct LevelProgress {
    int Level=1;
    int Experience=0;
    bool operator==(const LevelProgress&) const = default;
};
bool GainExperience(LevelProgress& progress,int amount);
int LevelHpBonus(const LevelProgress& progress);
void ApplyLevel(BattleUnit& unit,const LevelProgress& progress);
}
