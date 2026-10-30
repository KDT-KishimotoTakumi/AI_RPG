#pragma once
#include "Domain/Battle.h"

namespace GirlsSpire {
enum class BasicActionKind { Wait, Move, Attack, UseSkill };
struct BasicAction {
    BasicActionKind Kind = BasicActionKind::Wait;
    Position Target;
    Skill Ability=Skill::PowerStrike;
    bool operator==(const BasicAction&) const = default;
};
BasicAction ChooseBasicAction(const Battle& battle);
bool ApplyBasicAction(Battle& battle, const BasicAction& action);
}
