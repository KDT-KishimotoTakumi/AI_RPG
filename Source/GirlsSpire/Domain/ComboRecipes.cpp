#include "Battle.h"
#include "ComboCatalog.h"
#include <algorithm>
#include <functional>
namespace GirlsSpire {
std::vector<ComboCandidate> FindComboCandidates(const ComboSnapshot& history,const std::vector<BattleUnit>& units)
{
    const auto& actions=history.Pending;
    if(actions.empty() || actions.back().Sequence!=history.LastSequence || !(actions.back().Attributes&Ending) || actions.back().EnemyTargets.empty())return {};
    const auto alivePlayer=[&](int id){return std::any_of(units.begin(),units.end(),[id](const BattleUnit& u){return u.Id==id && u.Side==Faction::Player && u.Hp>0;});};
    const auto& last=actions.back();if(!alivePlayer(last.ActorId))return {};
    std::vector<ComboCandidate> result;
    for(const auto& recipe:ComboCatalog()) {
        std::vector<const ComboAction*> selected;
        std::function<bool(std::size_t)> search=[&](std::size_t start) {
            if(selected.size()+1==recipe.Pattern.size()) {
                selected.push_back(&last);
                if(MatchesComboCondition(recipe.Id,selected,units))return true;
                selected.pop_back();return false;
            }
            for(std::size_t i=start;i+1<actions.size();++i) {
                const auto& action=actions[i];
                if(!(action.Attributes&recipe.Pattern[selected.size()]) || action.ActorId==last.ActorId || !alivePlayer(action.ActorId) || std::any_of(selected.begin(),selected.end(),[&](const ComboAction* other){return other->ActorId==action.ActorId;}))continue;
                selected.push_back(&action);if(search(i+1))return true;selected.pop_back();
            }
            return false;
        };
        // 条件まで満たす辞書順最初の行動列を採用する。
        if(search(0)) {
            ComboCandidate candidate{recipe.Id,{}};
            for(const auto* action:selected)candidate.Actions.push_back(action->Sequence);
            result.push_back(std::move(candidate));
        }
    }
    return result;
}
}
