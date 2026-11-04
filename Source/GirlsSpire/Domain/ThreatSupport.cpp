#include "Battle.h"
#include "AbilityCatalog.h"
#include <algorithm>
namespace GirlsSpire {
bool Battle::ProjectEnemySupport(int actorId,const Battle& timeline)
{
    auto* attacker=Find(actorId);
    if(!state_.EnemyCharacterSkills || state_.Outcome!=BattleOutcome::Ongoing || !attacker || attacker->Hp<=0 || attacker->Side!=Faction::Enemy)return false;
    const int oldHp=attacker->Hp,oldAttack=attacker->AttackBoost.Amount;
    const bool oldCurse=attacker->AttackCursed,oldWeb=attacker->MovementWebbed;
    const auto offset=[&](int id){
        const auto& order=timeline.state_.Order;const auto found=std::find(order.begin(),order.end(),id);
        return found==order.end()?order.size():(static_cast<std::size_t>(found-order.begin())+order.size()-timeline.turnIndex_)%order.size();
    };
    for(const auto& caster:state_.Units){
        // 先行する支援だけを再帰的に仮定するので循環しない。自身の準備と攻撃は二重計上しない。
        if(caster.Id==actorId || caster.Hp<=0 || caster.Side!=Faction::Enemy || caster.Id<1 || caster.Id>8 || offset(caster.Id)>=offset(actorId))continue;
        const int hpBeforeCaster=attacker->Hp;
        for(int slot=0;slot<2;++slot){
            if(!caster.Skills[slot] || caster.Cooldowns[slot]>0)continue;
            const auto skill=static_cast<Skill>(slot);const auto ability=FormalSkill(caster.Id,skill);
            if(!ability || ability->Target!=AbilityTarget::LivingAlly)continue;
            const auto& effects=ability->Effects;
            if(!(effects.AttackBuff>attacker->AttackBoost.Amount || (effects.Healing>0 && attacker->Hp<attacker->MaxHp) || (effects.Cleanse && (attacker->AttackCursed || attacker->MovementWebbed))))continue;
            auto support=*this;support.state_.ActiveId=caster.Id;support.state_.Moved=false;support.state_.Acted=false;support.state_.ChargeReady=false;support.state_.UsedSkill=-1;
            support.turnIndex_=static_cast<std::size_t>(std::find(support.state_.Order.begin(),support.state_.Order.end(),caster.Id)-support.state_.Order.begin());
            support.ProjectEnemySupport(caster.Id,timeline);
            const auto apply=[&](const Battle& positioned){
                const auto preview=positioned.PreviewSkill(caster.Id,skill,attacker->At);
                if(!preview.Legal)return false;
                // 複数の候補位置は同じ一回の回復。支援効果を独立に上限化し、実際の行動を確約しない。
                attacker->Hp=std::max(attacker->Hp,std::min(attacker->MaxHp,hpBeforeCaster+preview.Healing));
                attacker->AttackBoost.Amount=std::max(attacker->AttackBoost.Amount,preview.AttackBuff);
                if(preview.Cleanse){attacker->AttackCursed=false;attacker->MovementWebbed=false;}
                return true;
            };
            if(apply(support))continue;
            for(const auto destination:support.Reachable(caster.Id)){
                auto moved=support;
                if(moved.Move(moved.Snapshot().Revision,caster.Id,destination) && apply(moved))break;
            }
        }
    }
    return attacker->Hp!=oldHp || attacker->AttackBoost.Amount!=oldAttack || attacker->AttackCursed!=oldCurse || attacker->MovementWebbed!=oldWeb;
}
}
