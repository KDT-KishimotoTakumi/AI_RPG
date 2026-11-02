#include "Battle.h"
#include "AbilityCatalog.h"
#include <algorithm>
#include <cstdlib>
namespace GirlsSpire {
SkillPreview Battle::PreviewCharacterSkill(int actorId,Skill skill,Position target) const
{
    const auto ability=*FormalSkill(actorId,skill);
    const auto& actor=*Find(actorId);
    const auto victim=Find(Occupant(target));
    if(ability.Target==AbilityTarget::Enemy){
        if(victim && (victim->Hp<=0 || victim->Side==actor.Side || victim->Side==Faction::Neutral))return {};
        if(!victim && state_.Tiles[target.Y*state_.Width+target.X].ObstacleHp==0)return {};
        auto projection=*this;projection.state_.ChargeReady=false;
        auto& source=*projection.Find(actorId);source.Attack+=ability.Effects.AttackBonus;
        if(ability.Range)source.Range=ability.Range;
        SkillPreview preview;preview.Attack=projection.PreviewAttack(actorId,target,0,false);preview.Legal=preview.Attack.Legal;return preview;
    }
    if(!victim || victim->Hp<=0 || victim->Side!=actor.Side ||
        (ability.Target==AbilityTarget::Self && victim->Id!=actorId) ||
        (ability.Target!=AbilityTarget::Self && std::abs(actor.At.X-target.X)+std::abs(actor.At.Y-target.Y)>ability.Range) || !HasLineOfSight(actor.At,target))return {};
    const auto& effects=ability.Effects;
    SkillPreview preview;preview.Support=true;preview.TargetId=victim->Id;
    preview.Healing=std::min(effects.Healing,victim->MaxHp-victim->Hp);
    preview.AttackBuff=effects.AttackBuff;preview.DefenseBuff=effects.DefenseBuff;preview.AccuracyBuff=effects.AccuracyBuff;
    preview.Cleanse=effects.Cleanse && (victim->AttackCursed || victim->MovementWebbed);
    preview.Legal=preview.Healing>0 || preview.AttackBuff>0 || preview.DefenseBuff>0 || preview.AccuracyBuff>0 || preview.Cleanse;
    return preview;
}
bool Battle::UseCharacterSkill(std::uint64_t revision,int actorId,Skill skill,Position target,const SkillPreview& preview)
{
    const auto ability=*FormalSkill(actorId,skill);state_.ChargeReady=false;
    auto& actor=*Find(actorId);
    if(preview.Support){
        auto& recipient=*Find(preview.TargetId);recipient.Hp+=preview.Healing;
        if(preview.AttackBuff)ApplyTimedBoost(recipient.AttackBoost,preview.AttackBuff,actorId==recipient.Id);
        if(preview.DefenseBuff)ApplyTimedBoost(recipient.DefenseBoost,preview.DefenseBuff,actorId==recipient.Id);
        if(preview.AccuracyBuff)ApplyTimedBoost(recipient.AccuracyBoost,preview.AccuracyBuff,actorId==recipient.Id);
        if(preview.Cleanse){recipient.AttackCursed=false;recipient.MovementWebbed=false;}
        if(actor.Side==Faction::Player)AddAidBond(state_.BondGains,actorId,recipient.Id,preview.Healing);
        lastHits_.clear();state_.Acted=true;++state_.Revision;RecordCombo(actorId,ability.Syntax,true);
    }else{
        const int oldRange=actor.Range;actor.Attack+=ability.Effects.AttackBonus;if(ability.Range)actor.Range=ability.Range;
        AttackWithSyntax(revision,actorId,target,0,ability.Syntax,ability.Effects.AttackPenalty>0,ability.Effects.MovementPenalty>0);
        actor.Attack-=ability.Effects.AttackBonus;actor.Range=oldRange;
    }
    actor.Cooldowns[static_cast<int>(skill)]=ability.Cooldown;state_.UsedSkill=static_cast<int>(skill);return true;
}
}
