#include "Domain/Level.h"
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace GirlsSpire;
int TestLevel()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL Level: "<<why<<'\n';}};
    for(int level=1;level<=10;++level) for(int xp:{0,50,100,199}) {
        if(level==10 && xp!=0) continue;
        for(int amount:{0,1,50,100,150,200,400,1800,std::numeric_limits<int>::max()}) {
            LevelProgress p{level,xp};
            const long long total=(level-1)*200LL+xp+amount;
            const int wantLevel=static_cast<int>(std::min(10LL,1+total/200));
            const int wantXp=wantLevel==10?0:static_cast<int>(total%200);
            check(GainExperience(p,amount) && p==LevelProgress{wantLevel,wantXp},"閾値・余剰・上限・大きな経験値");
        }
        const LevelProgress p{level,xp};
        check(LevelHpBonus(p)==5*(level-1),"HP成長");
        BattleUnit u;u.MaxHp=80;u.Hp=1;u.Attack=18;u.Defense=2;
        const auto before=u;ApplyLevel(u,p);
        check(u.MaxHp==80+5*(level-1) && u.Attack==18+level-1 && u.Defense==2+level-1,"能力成長");
        check(u.Hp==1 && u.Speed==before.Speed && u.Range==before.Range && u.Movement==before.Movement && u.Accuracy==before.Accuracy && u.Evasion==before.Evasion,"現在HP・他能力は不変");
    }
    for(auto p:{LevelProgress{0,0},LevelProgress{11,0},LevelProgress{1,-1},LevelProgress{1,200},LevelProgress{10,1}}) {
        const auto saved=p;check(!GainExperience(p,100) && p==saved,"不正状態は更新しない");
        bool threw=false;try{LevelHpBonus(p);}catch(const std::invalid_argument&){threw=true;}
        check(threw,"不正状態で能力を作らない");
    }
    LevelProgress p;check(!GainExperience(p,-1) && p==LevelProgress{},"負の獲得値拒否");
    std::cout<<"Level: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
