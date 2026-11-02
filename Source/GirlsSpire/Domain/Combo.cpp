#include "Combo.h"
#include "Rules.h"
#include <algorithm>
#include <set>
#include <tuple>
namespace GirlsSpire {
bool ComboLedger::Spend(int amount)
{
    if(amount<=0 || amount>state_.Gauge) return false;
    state_.Gauge-=amount;
    return true;
}
void ComboLedger::AdvanceRound(int round)
{
    if(round<=state_.Round) return;
    state_.Round=round;
    state_.Pending.clear();
}
bool ComboLedger::Observe(const ComboEvent& event)
{
    if(event.Sequence<=state_.LastSequence || event.Round<state_.Round || event.ActorId<0 || event.ActorId>9999 ||
        static_cast<int>(event.Side)<0 || static_cast<int>(event.Side)>2) return false;
    std::set<int> knockedOut;
    for(int id:event.KnockedOut)
        if(id<0 || id>9999 || !knockedOut.insert(id).second) return false;
    if(event.Round!=state_.Round) state_.Pending.clear();
    state_.Round=event.Round;
    state_.LastSequence=event.Sequence;
    const bool player=event.Side==Faction::Player;
    const bool actorDown=knockedOut.contains(event.ActorId);
    const bool participantDown=std::any_of(state_.Pending.begin(),state_.Pending.end(),
        [&](const ComboAction& action) {return knockedOut.contains(action.ActorId);});
    if(participantDown || (player && (!event.Succeeded || actorDown))) state_.Pending.clear();
    if(player && event.Succeeded && !actorDown) state_.Pending.push_back({event.Sequence,event.ActorId,event.Attributes,event.EnemyTargets});
    return true;
}
std::optional<ComboResolution> ComboLedger::Resolve(std::uint64_t sequence,const std::vector<ComboCandidate>& candidates)
{
    if(sequence!=state_.LastSequence || sequence==state_.ResolvedSequence) return {};
    const ComboCandidate* best=nullptr;
    std::vector<int> participants;
    for(const auto& candidate:candidates) {
        const auto count=candidate.Actions.size();
        if(candidate.RecipeId<=0 || (count!=2 && count!=3 && count!=5)) continue;
        if(candidate.BondExclusive && (candidate.RecipeId<201 || candidate.RecipeId>208 || count!=2))continue;
        std::vector<int> actors;
        std::set<int> unique;
        std::uint64_t previous=0;
        for(const auto id:candidate.Actions) {
            const auto action=std::find_if(state_.Pending.begin(),state_.Pending.end(),
                [id](const ComboAction& entry) {return entry.Sequence==id;});
            if(id<=previous || action==state_.Pending.end() || !unique.insert(action->ActorId).second) break;
            previous=id;
            actors.push_back(action->ActorId);
        }
        if(actors.size()!=count) continue;
        if(candidate.BondExclusive && (actors[0]!=0 || actors[1]!=candidate.RecipeId-200))continue;
        // 同人数でも入力コンテナの並びに結果を依存させない。
        if(!best || count>best->Actions.size() || (count==best->Actions.size() &&
            std::tuple{!candidate.BondExclusive,candidate.RecipeId,candidate.Actions}<std::tuple{!best->BondExclusive,best->RecipeId,best->Actions})) {
            best=&candidate;
            participants=std::move(actors);
        }
    }
    if(!best) return {};
    const int nextGauge=best->BondExclusive?std::min(100,state_.Gauge+20):AddComboGauge(state_.Gauge,static_cast<int>(participants.size()));
    ComboResolution result{best->RecipeId,participants,nextGauge-state_.Gauge};
    std::erase_if(state_.Pending,[&](const ComboAction& action) {
        return std::find(best->Actions.begin(),best->Actions.end(),action.Sequence)!=best->Actions.end();
    });
    state_.Gauge=nextGauge;
    state_.ResolvedSequence=sequence;
    return result;
}
}
