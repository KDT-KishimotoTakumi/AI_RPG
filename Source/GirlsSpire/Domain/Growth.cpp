#include "Growth.h"
#include <algorithm>
namespace GirlsSpire {
std::vector<Reward> OfferRewards(const GrowthProgress& progress,Random& random)
{
    std::vector<Reward> skills,result;
    for(int i=0;i<3;++i) if(!progress.Learned[i]) skills.push_back(static_cast<Reward>(4+i));
    if(!skills.empty()) result.push_back(skills[random.Uniform(static_cast<std::uint32_t>(skills.size()))]);
    std::vector<Reward> stats{Reward::MaxHp,Reward::Attack,Reward::Defense,Reward::Speed};
    while(result.size()<3) {
        const auto index=random.Uniform(static_cast<std::uint32_t>(stats.size()));
        result.push_back(stats[index]);stats.erase(stats.begin()+index);
    }
    return result;
}
bool TakeReward(GrowthProgress& progress,Reward reward)
{
    const int id=static_cast<int>(reward);
    if(id<0 || id>6) return false;
    if(id>=4) {
        if(progress.Learned[id-4]) return false;
        progress.Learned[id-4]=true;
    } else {
        const int amounts[]={10,2,1,1};
        // 12日・最大24報酬では届かない防御的な数値検査。
        if(progress.Bonuses[id]<0 || progress.Bonuses[id]>1000-amounts[id]) return false;
        progress.Bonuses[id]+=amounts[id];
    }
    return true;
}
void ApplyGrowth(BattleUnit& unit,const GrowthProgress& progress)
{
    unit.MaxHp+=progress.Bonuses[0];unit.Hp=std::min(unit.MaxHp,unit.Hp+progress.Bonuses[0]);
    unit.Attack+=progress.Bonuses[1];unit.Defense+=progress.Bonuses[2];unit.Speed+=progress.Bonuses[3];
    for(int i=0;i<3;++i) unit.Skills[i+2]=unit.Skills[i+2] || progress.Learned[i];
}
}
