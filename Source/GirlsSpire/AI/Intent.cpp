#include "Intent.h"
#include "Domain/Rules.h"
#include "Domain/AbilityCatalog.h"
#include <algorithm>
#include <numeric>
#include <tuple>
#include <cstdlib>
namespace GirlsSpire {
std::vector<BasicAction> RankIntentActions(const Battle& battle, const IntentOrder& order)
{
    const auto& state=battle.Snapshot();
    if (state.Outcome!=BattleOutcome::Ongoing || static_cast<int>(order.Kind)<0 ||
        static_cast<int>(order.Kind)>3 || (order.TargetId && order.Point)) return {};
    const auto actor=std::find_if(state.Units.begin(),state.Units.end(),
        [&](const BattleUnit& unit) { return unit.Id==state.ActiveId; });
    if (actor->Side!=Faction::Player) return {};
    const bool offensive=order.Kind==TacticalIntent::Assault || order.Kind==TacticalIntent::Disrupt;
    const auto distance=[](Position a,Position b) { return std::abs(a.X-b.X)+std::abs(a.Y-b.Y); };
    Position focus=actor->At;
    const BattleUnit* target=nullptr;
    if (order.Point) {
        focus=*order.Point;
        if (focus.X<0 || focus.Y<0 || focus.X>=state.Width || focus.Y>=state.Height ||
            state.Tiles[focus.Y*state.Width+focus.X].Wall) return {};
    } else {
        int best=-1000000;
        for (const auto& candidate : state.Units) {
            if (candidate.Hp<=0 || candidate.Side!=(offensive?Faction::Enemy:Faction::Player)) continue;
            if (order.TargetId && candidate.Id!=*order.TargetId) continue;
            int score=-distance(actor->At,candidate.At);
            if (order.Kind==TacticalIntent::Disrupt) score+=candidate.Speed*100;
            if (order.Kind==TacticalIntent::Escort && candidate.Id==0) score+=100000;
            if (order.Kind==TacticalIntent::Support) score+=(10000-candidate.Hp*10000/candidate.MaxHp)*100;
            if (!target || score>best || (score==best && candidate.Id<target->Id)) { target=&candidate; best=score; }
        }
        if (order.TargetId && !target) return {};
        if (target) focus=target->At;
    }
    std::vector<std::pair<int,BasicAction>> scored;
    for (const auto& victim : state.Units) {
        const auto preview=battle.PreviewAttack(actor->Id,victim.At);
        if (!preview.Legal) continue;
        const auto& hit=preview.Hits.front();
        const int focusBonus=offensive && target && victim.Id==target->Id?200000:0;
        const int score=10000+focusBonus-distance(focus,victim.At)*100+hit.Chance*std::min(hit.Damage,victim.Hp);
        scored.push_back({score,{BasicActionKind::Attack,victim.At}});
    }
    const int desired=order.Point?0:offensive?actor->Range:1;
    for(const auto& victim:state.Units) for(int index=0;index<SkillCount;++index) {
        const auto skill=static_cast<Skill>(index);
        const auto preview=battle.PreviewSkill(actor->Id,skill,victim.At);
        if(!preview.Legal) continue;
        const bool healing=preview.Support || preview.Healing>0;
        if(healing!=(order.Kind==TacticalIntent::Support)) continue;
        const int focusBonus=target && victim.Id==target->Id?200000:0;
        const auto definition=state.AbilityRules==AbilityRuleSet::CharacterSkills?FormalSkill(actor->Id,skill):std::nullopt;
        const int disruptBonus=order.Kind==TacticalIntent::Disrupt && definition && definition->Syntax==Twist?100000:0;
        const int score=healing?300000+focusBonus+preview.Healing*100-distance(focus,victim.At):disruptBonus+
            10000+(offensive?focusBonus:0)-distance(focus,victim.At)*100+
            preview.Attack.Hits.front().Chance*std::min(preview.Attack.Hits.front().Damage,victim.Hp);
        scored.push_back({score,{BasicActionKind::UseSkill,victim.At,skill}});
    }
    const auto gap=[&](Position at) { return std::max(0,distance(at,focus)-desired); };
    for (const auto at : battle.Reachable(actor->Id)) {
        const int improvement=gap(actor->At)-gap(at);
        if (improvement>0) scored.push_back({improvement*100,{BasicActionKind::Move,at}});
    }
    scored.push_back({-1,{}});
    std::sort(scored.begin(),scored.end(),[](const auto& left,const auto& right) {
        if (left.first!=right.first) return left.first>right.first;
        return std::tuple{left.second.Kind,left.second.Target.Y,left.second.Target.X,left.second.Ability}<
            std::tuple{right.second.Kind,right.second.Target.Y,right.second.Target.X,right.second.Ability};
    });
    std::vector<BasicAction> result;
    for (const auto& entry : scored) result.push_back(entry.second);
    return result;
}
IntentForecast IntentForecast::Build(const Battle& battle, const std::vector<BasicAction>& ranked, int trust)
{
    IntentForecast forecast;
    const auto& state=battle.Snapshot();
    if (state.Outcome!=BattleOutcome::Ongoing) return forecast;
    const auto actor=std::find_if(state.Units.begin(),state.Units.end(),
        [&](const BattleUnit& unit) { return unit.Id==state.ActiveId; });
    if (actor->Side!=Faction::Player) return forecast;
    forecast.source_=state; forecast.sourceRandom_=battle.RandomState();
    forecast.fallback_=IntentFallback::None;
    const auto policy=GetPrediction(trust);
    for (auto action : ranked) {
        if (action.Kind==BasicActionKind::Wait) action.Target={};
        if (std::any_of(forecast.candidates_.begin(),forecast.candidates_.end(),
            [&](const IntentCandidate& candidate) { return candidate.Action==action; })) continue;
        auto projection=battle;
        const auto movement=action.Kind==BasicActionKind::Move?battle.PreviewMove(actor->Id,action.Target):MovePreview{};
        if(movement.KnockedOut) continue;
        if (!ApplyBasicAction(projection,action)) continue;
        const int damage=action.Kind==BasicActionKind::Move?
            movement.Damage+projection.PotentialEnemyDamage(actor->Id):battle.PotentialEnemyDamage(actor->Id);
        if (action.Kind==BasicActionKind::Move && damage>=actor->Hp) continue;
        forecast.candidates_.push_back({action,damage,std::nullopt});
        if (forecast.candidates_.size()==static_cast<std::size_t>(policy.CandidateLimit)) break;
    }
    if (forecast.candidates_.empty()) {
        forecast.fallback_=IntentFallback::NoSafeCandidate;
        forecast.candidates_.push_back({{},battle.PotentialEnemyDamage(actor->Id),std::nullopt});
    }
    const std::vector<int> weights=trust<50?std::vector<int>{50,30,20}:
        trust<75?std::vector<int>{70,30}:std::vector<int>{100};
    const auto count=forecast.candidates_.size();
    const int total=std::accumulate(weights.begin(),weights.begin()+count,0);
    std::vector<int> remainder;
    int allocated=0;
    for (std::size_t i=0;i<count;++i) {
        const int percent=100*weights[i]/total;
        forecast.percentages_.push_back(percent); allocated+=percent;
        remainder.push_back(100*weights[i]%total);
    }
    // 同率の端数は元の優先順。表示と抽選に同じ整数%を使う。
    while (allocated<100) {
        const auto best=std::max_element(remainder.begin(),remainder.end());
        ++forecast.percentages_[static_cast<std::size_t>(best-remainder.begin())];
        *best=-1; ++allocated;
    }
    if (policy.ShowProbability)
        for (std::size_t i=0;i<count;++i) forecast.candidates_[i].Probability=forecast.percentages_[i];
    return forecast;
}
const std::vector<IntentCandidate>& IntentForecast::Candidates() const { return candidates_; }
IntentFallback IntentForecast::Fallback() const { return fallback_; }
bool IntentForecast::Certain() const { return candidates_.size()==1; }
bool IntentForecast::Execute(Battle& battle, Random& choiceRandom) const
{
    if (candidates_.empty() || battle.Snapshot()!=source_ || battle.RandomState()!=sourceRandom_) return false;
    auto nextRandom=choiceRandom;
    int roll=Certain()?0:static_cast<int>(nextRandom.Uniform(100));
    std::size_t selected=0;
    while (roll>=percentages_[selected]) { roll-=percentages_[selected]; ++selected; }
    auto nextBattle=battle;
    if (!ApplyBasicAction(nextBattle,candidates_[selected].Action)) return false;
    battle=std::move(nextBattle); choiceRandom=nextRandom;
    return true;
}
}
