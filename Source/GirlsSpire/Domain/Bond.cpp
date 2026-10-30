#include "Bond.h"
#include <algorithm>
namespace GirlsSpire {
bool ValidProfile(const Profile& profile)
{
    return std::all_of(profile.Bonds.begin(),profile.Bonds.end(),[](int rank){return rank>=0 && rank<=100;});
}
bool ApplyConversation(Profile& profile,int actorId,ConversationChoice choice)
{
    if(actorId<1 || actorId>8 || choice<ConversationChoice::Warm || choice>ConversationChoice::Cold || !ValidProfile(profile)) return false;
    const int delta=choice==ConversationChoice::Warm?10:choice==ConversationChoice::Cold?-5:0;
    auto& rank=profile.Bonds[actorId-1];rank=std::clamp(rank+delta,0,100);return true;
}
}
