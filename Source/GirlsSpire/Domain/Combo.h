#pragma once
#include "CombatTypes.h"
#include <cstdint>
#include <vector>
#include <optional>

namespace GirlsSpire {
enum ComboTag { Opening=1, Continuation=2, Twist=4, Ending=8 };
struct ComboEvent {
    std::uint64_t Sequence=0;
    int Round=1;
    int ActorId=0;
    Faction Side=Faction::Player;
    bool Succeeded=true;
    std::vector<int> KnockedOut{};
    int Attributes=0;
    std::vector<int> EnemyTargets{};
};
struct ComboAction {
    std::uint64_t Sequence;
    int ActorId;
    int Attributes=0;
    std::vector<int> EnemyTargets{};
    bool operator==(const ComboAction&) const = default;
};
struct ComboCandidate {
    int RecipeId=0;
    std::vector<std::uint64_t> Actions;
    bool BondExclusive=false;
};
struct ComboResolution {
    int RecipeId=0;
    std::vector<int> Participants;
    int GaugeGain=0;
    bool operator==(const ComboResolution&) const = default;
};
struct ComboSnapshot {
    int Round=1;
    int Gauge=0;
    std::uint64_t LastSequence=0;
    std::uint64_t ResolvedSequence=0;
    std::vector<ComboAction> Pending;
    bool operator==(const ComboSnapshot&) const = default;
};
class ComboLedger {
public:
    const ComboSnapshot& Snapshot() const { return state_; }
    bool Restore(const ComboSnapshot& snapshot);
    bool Observe(const ComboEvent& event);
    void AdvanceRound(int round);
    bool Spend(int amount);
    std::optional<ComboResolution> Resolve(std::uint64_t sequence,const std::vector<ComboCandidate>& candidates);
private:
    ComboSnapshot state_;
};
struct BattleUnit;
std::vector<ComboCandidate> FindComboCandidates(const ComboSnapshot& history,const std::vector<BattleUnit>& units);
}
