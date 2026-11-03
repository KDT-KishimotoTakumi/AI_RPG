#include "Battle.h"
#include <algorithm>
#include <limits>
#include <set>
namespace GirlsSpire {
Battle::Battle(const BattleCheckpoint& c)
    : state_(c.State),random_(c.RandomState),goal_(c.Goal),survivalRounds_(c.SurvivalRounds),enemyHpPercent_(c.EnemyHpPercent),turnIndex_(c.TurnIndex),lastHits_(c.LastHits) {}
BattleCheckpoint Battle::Checkpoint() const { return {state_,random_.State(),goal_,survivalRounds_,turnIndex_,lastHits_,enemyHpPercent_}; }
bool Battle::ValidCheckpoint() const
{
    const auto& s=state_;
    if(s.AbilityRules<AbilityRuleSet::Legacy || s.AbilityRules>AbilityRuleSet::CompleteCharacters)return false;
    if(s.PersonalityAI && s.AbilityRules!=AbilityRuleSet::CompleteCharacters)return false;
    if(s.EnemyAI<EnemyThinking::Legacy || s.EnemyAI>EnemyThinking::Tactical ||
       (s.EnemyAI!=EnemyThinking::Legacy && s.AbilityRules!=AbilityRuleSet::CompleteCharacters))return false;
    for(int rank:s.StartingBonds)
        if(rank<0 || rank>100 || (s.AbilityRules==AbilityRuleSet::Legacy && rank!=0))return false;
    if(s.ChargeReady) {
        const auto actor=Find(s.ActiveId);
        if(!s.Moved || s.Acted || !actor || actor->Role!=BattleRole::Cavalry || actor->Hp<=0)return false;
    }
    if(!ValidSetup(false) || s.Round<1 || s.Round==std::numeric_limits<int>::max() ||
       s.UsedSkill<-1 || s.UsedSkill>=SkillCount || !ValidBattleBond(s.BondGains) ||
       s.Combos.Round!=s.Round || s.Combos.LastSequence>s.Revision) return false;
    std::set<int> order;
    for(int id:s.Order) {
        const auto unit=Find(id);
        if(!unit || unit->Side==Faction::Neutral || !order.insert(id).second) return false;
    }
    for(const auto& unit:s.Units)
        if(unit.Hp>0 && unit.Side!=Faction::Neutral && !order.contains(unit.Id)) return false;
    if(!std::is_sorted(s.Order.begin(),s.Order.end(),[&](int a,int b) {
        const auto& left=*Find(a);const auto& right=*Find(b);
        if(left.Speed!=right.Speed) return left.Speed>right.Speed;
        if(left.Side!=right.Side) return left.Side==Faction::Player;
        return a<b;
    })) return false;
    if(s.Order.empty()?turnIndex_!=0:turnIndex_>=s.Order.size()) return false;
    if(s.Outcome==BattleOutcome::Ongoing) {
        if(s.Order.empty() || s.ActiveId!=s.Order[turnIndex_] || Find(s.ActiveId)->Hp<=0) return false;
    } else if(s.ActiveId!=-1) return false;
    auto expected=*this;expected.state_.Outcome=BattleOutcome::Ongoing;expected.Evaluate();
    if(expected.state_.Outcome!=s.Outcome) return false;
    if(s.UsedSkill>=0 && (!s.Acted || s.Order.empty() || Find(s.Order[turnIndex_])->Cooldowns[s.UsedSkill]==0)) return false;
    for(const auto& action:s.Combos.Pending) {
        const auto unit=Find(action.ActorId);
        if(!unit || unit->Side!=Faction::Player || action.Attributes<0 || action.Attributes>15) return false;
        for(int id:action.EnemyTargets) {const auto enemy=Find(id);if(!enemy || enemy->Side!=Faction::Enemy) return false;}
    }
    for(const auto& hit:lastHits_)
        if(!Find(hit.TargetId) || hit.Damage<0 || (!hit.Hit && hit.Damage!=0)) return false;
    if(s.LastCombo) {
        const auto& combo=*s.LastCombo;const auto size=combo.Participants.size();std::set<int> participants;
        if(combo.RecipeId<=0 || (size!=2 && size!=3 && size!=5) || combo.GaugeGain<0 || combo.GaugeGain>40) return false;
        for(int id:combo.Participants) {const auto unit=Find(id);if(!unit || unit->Side!=Faction::Player || !participants.insert(id).second) return false;}
    }
    return true;
}
std::optional<Battle> Battle::Restore(const BattleCheckpoint& checkpoint)
{
    Battle result(checkpoint);
    if(!result.combos_.Restore(checkpoint.State.Combos) || !result.ValidCheckpoint()) return std::nullopt;
    return result;
}
}
