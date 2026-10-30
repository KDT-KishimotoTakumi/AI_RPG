#pragma once
#include "BasicTurn.h"
#include <optional>

namespace GirlsSpire {
enum class TacticalIntent { Assault, Escort, Support, Disrupt };
struct IntentOrder {
    TacticalIntent Kind = TacticalIntent::Assault;
    std::optional<int> TargetId;
    std::optional<Position> Point;
    bool operator==(const IntentOrder&) const = default;
};
std::vector<BasicAction> RankIntentActions(const Battle& battle, const IntentOrder& order);
enum class IntentFallback { None, NoSafeCandidate, Unavailable };
struct IntentCandidate {
    BasicAction Action;
    int PotentialDamage = 0;
    std::optional<int> Probability;
    bool operator==(const IntentCandidate&) const = default;
};
class IntentForecast {
public:
    static IntentForecast Build(const Battle& battle, const std::vector<BasicAction>& ranked, int trust);
    const std::vector<IntentCandidate>& Candidates() const;
    IntentFallback Fallback() const;
    bool Certain() const;
    bool Execute(Battle& battle, Random& choiceRandom) const;
private:
    BattleSnapshot source_;
    std::uint64_t sourceRandom_ = 0;
    std::vector<IntentCandidate> candidates_;
    std::vector<int> percentages_;
    IntentFallback fallback_ = IntentFallback::Unavailable;
};
}
