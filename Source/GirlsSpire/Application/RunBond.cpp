#include "Run.h"
namespace GirlsSpire {
bool Run::ConverseRescue(std::uint64_t revision,ConversationChoice choice)
{
    if(state_.Revision!=revision || state_.PendingRescue<0 ||
        (state_.Phase!=RunPhase::Reward && state_.Phase!=RunPhase::Route))return false;
    auto next=*this;
    if(!ApplyConversation(next.profile_,state_.PendingRescue+1,choice))return false;
    next.state_.PendingRescue=-1;++next.state_.Revision;
    *this=std::move(next);return true;
}
bool Run::CanSpecialConverse(int actorId) const
{
    return state_.Phase==RunPhase::Event && state_.Active.Kind==NodeKind::Social &&
        actorId>=1 && actorId<=8 && HasMember(actorId) && profile_.Bonds[actorId-1]>=80 &&
        !profile_.SpecialConversations[actorId-1];
}
bool Run::SpecialConverse(std::uint64_t revision,int actorId)
{
    if(state_.Revision!=revision || !CanSpecialConverse(actorId)) return false;
    auto next=*this;
    next.profile_.SpecialConversations[actorId-1]=true;
    next.CompleteEvent(revision);*this=std::move(next);return true;
}
bool Run::Converse(std::uint64_t revision,int actorId,ConversationChoice choice)
{
    if(state_.Phase!=RunPhase::Event || state_.Active.Kind!=NodeKind::Social ||
       state_.Revision!=revision || actorId==0 || !HasMember(actorId)) return false;
    // 予告用Runコピーも独立したProfileを持ち、確定時だけ本体を置き換える。
    auto next=*this;
    if(!ApplyConversation(next.profile_,actorId,choice)) return false;
    next.CompleteEvent(revision);*this=std::move(next);return true;
}
}
