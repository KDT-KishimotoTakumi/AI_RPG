#pragma once
#include "Domain/Battle.h"
#include <optional>
#include <string>
namespace GirlsSpire {
struct BoardCue {
    std::optional<Faction> Side;
    bool Move=false,Attack=false,Trap=false;
    int RouteTrapDamage=0,EnemyDamage=0;
    std::string Symbols() const;
};
std::vector<BoardCue> ReadBoardCues(const Battle& battle,bool threats=false);
}
