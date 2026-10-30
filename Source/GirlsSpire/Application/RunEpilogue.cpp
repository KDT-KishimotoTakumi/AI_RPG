#include "Run.h"
#include <algorithm>
namespace GirlsSpire {
std::vector<int> Run::EpilogueChoices() const
{
    if(state_.Phase!=RunPhase::Won)return {};
    std::vector<int> result{0};
    for(int id=1;id<=8;++id)if(HasMember(id) && profile_.Bonds[id-1]>=80 && profile_.SpecialConversations[id-1])result.push_back(id);
    return result;
}
std::optional<Profile> Run::ProfileForEpilogue(int actorId) const
{
    const auto choices=EpilogueChoices();
    if(std::find(choices.begin(),choices.end(),actorId)==choices.end())return std::nullopt;
    auto result=profile_;result.Epilogues[actorId]=true;return result;
}
}
