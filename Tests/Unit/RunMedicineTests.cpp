#include "Application/Encounter.h"
#include <iostream>
using namespace GirlsSpire;
int TestRunMedicine()
{
    int checks=0,failed=0,used=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL RunMedicine: "<<why<<'\n';}};
    for(int seed=0;seed<100;++seed) {
        Run run(seed,Difficulty::Beginner,HeroRole::Sword);bool bought=false;
        for(int step=0;step<24 && run.Snapshot().Phase==RunPhase::Route;++step) {
            int node=0;
            for(int i=0;i<static_cast<int>(run.Snapshot().Choices.size());++i)
                if(!bought && run.Snapshot().Choices[i].Kind==NodeKind::Shop) node=i;
            run.ChooseNode(run.Snapshot().Revision,node);
            const auto kind=run.Snapshot().Active.Kind;
            if(kind==NodeKind::Shop && !bought) {
                check(run.Purchase(run.Snapshot().Revision,ShopOffer::Medicine,0),"傷薬購入");bought=true;
                run.CompleteEvent(run.Snapshot().Revision);continue;
            }
            if(kind==NodeKind::Shop || kind==NodeKind::Rest || kind==NodeKind::Social || kind==NodeKind::Anomaly) {run.CompleteEvent(run.Snapshot().Revision);continue;}
            auto battle=Encounter::Create(run.Snapshot(),seed);
            if(bought) {
                const auto entry=run.Snapshot();
                check(battle.Snapshot().Medicines==1,"購入薬を戦場へ渡す");
                check(battle.UseMedicine(0,0,battle.Snapshot().Units[0].At),"購入した薬を使用");
                auto end=battle.Snapshot();end.Outcome=BattleOutcome::Won;
                for(int invalid:{-1,2,6}) {
                    auto bad=end;bad.Medicines=invalid;
                    check(!run.CompleteBattle(entry.Revision,bad) && run.Snapshot()==entry,"不正・増殖した残数を拒否");
                }
                check(run.CompleteBattle(entry.Revision,end) && run.Snapshot().Wallet.Medicines==0,"消費数を周回へ戻す");
                const auto after=run.Snapshot();
                check(!run.CompleteBattle(entry.Revision,end) && run.Snapshot()==after,"二重引継ぎ拒否");
                ++used;break;
            }
            auto end=battle.Snapshot();end.Outcome=BattleOutcome::Won;end.Units[0].Hp=1;
            run.CompleteBattle(run.Snapshot().Revision,end);run.ChooseRecipient(run.Snapshot().Revision,0);
            run.ClaimReward(run.Snapshot().Revision,0);
            if(run.Snapshot().PendingRescue>=0)run.ConverseRescue(run.Snapshot().Revision,ConversationChoice::Neutral);
        }
    }
    check(used>0,"購入から実使用まで到達");
    std::cout<<"RunMedicine: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
