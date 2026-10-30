#pragma once
#include "AI/Intent.h"
#include <array>

namespace GirlsSpire {
struct CombatControlCheckpoint {
    std::array<IntentOrder,9> Orders{};
    std::array<int,9> Trust{};
    std::uint64_t ChoiceRandomState=0;
    bool ProfileDriven=false;
    bool operator==(const CombatControlCheckpoint&) const = default;
};

// 戦闘開始時の信頼と仲間抽選は、戦闘本体の乱数・現在の恒久絆から分離する。
struct CombatControl {
    std::array<IntentOrder,9> Orders{};
    std::array<int,9> Trust{};
    Random ChoiceRandom{0};
    bool ProfileDriven=false;
    CombatControlCheckpoint Checkpoint() const;
    static std::optional<CombatControl> Restore(const CombatControlCheckpoint& saved, const Battle& battle);
};
}
