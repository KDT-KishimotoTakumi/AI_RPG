#pragma once
namespace GirlsSpire {
struct Position { int X=0; int Y=0; bool operator==(const Position&) const = default; };
enum class Faction { Player, Enemy, Neutral };
}
