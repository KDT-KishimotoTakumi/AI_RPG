#include "Health.h"
#include <algorithm>
#include <stdexcept>
namespace GirlsSpire {
namespace {
bool Valid(int base,int wounds) { return base>=10 && base<=10000 && wounds>=0 && wounds<=4; }
}
int EffectiveMaxHp(int baseMaxHp,int wounds)
{
    if(!Valid(baseMaxHp,wounds)) throw std::invalid_argument("最大HPまたは負傷が無効です");
    return std::max(10,baseMaxHp-10*wounds);
}
bool RecoverHealth(HealthProgress& health,int baseMaxHp,int finalHp)
{
    if(!Valid(baseMaxHp,health.Wounds)) return false;
    const int max=EffectiveMaxHp(baseMaxHp,health.Wounds);
    if(finalHp<0 || finalHp>max) return false;
    if(finalHp==0) {
        health.Wounds=std::min(4,health.Wounds+1);
        health.Hp=(EffectiveMaxHp(baseMaxHp,health.Wounds)+1)/2;
    } else health.Hp=std::min(max,finalHp+(max+4)/5);
    return true;
}
bool HealAtRest(HealthProgress& health,int baseMaxHp)
{
    if(!Valid(baseMaxHp,health.Wounds)) return false;
    const int max=EffectiveMaxHp(baseMaxHp,health.Wounds);
    if(health.Hp<-1 || health.Hp>max) return false;
    health.Hp=health.Hp==-1?max:std::min(max,health.Hp+(max+1)/2);
    return true;
}
bool TreatHealth(HealthProgress& health)
{
    if(health.Wounds<1 || health.Wounds>4) return false;
    --health.Wounds;return true;
}
}
