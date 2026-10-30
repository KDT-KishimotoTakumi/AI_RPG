#include "Application/Run.h"
#include <iostream>
#include <functional>
using namespace GirlsSpire;
int TestRunRestore()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL RunRestore: "<<why<<'\n';}};
    const auto step=[](Run& run) {
        const auto s=run.Snapshot();
        if(s.Phase==RunPhase::Route) return run.ChooseNode(s.Revision,0);
        if(s.Phase==RunPhase::Reward) return s.RewardRecipient<0?run.ChooseRecipient(s.Revision,0):run.ClaimReward(s.Revision,0);
        if(s.Phase!=RunPhase::Event) return false;
        if(s.Active.Kind==NodeKind::Social && !s.Recruited.empty()) return run.Converse(s.Revision,s.Recruited[0]+1,ConversationChoice::Warm);
        if(s.Active.Kind==NodeKind::Rest) return run.Rest(s.Revision);
        if(s.Active.Kind==NodeKind::Shop) run.Purchase(s.Revision,ShopOffer::Medicine,0);
        const auto revision=run.Snapshot().Revision;
        if(run.CompleteBattle(revision,BattleOutcome::Won)) return true;
        return run.CompleteEvent(revision);
    };
    for(int seed=0;seed<100;++seed) {
        Profile profile;profile.Bonds.fill(seed);
        Run run(seed,static_cast<Difficulty>(seed%3),static_cast<HeroRole>(seed%4),profile);
        for(int n=0;n<150;++n) {
            const auto saved=run.Checkpoint();const auto bonds=run.ProfileState();
            auto restored=Run::Restore(saved,bonds);
            check(restored.has_value(),"経路・イベント・報酬・終了の復元");
            if(restored) {
                check(restored->Checkpoint()==saved && restored->ProfileState()==bonds,"乱数・報酬後遷移を含む全項目一致");
                auto original=run;
                check(step(original)==step(*restored),"復元後の次操作一致");
                check(original.Checkpoint()==restored->Checkpoint() && original.ProfileState()==restored->ProfileState(),"継続結果と乱数一致");
            }
            if(!step(run)) break;
        }
        check(run.Snapshot().Phase==RunPhase::Won,"最終報酬後に勝利");
    }
    Run initial(42,Difficulty::Beginner,HeroRole::Sword);
    const auto saved=initial.Checkpoint();
    const auto reject=[&](const std::function<void(RunCheckpoint&)>& mutate) {
        auto bad=saved;mutate(bad);const auto before=bad;
        check(!Run::Restore(bad,{}),"不正保存状態拒否");
        check(bad==before && initial.Checkpoint()==saved,"失敗時入力と元周回不変");
    };
    reject([](auto& c){c.State.Day=0;});reject([](auto& c){c.State.Day=13;});
    reject([](auto& c){c.State.EventsToday=3;});reject([](auto& c){c.State.EventIndex=c.State.EventsToday;});
    reject([](auto& c){c.State.Level=static_cast<Difficulty>(3);});
    reject([](auto& c){c.State.Role=static_cast<HeroRole>(4);});
    reject([](auto& c){c.State.Phase=static_cast<RunPhase>(5);});
    reject([](auto& c){c.AfterReward=RunPhase::Event;});
    reject([](auto& c){c.State.Recruited={0,0};});reject([](auto& c){c.State.Recruited={8};});
    reject([](auto& c){c.State.Recruited={0,1,2,3,4};});
    reject([](auto& c){c.State.Choices.clear();});
    reject([](auto& c){c.State.Choices[0].Candidate=8;});
    reject([](auto& c){c.State.Active.Kind=static_cast<NodeKind>(9);});
    reject([](auto& c){c.State.Wallet.Coins=-1;});reject([](auto& c){c.State.Wallet.Medicines=6;});
    reject([](auto& c){c.State.Stock.Power=2;});reject([](auto& c){c.State.Stock.Guard=-1;});
    reject([](auto& c){c.State.Stock.Medicines=4;});
    reject([](auto& c){c.State.Growth[0].Bonuses[0]=-1;});
    reject([](auto& c){c.State.Growth[0].Bonuses[0]=1001;});
    reject([](auto& c){c.State.Health[0].Hp=-2;});reject([](auto& c){c.State.Health[0].Hp=81;});
    reject([](auto& c){c.State.Health[0].Wounds=5;});
    reject([](auto& c){c.State.Levels[0].Level=0;});reject([](auto& c){c.State.Levels[0]={10,1};});
    reject([](auto& c){c.State.Accessories[0]=static_cast<Accessory>(3);});
    reject([](auto& c){c.State.RewardRecipient=8;});
    reject([](auto& c){c.State.Rewards={Reward::Heal};});
    Profile permanent;permanent.Bonds.fill(100);
    auto restored=Run::Restore(saved,permanent);
    check(restored && restored->ProfileState()==permanent,"恒久データは別引数から取得");
    permanent.Bonds[0]=101;check(!Run::Restore(saved,permanent),"不正恒久データ拒否");
    initial.ChooseNode(initial.Snapshot().Revision,0);initial.CompleteEvent(initial.Snapshot().Revision,true);
    auto lost=Run::Restore(initial.Checkpoint(),{});
    check(lost && lost->Checkpoint()==initial.Checkpoint(),"敗北状態復元");
    std::cout<<"RunRestore: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
