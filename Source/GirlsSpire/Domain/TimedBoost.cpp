#include "TimedBoost.h"
namespace GirlsSpire {
bool ValidTimedBoost(const TimedBoost& boost)
{
    return boost.Amount>=0 && (boost.Amount>0 || !boost.SkipCurrentEnd);
}
bool ApplyTimedBoost(TimedBoost& boost,int amount,bool duringRecipientTurn)
{
    if(!ValidTimedBoost(boost) || amount<=0)return false;
    if(amount>boost.Amount)boost.Amount=amount;
    boost.SkipCurrentEnd=duringRecipientTurn;
    return true;
}
bool EndTimedBoostTurn(TimedBoost& boost)
{
    if(!ValidTimedBoost(boost))return false;
    if(boost.SkipCurrentEnd)boost.SkipCurrentEnd=false;
    else boost.Amount=0;
    return true;
}
}
