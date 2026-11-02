#include "Battle.h"
#include "AbilityCatalog.h"
#include "Rules.h"
#include <algorithm>
#include <cstdlib>
namespace GirlsSpire {
AttackPreview Battle::PreviewCharacterUltimate(int actorId,Position target) const
{
    const auto definition=FormalUltimate(actorId,actorId>0 && actorId<=8?state_.StartingBonds[actorId-1]:0);
    if(!definition)return {};
    const auto& ability=*definition;const auto& actor=*Find(actorId);
    const auto distance=[](Position a,Position b){return std::abs(a.X-b.X)+std::abs(a.Y-b.Y);};
    const bool selfArea=ability.Area==AbilityArea::SelfRadius || ability.Area==AbilityArea::AdjacentFour;
    if(selfArea && target!=actor.At)return {};
    const int range=ability.Range?ability.Range:actor.Range;
    std::vector<Position> line;
    if(ability.Area==AbilityArea::CardinalLine){
        const int dx=target.X-actor.At.X,dy=target.Y-actor.At.Y;
        if((dx==0)==(dy==0))return {};
        const Position step{(dx>0)-(dx<0),(dy>0)-(dy<0)};
        for(int i=1;i<=range;++i){const Position at{actor.At.X+step.X*i,actor.At.Y+step.Y*i};if(!Inside(at))break;const auto& tile=state_.Tiles[at.Y*state_.Width+at.X];if(tile.Wall || tile.ObstacleHp>0)break;line.push_back(at);}
    }else if(!selfArea){
        const auto victim=Find(Occupant(target));
        if(!victim || victim->Side!=Faction::Enemy || distance(actor.At,target)>range || !HasLineOfSight(actor.At,target))return {};
    }
    AttackPreview preview;preview.SelfDefenseBuff=ability.Effects.SelfDefenseBuff;
    for(const auto& unit:state_.Units){
        if(unit.Hp<=0 || !HasLineOfSight(actor.At,unit.At))continue;
        if(ability.Target==AbilityTarget::LivingAlly){
            if(unit.Side!=Faction::Player || distance(actor.At,unit.At)>range)continue;
            PredictedAid aid{unit.Id,std::min(ability.Effects.Healing,unit.MaxHp-unit.Hp),ability.Effects.AttackBuff,ability.Effects.Cleanse && (unit.AttackCursed || unit.MovementWebbed)};
            preview.Legal=preview.Legal || aid.Healing>0 || aid.AttackBuff>0 || aid.Cleanse;preview.Aid.push_back(aid);continue;
        }
        if(unit.Side!=Faction::Enemy)continue;
        bool affected=false;
        switch(ability.Area){
        case AbilityArea::Single:affected=unit.At==target;break;
        case AbilityArea::TargetCross:affected=distance(unit.At,target)<=1;break;
        case AbilityArea::CardinalLine:affected=std::find(line.begin(),line.end(),unit.At)!=line.end();break;
        case AbilityArea::AdjacentFour:affected=distance(actor.At,unit.At)==1;break;
        default:break;
        }
        if(!affected)continue;
        auto projection=*this;projection.state_.ChargeReady=false;
        auto& source=*projection.Find(actorId);source.Attack+=ability.Effects.AttackBonus;source.Range=24;
        auto& victim=*projection.Find(unit.Id);victim.Defense=std::max(0,victim.Defense+victim.DefenseBoost.Amount-ability.Effects.IgnoreDefense);victim.DefenseBoost={};
        const auto hit=projection.PreviewAttack(actorId,unit.At,0,false);
        if(hit.Legal)preview.Hits.push_back(hit.Hits.front());
    }
    std::sort(preview.Hits.begin(),preview.Hits.end(),[](const PredictedHit& a,const PredictedHit& b){return a.TargetId<b.TargetId;});
    std::sort(preview.Aid.begin(),preview.Aid.end(),[](const PredictedAid& a,const PredictedAid& b){return a.TargetId<b.TargetId;});
    preview.Legal=preview.Legal || !preview.Hits.empty();return preview;
}
bool Battle::UseCharacterUltimate(int actorId,const AttackPreview& preview)
{
    const auto ability=*FormalUltimate(actorId,actorId?state_.StartingBonds[actorId-1]:0);
    combos_.Spend(50);lastHits_.clear();std::vector<int> enemies,knockedOut;bool succeeded=true;
    for(const auto& aid:preview.Aid){auto& recipient=*Find(aid.TargetId);recipient.Hp+=aid.Healing;if(aid.AttackBuff)ApplyTimedBoost(recipient.AttackBoost,aid.AttackBuff,actorId==recipient.Id);if(aid.Cleanse){recipient.AttackCursed=false;recipient.MovementWebbed=false;}AddAidBond(state_.BondGains,actorId,recipient.Id,aid.Healing);}
    for(const auto& hit:preview.Hits){
        auto& victim=*Find(hit.TargetId);const bool success=random_.Uniform(100)<static_cast<std::uint32_t>(hit.Chance);const int damage=success?std::min(victim.Hp,hit.Damage):0;victim.Hp-=damage;
        if(success && ability.Effects.AttackPenalty)victim.AttackCursed=true;if(success && ability.Effects.MovementPenalty)victim.MovementWebbed=true;
        if(success)enemies.push_back(victim.Id);if(victim.Hp==0)knockedOut.push_back(victim.Id);succeeded=succeeded && success;lastHits_.push_back({victim.Id,success,damage});
    }
    if(preview.SelfDefenseBuff)ApplyTimedBoost(Find(actorId)->DefenseBoost,preview.SelfDefenseBuff,true);
    state_.Acted=true;state_.ChargeReady=false;++state_.Revision;RecordCombo(actorId,Ending,succeeded,enemies,knockedOut);Evaluate();return true;
}
}
