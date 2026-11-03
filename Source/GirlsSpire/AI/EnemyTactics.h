#pragma once
#include "BasicTurn.h"
#include <array>
#include <optional>
namespace GirlsSpire {
struct RankedAttack {
    BasicAction Action;
    std::array<int,4> Score{};
};
std::optional<RankedAttack> ChooseRankedAttack(const Battle& battle);
BasicAction ChooseTacticalMove(const Battle& battle);
}
