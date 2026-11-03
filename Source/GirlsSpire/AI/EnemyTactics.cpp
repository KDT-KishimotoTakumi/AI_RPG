#include "EnemyTactics.h"
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <tuple>
namespace GirlsSpire {
namespace {
int Distance(Position a,Position b){return std::abs(a.X-b.X)+std::abs(a.Y-b.Y);}
const BattleUnit& ActiveUnit(const BattleSnapshot& state)
{
    return *std::find_if(state.Units.begin(),state.Units.end(),[&](const auto& unit){return unit.Id==state.ActiveId;});
}
int NextTurn(const BattleSnapshot& state,int id)
{
    const auto start=std::find(state.Order.begin(),state.Order.end(),state.ActiveId)-state.Order.begin();
    int turns=0;
    for(std::size_t step=1;step<=state.Order.size();++step){
        const int candidate=state.Order[(start+step)%state.Order.size()];
        const auto unit=std::find_if(state.Units.begin(),state.Units.end(),[&](const auto& item){return item.Id==candidate;});
        if(unit->Hp<=0)continue;
        ++turns;if(candidate==id)return turns;
    }
    return static_cast<int>(state.Order.size())+1;
}
std::array<int,4> AttackScore(const BattleSnapshot& state,const BattleUnit& actor,const BattleUnit& target,const AttackPreview& preview)
{
    const auto mode=actor.Side==Faction::Enemy?state.EnemyAI:EnemyThinking::Legacy;
    if(mode==EnemyThinking::Basic)return {0,-Distance(actor.At,target.At),0,-target.Id};
    int damage=0,knockouts=0;bool disrupt=false;
    for(const auto& hit:preview.Hits){
        const auto victim=std::find_if(state.Units.begin(),state.Units.end(),[&](const auto& item){return item.Id==hit.TargetId;});
        damage+=hit.Chance*std::min(hit.Damage,victim->Hp);
        if(hit.Damage>=victim->Hp)knockouts+=hit.Chance;
        if(victim->Side==Faction::Player)for(const auto& pending:state.Combos.Pending)
            if(pending.Sequence>state.Combos.ResolvedSequence && pending.ActorId==victim->Id)disrupt=true;
    }
    if(mode!=EnemyThinking::Legacy && disrupt)damage+=500;
    return {mode==EnemyThinking::Tactical?knockouts:0,damage,mode==EnemyThinking::Tactical?-NextTurn(state,target.Id):0,-target.Id};
}
}
std::optional<RankedAttack> ChooseRankedAttack(const Battle& battle)
{
    const auto& state=battle.Snapshot();if(state.Outcome!=BattleOutcome::Ongoing)return std::nullopt;
    const auto& actor=ActiveUnit(state);std::optional<RankedAttack> best;
    for(const auto& target:state.Units){
        if(target.Hp<=0 || target.Side==actor.Side || (actor.Side==Faction::Player && target.Side==Faction::Neutral))continue;
        auto preview=battle.PreviewAttack(actor.Id,target.At);if(!preview.Legal)continue;
        BasicAction action{BasicActionKind::Attack,target.At};
        if(actor.Side==Faction::Enemy && actor.Role==BattleRole::Oni){
            const auto skill=battle.PreviewSkill(actor.Id,Skill::PowerStrike,target.At);
            if(skill.Legal && skill.Attack.Hits.front().Damage>preview.Hits.front().Damage){preview=skill.Attack;action={BasicActionKind::UseSkill,target.At,Skill::PowerStrike};}
        }
        const auto score=AttackScore(state,actor,target,preview);
        if(!best || score>best->Score)best=RankedAttack{action,score};
    }
    return best;
}
BasicAction ChooseTacticalMove(const Battle& battle)
{
    const auto& state=battle.Snapshot();if(state.Outcome!=BattleOutcome::Ongoing)return {};
    const auto& actor=ActiveUnit(state);
    if(actor.Side!=Faction::Enemy || state.EnemyAI!=EnemyThinking::Tactical || state.Acted)return {};
    const auto nearest=[&](Position at){
        int distance=std::numeric_limits<int>::max();
        for(const auto& target:state.Units)if(target.Hp>0 && target.Side!=actor.Side)distance=std::min(distance,Distance(at,target.At));
        return distance;
    };
    BasicAction firing,approach;int shortest=nearest(actor.At);
    using MoveScore=std::tuple<std::array<int,4>,int,int,int,int>;
    std::optional<MoveScore> best;
    for(const auto destination:battle.Reachable(actor.Id)){
        const auto move=battle.PreviewMove(actor.Id,destination);if(!move.Legal || move.KnockedOut)continue;
        const int distance=nearest(destination);
        if(distance<shortest){shortest=distance;approach={BasicActionKind::Move,destination};}
        auto projection=battle;if(!projection.Move(state.Revision,actor.Id,destination))continue;
        const auto attack=ChooseRankedAttack(projection);if(!attack)continue;
        const MoveScore score{attack->Score,-(move.Damage+projection.PotentialHostileDamage(actor.Id)),-distance,-destination.Y,-destination.X};
        if(!best || score>*best){best=score;firing={BasicActionKind::Move,destination};}
    }
    return best?firing:approach;
}
}
