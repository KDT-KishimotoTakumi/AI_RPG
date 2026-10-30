#include "Domain/Health.h"
#include <iostream>
#include <algorithm>
#include <stdexcept>
int TestHealth()
{
    using namespace GirlsSpire;
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* label){++checks;if(!ok){++failed;std::cerr<<"FAIL Health: "<<label<<'\n';}};
    for(int base=10;base<=110;++base) for(int wounds=0;wounds<=4;++wounds) {
        const int max=std::max(10,base-wounds*10);
        check(EffectiveMaxHp(base,wounds)==max,"最大HP低下と下限");
        for(int hp:{1,max-1,max}) {
            HealthProgress h{hp,wounds};
            check(RecoverHealth(h,base,hp) && h.Hp==std::min(max,hp+(max+4)/5) && h.Wounds==wounds,"生存20%端数切上げ");
            h={hp,wounds};check(HealAtRest(h,base) && h.Hp==std::min(max,hp+(max+1)/2) && h.Wounds==wounds,"休息50%端数切上げ");
        }
        HealthProgress h{1,wounds};const int next=std::min(4,wounds+1);
        check(RecoverHealth(h,base,0) && h.Wounds==next && h.Hp==(std::max(10,base-next*10)+1)/2,"負傷後50%復帰・20%を重ねない");
        const int hp=h.Hp;
        check(TreatHealth(h) && h.Wounds==next-1 && h.Hp==hp,"治療で一段階改善・HP不変");
    }
    HealthProgress h;check(HealAtRest(h,80) && h.Hp==80,"初期HPは最大");
    const auto before=h;check(!TreatHealth(h) && h==before,"負傷なし拒否");
    for(int value:{-1,81}) check(!RecoverHealth(h,80,value) && h==before,"無効終了HP拒否");
    h={81,0};const auto invalid=h;check(!HealAtRest(h,80) && h==invalid,"不正現在HP拒否");
    h={-2,0};check(!HealAtRest(h,80),"不正な初期HP拒否");
    for(int base:{0,9,10001}) {
        h={1,0};check(!RecoverHealth(h,base,1) && !HealAtRest(h,base),"無効基礎HP拒否");
    }
    for(int wounds:{-1,5}) {
        h={1,wounds};const auto saved=h;
        check(!RecoverHealth(h,80,1) && !HealAtRest(h,80) && !TreatHealth(h) && h==saved,"不正負傷拒否");
        bool threw=false;try{EffectiveMaxHp(80,wounds);}catch(const std::invalid_argument&){threw=true;}
        check(threw,"無効最大HP計算拒否");
    }
    std::cout<<"Health: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
