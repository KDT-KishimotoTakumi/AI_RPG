#include "Battle.h"
#include <algorithm>
#include <cstdlib>
namespace GirlsSpire {
SkillPreview Battle::PreviewSkill(int actorId,Skill skill,Position target) const
{
    const int index=static_cast<int>(skill);
    if(index<0 || index>=SkillCount || !Active(actorId) || state_.Acted || !Inside(target)) return {};
    const auto& actor=*Find(actorId);
    if(!actor.Skills[index] || actor.Cooldowns[index]>0) return {};
    SkillPreview preview;
    if(skill==Skill::PowerStrike || skill==Skill::Sweep || skill==Skill::FarStrike) {
        if(skill!=Skill::PowerStrike) {
            const auto victim=Find(Occupant(target));
            if(!victim || victim->Side==actor.Side || victim->Side==Faction::Neutral) return {};
        }
        // 通常攻撃と同じ射線・命中・防御判定をコピー上で利用する。
        auto projection=*this;
        projection.state_.ChargeReady=false;
        projection.Find(actorId)->Attack+=skill==Skill::PowerStrike?8:0;
        projection.Find(actorId)->Range+=skill==Skill::FarStrike?2:0;
        preview.Attack=projection.PreviewAttack(actorId,target,skill==Skill::Sweep?1:0,false);
        preview.Legal=preview.Attack.Legal;
        return preview;
    }
    const auto victim=Find(Occupant(target));
    if(!victim || victim->Hp<=0 || victim->Side!=actor.Side || victim->Hp>=victim->MaxHp ||
        std::abs(actor.At.X-target.X)+std::abs(actor.At.Y-target.Y)>3 || !HasLineOfSight(actor.At,target)) return {};
    preview.Legal=true;
    preview.TargetId=victim->Id;
    preview.Healing=std::min(skill==Skill::Heal?40:20,victim->MaxHp-victim->Hp);
    return preview;
}
bool Battle::UseSkill(std::uint64_t revision,int actorId,Skill skill,Position target)
{
    if(revision!=state_.Revision) return false;
    const auto preview=PreviewSkill(actorId,skill,target);
    if(!preview.Legal) return false;
    state_.ChargeReady=false;
    auto& actor=*Find(actorId);
    if(preview.Healing==0) {
        const int bonus=skill==Skill::PowerStrike?8:0;
        const int range=skill==Skill::FarStrike?2:0;
        actor.Attack+=bonus;actor.Range+=range;
        AttackWithSyntax(revision,actorId,target,skill==Skill::Sweep?1:0,
            skill==Skill::FarStrike?Continuation|Ending:Twist|Ending);
        actor.Attack-=bonus;actor.Range-=range;
    } else {
        Find(preview.TargetId)->Hp+=preview.Healing;
        if(actor.Side==Faction::Player) AddAidBond(state_.BondGains,actorId,preview.TargetId,preview.Healing);
        lastHits_.clear();
        state_.Acted=true;
        ++state_.Revision;
        RecordCombo(actorId,Continuation|Twist,true);
    }
    const int index=static_cast<int>(skill);
    actor.Cooldowns[index]=(skill==Skill::Sweep || skill==Skill::Heal)?3:2;
    state_.UsedSkill=index;
    return true;
}
}
