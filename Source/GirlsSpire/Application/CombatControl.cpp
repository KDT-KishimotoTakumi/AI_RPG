#include "CombatControl.h"
#include <algorithm>

namespace GirlsSpire {
CombatControlCheckpoint CombatControl::Checkpoint() const
{
    return {Orders,Trust,ChoiceRandom.State(),ProfileDriven};
}

std::optional<CombatControl> CombatControl::Restore(const CombatControlCheckpoint& saved, const Battle& battle)
{
    const auto& state=battle.Snapshot();
    for(std::size_t id=0;id<saved.Orders.size();++id) {
        const auto& order=saved.Orders[id];
        if(saved.Trust[id]<0 || saved.Trust[id]>100 ||
            order.Kind<TacticalIntent::Assault || order.Kind>TacticalIntent::Disrupt ||
            (order.TargetId && order.Point)) return std::nullopt;
        if(order.TargetId && std::none_of(state.Units.begin(),state.Units.end(),
            [&](const BattleUnit& unit) {return unit.Id==*order.TargetId;})) return std::nullopt;
        if(order.Point && (order.Point->X<0 || order.Point->Y<0 ||
            order.Point->X>=state.Width || order.Point->Y>=state.Height)) return std::nullopt;
    }
    return CombatControl{saved.Orders,saved.Trust,Random(saved.ChoiceRandomState),saved.ProfileDriven};
}
}
