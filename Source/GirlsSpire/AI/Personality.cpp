#include "Personality.h"
#include "Domain/AbilityCatalog.h"
#include <algorithm>
#include <cstdlib>
namespace GirlsSpire {
int PersonalityBonus(const Battle& battle,const BasicAction& action)
{
    const auto& state=battle.Snapshot();
    if(!UsesCharacterSkills(state.AbilityRules) || state.Outcome!=BattleOutcome::Ongoing)return 0;
    const auto actor=std::find_if(state.Units.begin(),state.Units.end(),[&](const auto& unit){return unit.Id==state.ActiveId;});
    if(actor->Side!=Faction::Player || actor->Id<1 || actor->Id>8)return 0;
    const auto target=std::find_if(state.Units.begin(),state.Units.end(),[&](const auto& unit){return unit.At==action.Target;});
    if(target==state.Units.end())return 0;
    SkillPreview skill;
    AttackPreview attack;
    std::optional<CharacterAbility> definition;
    if(action.Kind==BasicActionKind::Attack)attack=battle.PreviewAttack(actor->Id,action.Target);
    else if(action.Kind==BasicActionKind::UseSkill){
        definition=FormalSkill(actor->Id,action.Ability);
        skill=battle.PreviewSkill(actor->Id,action.Ability,action.Target);
        if(!skill.Legal)return 0;
        attack=skill.Attack;
    }else return 0;
    const auto hit=std::find_if(attack.Hits.begin(),attack.Hits.end(),[&](const auto& item){return item.TargetId==target->Id;});
    const bool damaging=attack.Legal && target->Side==Faction::Enemy && hit!=attack.Hits.end() && hit->Damage>0;
    bool preferred=false;
    switch(actor->Id){
    case 1:preferred=damaging && hit->Damage>=target->Hp;break;
    case 2:preferred=skill.Healing>0;break;
    case 3:preferred=damaging && (target->AttackCursed || target->MovementWebbed);break;
    case 4:preferred=damaging && definition &&
        ((definition->Effects.AttackPenalty>0 && !target->AttackCursed) ||
         (definition->Effects.MovementPenalty>0 && !target->MovementWebbed));break;
    case 5:{
        const int range=definition && definition->Range?definition->Range:
            actor->Range+(!definition && action.Kind==BasicActionKind::UseSkill && action.Ability==Skill::FarStrike?2:0);
        preferred=damaging && std::abs(actor->At.X-target->At.X)+std::abs(actor->At.Y-target->At.Y)==range;break;
    }
    case 6:preferred=damaging && action.Kind==BasicActionKind::UseSkill;break;
    case 7:preferred=skill.Support && target->Id==actor->Id && skill.DefenseBuff>0;break;
    case 8:preferred=skill.Cleanse;break;
    }
    return preferred?500:0;
}
}
