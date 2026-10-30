#include "Battle.h"
#include <algorithm>
#include <cstdlib>
#include <functional>
namespace GirlsSpire {
std::vector<ComboCandidate> FindComboCandidates(const ComboSnapshot& history,const std::vector<BattleUnit>& units)
{
    const auto& actions=history.Pending;
    if(actions.empty() || actions.back().Sequence!=history.LastSequence ||
        !(actions.back().Attributes&Ending) || actions.back().EnemyTargets.empty()) return {};
    const auto findUnit=[&](int id)->const BattleUnit* {
        const auto unit=std::find_if(units.begin(),units.end(),[id](const BattleUnit& u){return u.Id==id && u.Side==Faction::Player && u.Hp>0;});
        return unit==units.end()?nullptr:&*unit;
    };
    const auto& last=actions.back();
    const auto finalUnit=findUnit(last.ActorId);
    if(!finalUnit) return {};
    std::vector<ComboCandidate> result;
    const std::vector<std::vector<int>> patterns={{Opening,Ending},{Opening,Twist,Ending},{Opening,Continuation,Continuation,Twist,Ending}};
    for(const auto& pattern:patterns) {
        const auto count=pattern.size();
        std::vector<const ComboAction*> selected;
        std::function<bool(std::size_t)> search=[&](std::size_t start) {
            if(selected.size()==count-1) return true;
            for(std::size_t index=start;index+1<actions.size();++index) {
                const auto& action=actions[index];
                if(!(action.Attributes&pattern[selected.size()]) || action.ActorId==last.ActorId ||
                    std::any_of(selected.begin(),selected.end(),[&](const ComboAction* a){return a->ActorId==action.ActorId;})) continue;
                const auto unit=findUnit(action.ActorId);
                if(!unit) continue;
                const int distance=std::abs(unit->At.X-finalUnit->At.X)+std::abs(unit->At.Y-finalUnit->At.Y);
                if((count==2 && distance!=1) || (count==5 && distance>4)) continue;
                if(count==3 && selected.size()==1 && std::none_of(action.EnemyTargets.begin(),action.EnemyTargets.end(),
                    [&](int id){return std::find(last.EnemyTargets.begin(),last.EnemyTargets.end(),id)!=last.EnemyTargets.end();})) continue;
                selected.push_back(&action);
                if(search(index+1)) return true;
                selected.pop_back();
            }
            return false;
        };
        // 時系列順に探索し、各レシピの辞書順で最初の候補だけを返す。
        if(search(0)) {
            ComboCandidate candidate{static_cast<int>(count),{}};
            for(const auto* action:selected) candidate.Actions.push_back(action->Sequence);
            candidate.Actions.push_back(last.Sequence);
            result.push_back(std::move(candidate));
        }
    }
    return result;
}
}
