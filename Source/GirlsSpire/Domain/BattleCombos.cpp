#include "Battle.h"
#include "ComboCatalog.h"
namespace GirlsSpire {
void Battle::RecordCombo(int actorId,int attributes,bool succeeded,const std::vector<int>& enemies,const std::vector<int>& knockedOut)
{
    const auto& actor=*Find(actorId);
    combos_.Observe({state_.Revision,state_.Round,actorId,actor.Side,succeeded,knockedOut,attributes,enemies});
    if(actor.Side==Faction::Player && succeeded && actor.Hp>0) {
        auto candidates=FindComboCandidates(combos_.Snapshot(),state_.Units);
        if(state_.AbilityRules==AbilityRuleSet::CompleteCharacters){
            const auto bonds=FindBondComboCandidates(combos_.Snapshot(),state_.Units,state_.StartingBonds);
            candidates.insert(candidates.end(),bonds.begin(),bonds.end());
        }
        const auto resolution=combos_.Resolve(state_.Revision,candidates);
        if(resolution) {
            state_.LastCombo=resolution;
            AddComboBond(state_.BondGains,resolution->Participants);
        }
    }
    state_.Combos=combos_.Snapshot();
}
}
