#include "EnemyCharacterSkills.h"
#include "Domain/AbilityCatalog.h"
#include <algorithm>
#include <array>
namespace GirlsSpire {
namespace {
const BattleUnit* Actor(const Battle& battle)
{
    const auto& state=battle.Snapshot();
    if(!state.EnemyCharacterSkills || state.Outcome!=BattleOutcome::Ongoing)return nullptr;
    const auto actor=std::find_if(state.Units.begin(),state.Units.end(),[&](const auto& unit){return unit.Id==state.ActiveId;});
    return actor!=state.Units.end() && actor->Side==Faction::Enemy && actor->Id>=1 && actor->Id<=8?&*actor:nullptr;
}
int EffectiveAttack(const BattleUnit& unit)
{
    return std::max(0,unit.Attack+unit.AttackBoost.Amount-(unit.AttackCursed?4:0));
}
}
bool UsesCharacterEnemyAI(const Battle& battle){return Actor(battle)!=nullptr;}
std::optional<CharacterAttack> ChooseCharacterAttack(const Battle& battle,Position target)
{
    const auto actor=Actor(battle);if(!actor || battle.Snapshot().Acted)return std::nullopt;
    const auto& units=battle.Snapshot().Units;
    std::optional<CharacterAttack> best;std::array<int,4> bestScore{};
    const auto consider=[&](BasicAction action,const AttackPreview& preview,int slot,bool normal){
        if(!preview.Legal)return;
        int expected=0;bool fresh=false;
        const auto ability=normal?std::optional<CharacterAbility>{}:FormalSkill(actor->Id,action.Ability);
        for(const auto& hit:preview.Hits){
            const auto victim=std::find_if(units.begin(),units.end(),[&](const auto& unit){return unit.Id==hit.TargetId;});
            expected+=hit.Chance*std::min(hit.Damage,victim->Hp);
            if(ability)fresh=fresh || (ability->Effects.AttackPenalty>0 && !victim->AttackCursed) || (ability->Effects.MovementPenalty>0 && !victim->MovementWebbed);
        }
        const std::array<int,4> score{expected,fresh?1:0,normal?1:0,-slot};
        if(!best || score>bestScore){best=CharacterAttack{action,preview};bestScore=score;}
    };
    consider({BasicActionKind::Attack,target},battle.PreviewAttack(actor->Id,target),0,true);
    for(int slot=0;slot<2;++slot){const auto skill=static_cast<Skill>(slot);consider({BasicActionKind::UseSkill,target,skill},battle.PreviewSkill(actor->Id,skill,target).Attack,slot,false);}
    return best;
}
std::optional<BasicAction> ChooseCharacterRecovery(const Battle& battle)
{
    const auto actor=Actor(battle);if(!actor || battle.Snapshot().Acted)return std::nullopt;
    const BattleUnit* recipient=nullptr;std::optional<BasicAction> best;
    for(const auto& target:battle.Snapshot().Units)for(int slot=0;slot<2;++slot){
        if(target.Hp<=0 || target.Side!=actor->Side)continue;
        const auto skill=static_cast<Skill>(slot);const auto preview=battle.PreviewSkill(actor->Id,skill,target.At);
        if(!preview.Legal || (preview.Healing==0 && !preview.Cleanse) || (target.Hp*2>target.MaxHp && !preview.Cleanse))continue;
        if(!recipient || target.Hp*recipient->MaxHp<recipient->Hp*target.MaxHp ||
            (target.Hp*recipient->MaxHp==recipient->Hp*target.MaxHp && target.Id<recipient->Id)){
            recipient=&target;best=BasicAction{BasicActionKind::UseSkill,target.At,skill};
        }
    }
    return best;
}
std::optional<BasicAction> ChooseCharacterPreparation(const Battle& battle)
{
    const auto actor=Actor(battle);if(!actor || battle.Snapshot().Acted)return std::nullopt;
    const BattleUnit* recipient=nullptr;std::optional<BasicAction> best;
    for(const auto& target:battle.Snapshot().Units)for(int slot=0;slot<2;++slot){
        if(target.Hp<=0 || target.Side!=actor->Side)continue;
        const auto skill=static_cast<Skill>(slot);const auto preview=battle.PreviewSkill(actor->Id,skill,target.At);
        if(!preview.Legal || !(preview.AttackBuff>target.AttackBoost.Amount || preview.DefenseBuff>target.DefenseBoost.Amount || preview.AccuracyBuff>target.AccuracyBoost.Amount))continue;
        if(!recipient || EffectiveAttack(target)>EffectiveAttack(*recipient) ||
            (EffectiveAttack(target)==EffectiveAttack(*recipient) && target.Id<recipient->Id)){
            recipient=&target;best=BasicAction{BasicActionKind::UseSkill,target.At,skill};
        }
    }
    return best;
}
}
