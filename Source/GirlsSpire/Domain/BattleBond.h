#pragma once
#include "Bond.h"
#include <vector>
namespace GirlsSpire {
struct BattleBond {
    std::array<int,8> Combo{};
    std::array<int,8> Aid{};
    bool operator==(const BattleBond&) const = default;
};
bool ValidBattleBond(const BattleBond& gains);
bool AddComboBond(BattleBond& gains,const std::vector<int>& participants);
bool AddAidBond(BattleBond& gains,int actorId,int targetId,int healing);
bool ApplyBattleBond(Profile& profile,const BattleBond& gains);
}
