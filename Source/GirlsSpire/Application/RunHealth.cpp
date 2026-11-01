#include "Run.h"
#include "Domain/CharacterCatalog.h"
#include <algorithm>
#include <set>
namespace GirlsSpire {
bool Run::HasMember(int id) const
{
    return id>=0 && id<=8 && (id==0 || std::find(state_.Recruited.begin(),state_.Recruited.end(),id-1)!=state_.Recruited.end());
}
int Run::MaxHp(int id) const
{
    if(!HasMember(id)) return 0;
    return EffectiveMaxHp(BaseMaxHp(id),state_.Health[id].Wounds);
}
int Run::BaseMaxHp(int id) const
{
    return FindCharacter(id)->MaxHp+state_.Growth[id].Bonuses[0]+LevelHpBonus(state_.Levels[id]);
}
int Run::CurrentHp(int id) const
{
    if(!HasMember(id)) return 0;
    return state_.Health[id].Hp<0?MaxHp(id):state_.Health[id].Hp;
}
bool Run::CompleteBattle(std::uint64_t revision,const BattleSnapshot& battle)
{
    if(battle.Medicines<0 || battle.Medicines>state_.Wallet.Medicines) return false;
    if(!ValidBattleBond(battle.BondGains)) return false;
    for(int id=1;id<=8;++id)
        if(!HasMember(id) && (battle.BondGains.Combo[id-1]!=0 || battle.BondGains.Aid[id-1]!=0)) return false;
    auto next=*this;
    const auto kind=state_.Active.Kind;
    const int experience=kind==NodeKind::Elite?150:kind==NodeKind::MidBoss || kind==NodeKind::FinalBoss?200:100;
    std::set<int> seen;bool survivor=false;
    for(const auto& unit:battle.Units) if(unit.Side==Faction::Player) {
        const int id=unit.Id;
        if(!HasMember(id) || !seen.insert(id).second || unit.MaxHp!=MaxHp(id) || unit.Hp<0 || unit.Hp>unit.MaxHp) return false;
        survivor|=unit.Hp>0;
        if(battle.Outcome==BattleOutcome::Won) {
            GainExperience(next.state_.Levels[id],experience);
            RecoverHealth(next.state_.Health[id],next.BaseMaxHp(id),unit.Hp);
        }
    }
    if(seen.size()!=state_.Recruited.size()+1 ||
        (battle.Outcome==BattleOutcome::Won && !survivor) || (battle.Outcome==BattleOutcome::Lost && survivor)) return false;
    if(!next.CompleteBattle(revision,battle.Outcome)) return false;
    ApplyBattleBond(next.profile_,battle.BondGains);
    if(battle.Outcome==BattleOutcome::Won) {
        next.state_.Wallet.Coins+=kind==NodeKind::Elite?75:kind==NodeKind::MidBoss || kind==NodeKind::FinalBoss?100:50;
        next.state_.Wallet.Medicines=battle.Medicines;
        if(kind==NodeKind::Rescue)next.state_.PendingRescue=state_.Active.Candidate;
    }
    *this=std::move(next);return true;
}
bool Run::Rest(std::uint64_t revision,int patient)
{
    if(state_.Phase!=RunPhase::Event || state_.Active.Kind!=NodeKind::Rest || revision!=state_.Revision || patient<-1) return false;
    auto next=*this;
    if(patient==-1) {
        for(int id=0;id<=8;++id) if(HasMember(id))
            HealAtRest(next.state_.Health[id],BaseMaxHp(id));
    } else if(!HasMember(patient) || !TreatHealth(next.state_.Health[patient])) return false;
    next.CompleteEvent(revision);*this=std::move(next);return true;
}
}
