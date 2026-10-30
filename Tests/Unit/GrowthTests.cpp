#include "Domain/Growth.h"
#include <set>
#include <iostream>
int TestGrowth()
{
    using namespace GirlsSpire;
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* label){++checks;if(!ok){++failed;std::cerr<<"FAIL Growth: "<<label<<'\n';}};
    std::set<Reward> seen;
    for(int seed=0;seed<100;++seed) {
        GrowthProgress g;Random r(seed),repeat(seed);
        const auto offer=OfferRewards(g,r);
        check(offer==OfferRewards(g,repeat) && r.State()==repeat.State(),"100シード一致");
        check(offer.size()==3 && std::set<Reward>(offer.begin(),offer.end()).size()==3,"三択重複なし");
        if(offer.size()!=3) continue;
        check(static_cast<int>(offer[0])>=4 && static_cast<int>(offer[1])<4 && static_cast<int>(offer[2])<4,"技1と能力2");
        seen.insert(offer.begin(),offer.end());
        for(int count=0;count<3;++count) {
            auto next=OfferRewards(g,r);
            check(TakeReward(g,next[0]),"未取得技を取得");
            const auto saved=g;
            check(!TakeReward(g,next[0]) && g==saved,"重複取得拒否");
        }
        const auto full=OfferRewards(g,r);
        check(full.size()==3 && static_cast<int>(full[0])<4 && static_cast<int>(full[1])<4 && static_cast<int>(full[2])<4,"3技で能力だけ");
    }
    check(seen.size()==7,"全候補が抽選される");
    GrowthProgress g;
    for(auto reward:{Reward::MaxHp,Reward::Attack,Reward::Defense,Reward::Speed}) {
        check(TakeReward(g,reward) && TakeReward(g,reward),"同じ能力強化を再取得");
    }
    BattleUnit u;u.Hp=10;ApplyGrowth(u,g);
    check(u.MaxHp==50 && u.Hp==30 && u.Attack==16 && u.Defense==4 && u.Speed==12,"能力累積と現在HP");
    check(TakeReward(g,Reward::Sweep) && TakeReward(g,Reward::FarStrike) && TakeReward(g,Reward::Heal),"追加3技取得");
    BattleUnit learned;ApplyGrowth(learned,g);
    check(learned.Skills[2] && learned.Skills[3] && learned.Skills[4] && !learned.Skills[0],"追加技のみ反映");
    const auto saved=g;
    for(int bonus:{-1,1000}) {
        auto invalid=g;invalid.Bonuses[0]=bonus;const auto before=invalid;
        check(!TakeReward(invalid,Reward::MaxHp) && invalid==before,"不正な累積値を拒否");
    }
    BattleUnit full;full.Hp=full.MaxHp;full.Skills[2]=true;ApplyGrowth(full,{});
    check(full.Hp==full.MaxHp && full.Skills[2],"全快と既存技を維持");
    for(int id:{-1,7,999}) check(!TakeReward(g,static_cast<Reward>(id)) && g==saved,"不正報酬は不変");
    std::cout<<"Growth: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
