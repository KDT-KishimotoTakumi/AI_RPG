#include "Level.h"
#include <algorithm>
#include <stdexcept>
namespace GirlsSpire {
namespace {
bool Valid(const LevelProgress& p)
{
    return p.Level>=1 && p.Level<=10 && p.Experience>=0 && p.Experience<200 && (p.Level<10 || p.Experience==0);
}
}
bool GainExperience(LevelProgress& p,int amount)
{
    if(!Valid(p) || amount<0) return false;
    // 上限までの総経験値に飽和させ、過剰な入力でも整数を溢れさせない。
    const int total=std::min(1800,(p.Level-1)*200+p.Experience+std::min(1800,amount));
    p.Level=1+total/200;p.Experience=total%200;
    return true;
}
int LevelHpBonus(const LevelProgress& p)
{
    if(!Valid(p)) throw std::invalid_argument("レベルまたは経験値が無効です");
    return 5*(p.Level-1);
}
void ApplyLevel(BattleUnit& unit,const LevelProgress& p)
{
    unit.MaxHp+=LevelHpBonus(p);
    unit.Attack+=p.Level-1;unit.Defense+=p.Level-1;
}
}
