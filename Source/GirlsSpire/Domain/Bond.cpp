#include "Bond.h"
#include <algorithm>
namespace GirlsSpire {
bool ValidProfile(const Profile& profile)
{
    return profile.HighestDifficulty>=0 && profile.HighestDifficulty<=2 &&
        std::all_of(profile.Bonds.begin(),profile.Bonds.end(),[](int rank){return rank>=0 && rank<=100;}) &&
        std::all_of(profile.DiscoveredCombos.begin(),profile.DiscoveredCombos.end(),[](int id){return id>0;}) &&
        std::adjacent_find(profile.DiscoveredCombos.begin(),profile.DiscoveredCombos.end(),[](int a,int b){return a>=b;})==profile.DiscoveredCombos.end();
}
bool DiscoverCombo(Profile& profile,int recipeId)
{
    if(recipeId<=0 || !ValidProfile(profile))return false;
    const auto at=std::lower_bound(profile.DiscoveredCombos.begin(),profile.DiscoveredCombos.end(),recipeId);
    if(at==profile.DiscoveredCombos.end() || *at!=recipeId)profile.DiscoveredCombos.insert(at,recipeId);
    return true;
}
bool CanStartDifficulty(const Profile& profile,int difficulty)
{
    return ValidProfile(profile) && difficulty>=0 && difficulty<=profile.HighestDifficulty;
}
bool ApplyConversation(Profile& profile,int actorId,ConversationChoice choice)
{
    if(actorId<1 || actorId>8 || choice<ConversationChoice::Warm || choice>ConversationChoice::Cold || !ValidProfile(profile)) return false;
    const int delta=choice==ConversationChoice::Warm?10:choice==ConversationChoice::Cold?-5:0;
    auto& rank=profile.Bonds[actorId-1];rank=std::clamp(rank+delta,0,100);return true;
}
}
