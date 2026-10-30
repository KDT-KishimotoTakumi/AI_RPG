#include "Run.h"
#include <algorithm>
namespace GirlsSpire {
bool Run::CompleteBattle(std::uint64_t revision,BattleOutcome outcome)
{
    if(state_.Phase!=RunPhase::Event || revision!=state_.Revision ||
        (outcome!=BattleOutcome::Won && outcome!=BattleOutcome::Lost)) return false;
    const auto kind=state_.Active.Kind;
    if(kind!=NodeKind::Battle && kind!=NodeKind::Elite && kind!=NodeKind::Rescue && kind!=NodeKind::MidBoss && kind!=NodeKind::FinalBoss) return false;
    CompleteEvent(revision,outcome==BattleOutcome::Lost);
    if(outcome==BattleOutcome::Won) {
        afterReward_=state_.Phase;state_.Phase=RunPhase::Reward;
        state_.RewardRecipient=-1;state_.Rewards.clear();
    }
    return true;
}
bool Run::ChooseRecipient(std::uint64_t revision,int actorId)
{
    if(state_.Phase!=RunPhase::Reward || revision!=state_.Revision || state_.RewardRecipient!=-1 || actorId<0 || actorId>8) return false;
    if(actorId!=0 && std::find(state_.Recruited.begin(),state_.Recruited.end(),actorId-1)==state_.Recruited.end()) return false;
    state_.RewardRecipient=actorId;
    state_.Rewards=OfferRewards(state_.Growth[actorId],random_);
    ++state_.Revision;return true;
}
bool Run::ClaimReward(std::uint64_t revision,int index)
{
    if(state_.Phase!=RunPhase::Reward || revision!=state_.Revision || state_.RewardRecipient<0 || index<0 ||
        static_cast<std::size_t>(index)>=state_.Rewards.size()) return false;
    if(!TakeReward(state_.Growth[state_.RewardRecipient],state_.Rewards[index])) return false;
    auto& health=state_.Health[state_.RewardRecipient];
    if(state_.Rewards[index]==Reward::MaxHp && health.Hp>=0)
        health.Hp=std::min(MaxHp(state_.RewardRecipient),health.Hp+10);
    state_.Rewards.clear();state_.RewardRecipient=-1;state_.Phase=afterReward_;
    if(state_.Phase==RunPhase::Won) {state_.Growth={};state_.Health={};state_.Levels={};state_.Wallet={0,0};state_.Accessories={};}
    ++state_.Revision;return true;
}
}
