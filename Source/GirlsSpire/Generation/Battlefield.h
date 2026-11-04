#pragma once
#include "Domain/Battle.h"
#include <functional>

namespace GirlsSpire {
struct TerrainLayout {
    int Width = 8;
    int Height = 8;
    std::vector<Tile> Tiles;
    bool operator==(const TerrainLayout&) const = default;
};
enum class FieldFailure { None, Invalid, Disconnected, Unsafe };
struct GenerationResult {
    TerrainLayout Layout;
    bool UsedFallback = false;
    int Attempts = 0;
    std::uint64_t RandomState = 0;
    bool operator==(const GenerationResult&) const = default;
};
class Battlefield {
public:
    using DraftFactory = std::function<TerrainLayout(Random&)>;
    static FieldFailure Validate(const TerrainLayout& layout, const std::vector<BattleUnit>& units, BattleGoal goal,bool enemyCharacterSkills=false);
    static GenerationResult Generate(std::uint64_t seed, const std::vector<BattleUnit>& units,
        BattleGoal goal = BattleGoal::Eliminate, int maxAttempts = 8, const DraftFactory& draft = {},bool enemyCharacterSkills=false);
};
}
