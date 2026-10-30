#include "BattleBond.h"
#include <algorithm>
#include <set>
namespace GirlsSpire {
bool ValidBattleBond(const BattleBond& gains)
{
    return std::all_of(gains.Combo.begin(),gains.Combo.end(),[](int n){return n>=0 && n<=5;}) &&
        std::all_of(gains.Aid.begin(),gains.Aid.end(),[](int n){return n>=0 && n<=3;});
}
bool AddComboBond(BattleBond& gains,const std::vector<int>& participants)
{
    const auto count=participants.size();
    if((count!=2 && count!=3 && count!=5) || !ValidBattleBond(gains)) return false;
    std::set<int> seen;
    for(int id:participants) if(id<0 || id>8 || !seen.insert(id).second) return false;
    const int delta=count==2?1:count==3?2:3;
    for(int id:participants) if(id>0) gains.Combo[id-1]=std::min(5,gains.Combo[id-1]+delta);
    return true;
}
bool AddAidBond(BattleBond& gains,int actorId,int targetId,int healing)
{
    if(actorId<0 || actorId>8 || targetId<0 || targetId>8 || actorId==targetId || healing<=0 || !ValidBattleBond(gains)) return false;
    for(int id:{actorId,targetId}) if(id>0) gains.Aid[id-1]=std::min(3,gains.Aid[id-1]+1);
    return true;
}
bool ApplyBattleBond(Profile& profile,const BattleBond& gains)
{
    if(!ValidProfile(profile) || !ValidBattleBond(gains)) return false;
    for(int id=0;id<8;++id) profile.Bonds[id]=std::min(100,profile.Bonds[id]+gains.Combo[id]+gains.Aid[id]);
    return true;
}
}
