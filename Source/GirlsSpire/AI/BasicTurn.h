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
// 直線距離が縮む移動先がない（壁や障害物の裏）ときの接近。歩数が縮む移動先を選び、なければ待機。
BasicAction ChooseRouteApproach(const Battle& battle);
BasicAction ChooseBasicAction(const Battle& battle);
bool ApplyBasicAction(Battle& battle, const BasicAction& action);
}
