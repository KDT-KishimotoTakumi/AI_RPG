#include "Battle.h"
namespace GirlsSpire {
AttackPreview Battle::PreviewUltimate(int actorId,Position target) const
{
    if(!Active(actorId) || state_.Acted || state_.Combos.Gauge<50 || !Inside(target)) return {};
    if(Find(actorId)->Side!=Faction::Player) return {};
    if(UsesCharacterAbilities(state_.AbilityRules))return PreviewCharacterUltimate(actorId,target);
    const auto victim=Find(Occupant(target));
    if(!victim || victim->Side!=Faction::Enemy) return {};
    auto projection=*this;
    projection.Find(actorId)->Attack+=20;
    return projection.PreviewAttack(actorId,target,0,false);
}
bool Battle::UseUltimate(std::uint64_t revision,int actorId,Position target)
{
    if(revision!=state_.Revision)return false;
    const auto preview=PreviewUltimate(actorId,target);if(!preview.Legal)return false;
    if(UsesCharacterAbilities(state_.AbilityRules))return UseCharacterUltimate(actorId,preview);
    // 先に消費し、攻撃結果の「結」で成立した連携だけを後から加算する。
    combos_.Spend(50);
    auto& actor=*Find(actorId);
    actor.Attack+=20;
    AttackWithSyntax(revision,actorId,target,0,Ending);
    actor.Attack-=20;
    return true;
}
}
