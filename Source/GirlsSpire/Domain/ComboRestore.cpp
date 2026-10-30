#include "Combo.h"
namespace GirlsSpire {
bool ComboLedger::Restore(const ComboSnapshot& snapshot)
{
    if(snapshot.Round<1 || snapshot.Gauge<0 || snapshot.Gauge>100 ||
        snapshot.ResolvedSequence>snapshot.LastSequence) return false;
    std::uint64_t previous=0;
    for(const auto& action:snapshot.Pending) {
        if(action.Sequence<=previous || action.Sequence>snapshot.LastSequence ||
            action.ActorId<0 || action.ActorId>9999) return false;
        previous=action.Sequence;
    }
    // 選択外の古い行動も有効。復元では履歴消費・ゲージ再加算を行わない。
    state_=snapshot;
    return true;
}
}
