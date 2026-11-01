#include "Run.h"
#include "Domain/CharacterCatalog.h"
#include <algorithm>
#include <limits>
#include <set>
namespace GirlsSpire {
namespace {
bool Between(int value,int low,int high) { return value>=low && value<=high; }
bool ValidNode(const RouteNode& node)
{
    return Between(static_cast<int>(node.Kind),0,8) &&
        (node.Kind==NodeKind::Rescue?Between(node.Candidate,0,7):node.Candidate==-1);
}
bool ValidState(const RunCheckpoint& checkpoint)
{
    const auto& s=checkpoint.State;
    if(!Between(s.Day,1,12) || !Between(s.EventsToday,1,2) || !Between(s.EventIndex,0,s.EventsToday-1) ||
       !Between(static_cast<int>(s.Level),0,2) || !Between(static_cast<int>(s.Role),0,3) ||
       !Between(static_cast<int>(s.Phase),0,4) ||
       (checkpoint.AfterReward!=RunPhase::Route && checkpoint.AfterReward!=RunPhase::Won) || !ValidNode(s.Active)) return false;
    std::set<int> members;
    if(s.Recruited.size()>4) return false;
    for(int id:s.Recruited) if(!Between(id,0,7) || !members.insert(id).second) return false;
    if(!Between(s.PendingRescue,-1,7))return false;
    if(s.PendingRescue>=0 && (s.Active.Kind!=NodeKind::Rescue || s.Active.Candidate!=s.PendingRescue ||
        !members.contains(s.PendingRescue) || checkpoint.AfterReward!=RunPhase::Route ||
        (s.Phase!=RunPhase::Route && s.Phase!=RunPhase::Reward)))return false;
    const bool route=s.Phase==RunPhase::Route || (s.Phase==RunPhase::Reward && checkpoint.AfterReward==RunPhase::Route);
    if(route ? (s.Choices.size()!=1 && s.Choices.size()!=3) : !s.Choices.empty()) return false;
    std::set<std::pair<int,int>> choices;
    for(const auto& node:s.Choices)
        if(!ValidNode(node) || (node.Kind==NodeKind::Rescue && members.contains(node.Candidate)) ||
           !choices.insert({static_cast<int>(node.Kind),node.Candidate}).second) return false;
    if(s.Phase==RunPhase::Event && s.Active.Kind==NodeKind::Rescue && members.contains(s.Active.Candidate)) return false;
    if(!Between(s.Wallet.Coins,0,std::numeric_limits<int>::max()-100) || !Between(s.Wallet.Medicines,0,5) ||
       !Between(s.Stock.Power,0,1) || !Between(s.Stock.Guard,0,1) || !Between(s.Stock.Medicines,0,3)) return false;
    for(int id=0;id<9;++id) {
        for(int bonus:s.Growth[id].Bonuses) if(!Between(bonus,0,1000)) return false;
        auto level=s.Levels[id];
        if(!GainExperience(level,0) || level!=s.Levels[id] || !Between(s.Health[id].Wounds,0,4) ||
           !Between(static_cast<int>(s.Accessories[id]),0,2)) return false;
        const int maxHp=EffectiveMaxHp(FindCharacter(id)->MaxHp+s.Growth[id].Bonuses[0]+LevelHpBonus(level),s.Health[id].Wounds);
        if(!Between(s.Health[id].Hp,-1,maxHp)) return false;
    }
    if(s.Phase!=RunPhase::Reward) return s.RewardRecipient==-1 && s.Rewards.empty();
    if(s.RewardRecipient==-1) return s.Rewards.empty();
    if(!Between(s.RewardRecipient,0,8) || (s.RewardRecipient!=0 && !members.contains(s.RewardRecipient-1)) || s.Rewards.size()!=3) return false;
    std::set<int> rewards;
    for(auto reward:s.Rewards) {
        auto growth=s.Growth[s.RewardRecipient];
        if(!TakeReward(growth,reward) || !rewards.insert(static_cast<int>(reward)).second) return false;
    }
    return true;
}
}
RunCheckpoint Run::Checkpoint() const { return {state_,random_.State(),afterReward_}; }
std::optional<Run> Run::Restore(const RunCheckpoint& checkpoint,const Profile& profile)
{
    if(!ValidProfile(profile) || !ValidState(checkpoint)) return std::nullopt;
    Run result(0,checkpoint.State.Level,checkpoint.State.Role,profile);
    result.state_=checkpoint.State;
    // 初期経路生成の乱数を残さず、保存時の消費位置へ戻す。
    result.random_=Random(checkpoint.RandomState);
    result.afterReward_=checkpoint.AfterReward;
    return result;
}
}
