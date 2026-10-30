#include "Battle.h"
#include <cstdlib>
namespace GirlsSpire {
bool Battle::CanTeamUltimate(int actorId) const
{
    if(!Active(actorId) || state_.Acted || state_.Combos.Gauge<100) return false;
    const auto& actor=*Find(actorId);
    if(actor.Side!=Faction::Player) return false;
    int participants=0;
    bool protagonist=false;
    for(const auto& unit:state_.Units) {
        if(unit.Side!=Faction::Player) continue;
        if(unit.Hp<=0 || std::abs(unit.At.X-actor.At.X)+std::abs(unit.At.Y-actor.At.Y)>4) return false;
        ++participants;
        protagonist=protagonist || unit.Id==0;
    }
    return participants==5 && protagonist;
}
AttackPreview Battle::PreviewTeamUltimate(int actorId,Position target) const
{
    if(!CanTeamUltimate(actorId) || !Inside(target)) return {};
    const auto victim=Find(Occupant(target));
    if(!victim || victim->Side!=Faction::Enemy) return {};
    auto projection=*this;
    projection.Find(actorId)->Attack+=40;
    return projection.PreviewAttack(actorId,target,1,false);
}
bool Battle::UseTeamUltimate(std::uint64_t revision,int actorId,Position target)
{
    if(revision!=state_.Revision || !PreviewTeamUltimate(actorId,target).Legal) return false;
    combos_.Spend(100);
    auto& actor=*Find(actorId);
    actor.Attack+=40;
    // 範囲攻撃全体を発動者1人の結として記録し、消費後に連携を解決する。
    AttackWithSyntax(revision,actorId,target,1,Ending);
    actor.Attack-=40;
    return true;
}
}
