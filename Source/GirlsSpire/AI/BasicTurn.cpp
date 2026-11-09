#include "BasicTurn.h"
#include "EnemyTactics.h"
#include "EnemyCharacterSkills.h"
#include "Domain/AbilityCatalog.h"
#include <algorithm>
#include <cstdlib>
#include <limits>
namespace GirlsSpire {
BasicAction ChooseRouteApproach(const Battle& battle)
{
    const auto& state=battle.Snapshot();
    const auto actor=std::find_if(state.Units.begin(),state.Units.end(),
        [&](const BattleUnit& unit) { return unit.Id==state.ActiveId; });
    const auto route=[&](Position at) {
        int shortest=std::numeric_limits<int>::max();
        for(const auto& target:state.Units) {
            if(target.Hp<=0 || target.Side==actor->Side || (actor->Side==Faction::Player && target.Side==Faction::Neutral)) continue;
            const int steps=battle.RouteDistance(at,target.At);
            if(steps>=0) shortest=std::min(shortest,steps);
        }
        return shortest;
    };
    BasicAction best;
    int shortest=route(actor->At);
    for(const auto destination:battle.Reachable(actor->Id)) {
        const auto move=battle.PreviewMove(actor->Id,destination);
        if(!move.Legal || move.KnockedOut) continue;
        const int steps=route(destination);
        if(steps<shortest) { shortest=steps; best={BasicActionKind::Move,destination}; }
    }
    return best;
}
BasicAction ChooseBasicAction(const Battle& battle)
{
    const auto& state = battle.Snapshot();
    if (state.Outcome != BattleOutcome::Ongoing) return {};
    const auto actor = std::find_if(state.Units.begin(),state.Units.end(),
        [&](const BattleUnit& unit) { return unit.Id == state.ActiveId; });
    BasicAction best;
    if(const auto recovery=ChooseCharacterRecovery(battle))return *recovery;
    if(!state.Acted && actor->Side==Faction::Enemy && actor->Role==BattleRole::Fox) {
        const BattleUnit* recipient=nullptr;
        for(const auto& target:state.Units) {
            if(target.Hp*2>target.MaxHp || !battle.PreviewSkill(actor->Id,Skill::FirstAid,target.At).Legal)continue;
            // 浮動小数点の丸めに依存せずHP割合を比較し、同率は人物IDで固定する。
            if(!recipient || target.Hp*recipient->MaxHp<recipient->Hp*target.MaxHp ||
                (target.Hp*recipient->MaxHp==recipient->Hp*target.MaxHp && target.Id<recipient->Id))recipient=&target;
        }
        if(recipient)return {BasicActionKind::UseSkill,recipient->At,Skill::FirstAid};
    }
    const auto attack=ChooseRankedAttack(battle);
    if(attack)return attack->Action;
    if (state.Acted) {
        bool usedSupport=state.UsedSkill==static_cast<int>(Skill::FirstAid) || state.UsedSkill==static_cast<int>(Skill::Heal);
        if(UsesCharacterEnemyAI(battle) && state.UsedSkill>=0 && state.UsedSkill<2)
            usedSupport=FormalSkill(actor->Id,static_cast<Skill>(state.UsedSkill))->Target!=AbilityTarget::Enemy;
        if(actor->Side!=Faction::Enemy || actor->Role!=BattleRole::Ninja || state.Moved ||
            usedSupport)return best;
        const auto distance=[&](Position at) {
            int nearest=std::numeric_limits<int>::max();
            for(const auto& unit:state.Units)if(unit.Side==Faction::Player && unit.Hp>0)
                nearest=std::min(nearest,std::abs(at.X-unit.At.X)+std::abs(at.Y-unit.At.Y));
            return nearest;
        };
        int leastDamage=battle.PotentialHostileDamage(actor->Id),farthest=distance(actor->At);
        for(const auto destination:battle.Reachable(actor->Id)) {
            const auto move=battle.PreviewMove(actor->Id,destination);
            if(!move.Legal || move.KnockedOut)continue;
            auto projection=battle;
            if(!projection.Move(state.Revision,actor->Id,destination))continue;
            const int damage=move.Damage+projection.PotentialHostileDamage(actor->Id);
            const int separation=distance(destination);
            if(damage<leastDamage || (damage==leastDamage && separation>farthest)) {
                leastDamage=damage;farthest=separation;best={BasicActionKind::Move,destination};
            }
        }
        return best;
    }
    if(const auto preparation=ChooseCharacterPreparation(battle))return *preparation;
    if(actor->Side==Faction::Enemy && state.EnemyAI==EnemyThinking::Tactical)return ChooseTacticalMove(battle);
    if(actor->Side==Faction::Enemy && (actor->Role==BattleRole::Archer || actor->Role==BattleRole::Gunner || actor->Role==BattleRole::CurseMonk || actor->Role==BattleRole::Fox || actor->Role==BattleRole::Spider || actor->Role==BattleRole::CastleLord)) {
        int firingScore=-1;
        for(const auto destination:battle.Reachable(actor->Id)) {
            auto projection=battle;
            if(!projection.Move(state.Revision,actor->Id,destination))continue;
            for(const auto& target:state.Units) {
                const auto shot=projection.PreviewAttack(actor->Id,target.At);
                if(!shot.Legal)continue;
                const auto& hit=shot.Hits.front();
                const int score=hit.Chance*std::min(hit.Damage,target.Hp);
                if(score>firingScore) {
                    firingScore=score;best={BasicActionKind::Move,destination};
                }
            }
        }
        // 移動の仮適用だけで射程・射線・所属を照合し、実戦の乱数は進めない。
        if(firingScore>=0)return best;
    }
    const auto nearest = [&](Position at) {
        int distance = std::numeric_limits<int>::max();
        for (const auto& target : state.Units) {
            if (target.Hp <= 0 || target.Side == actor->Side ||
                (actor->Side == Faction::Player && target.Side == Faction::Neutral)) continue;
            distance = std::min(distance,std::abs(at.X - target.At.X) + std::abs(at.Y - target.At.Y));
        }
        return distance;
    };
    int shortest = nearest(actor->At);
    for (auto destination : battle.Reachable(actor->Id)) {
        const int distance = nearest(destination);
        if (distance < shortest) { shortest = distance; best = {BasicActionKind::Move,destination}; }
    }
    if(best.Kind==BasicActionKind::Wait) return ChooseRouteApproach(battle);
    return best;
}

bool ApplyBasicAction(Battle& battle, const BasicAction& action)
{
    const auto& state = battle.Snapshot();
    switch (action.Kind) {
    case BasicActionKind::UseSkill: return battle.UseSkill(state.Revision,state.ActiveId,action.Ability,action.Target);
    case BasicActionKind::Move: return battle.Move(state.Revision,state.ActiveId,action.Target);
    case BasicActionKind::Attack: return battle.Attack(state.Revision,state.ActiveId,action.Target);
    case BasicActionKind::Wait: return battle.EndTurn(state.Revision,state.ActiveId,Facing::South);
    }
    return false;
}
}
