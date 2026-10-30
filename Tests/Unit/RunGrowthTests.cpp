#include "Application/Run.h"
#include "Application/Encounter.h"
#include <iostream>
int TestRunGrowth()
{
    using namespace GirlsSpire;
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* label){++checks;if(!ok){++failed;std::cerr<<"FAIL RunGrowth: "<<label<<'\n';}};
    for(int seed=0;seed<100;++seed) {
        Run run(seed,Difficulty::Beginner,HeroRole::Sword);auto repeat=run;
        const auto initial=run.Snapshot();
        check(!run.ChooseRecipient(initial.Revision,0) && !run.ClaimReward(initial.Revision,0) && !run.CompleteBattle(initial.Revision,BattleOutcome::Won),"戦闘前の報酬拒否");
        for(auto* r:{&run,&repeat}) {
            r->ChooseNode(r->Snapshot().Revision,0);
            check(r->CompleteBattle(r->Snapshot().Revision,BattleOutcome::Won),"勝利で報酬開始");
        }
        if(run.Snapshot().Phase!=RunPhase::Reward) {check(false,"報酬状態へ遷移");continue;}
        const auto s=run.Snapshot();const auto rng=run.RandomState();
        check(s.Recruited.size()==1 && s.Rewards.empty(),"救出加入後、受取人未選択では候補非表示");
        check(!run.ChooseNode(s.Revision,0) && !run.CompleteEvent(s.Revision),"未取得で次へ進めない");
        for(int id:{-1,9,999}) check(!run.ChooseRecipient(s.Revision,id) && run.Snapshot()==s && run.RandomState()==rng,"不正人物は不変");
        check(!run.ChooseRecipient(s.Revision+1,0) && !run.ClaimReward(s.Revision,0),"古い入力・受取人なし拒否");
        const int unjoined=s.Recruited[0]==0?2:1;
        check(!run.ChooseRecipient(s.Revision,unjoined) && run.Snapshot()==s,"未加入者には渡せない");
        const int actor=seed%2==0?0:s.Recruited[0]+1;
        for(auto* r:{&run,&repeat}) check(r->ChooseRecipient(r->Snapshot().Revision,actor),"主人公または救出仲間を確定");
        const auto selected=run.Snapshot();const auto random=run.RandomState();
        check(selected.Rewards.size()==3 && selected==repeat.Snapshot() && random==repeat.RandomState(),"抽選再現");
        check(!run.ChooseRecipient(selected.Revision,actor) && run.Snapshot()==selected && run.RandomState()==random,"人物変更・引き直し不可");
        for(int index:{-1,3,999}) check(!run.ClaimReward(selected.Revision,index) && run.Snapshot()==selected,"無効選択拒否");
        const auto reward=selected.Rewards[seed%3];
        check(!run.ClaimReward(selected.Revision+1,0) && run.Snapshot()==selected,"取得時の古い入力拒否");
        check(run.ClaimReward(selected.Revision,seed%3),"報酬取得");
        const auto after=run.Snapshot();
        check(after.Phase==RunPhase::Route && after.RewardRecipient==-1 && after.Rewards.empty(),"報酬を消費し経路へ戻る");
        check(!run.ClaimReward(selected.Revision,seed%3) && run.Snapshot()==after,"二重取得拒否");
        auto expected=GrowthProgress{};TakeReward(expected,reward);
        check(after.Growth[actor]==expected,"選んだ人物だけ強化");
        auto encounter=after;encounter.Phase=RunPhase::Event;encounter.Active={NodeKind::Battle};
        const auto b=Encounter::Create(encounter,0);
        for(const auto& u:b.Snapshot().Units) if(u.Id==actor) {
            check(u.MaxHp==(actor==0?80:40)+expected.Bonuses[0],"次戦へ最大HP反映");
            check(u.Skills[2]==expected.Learned[0] && u.Skills[3]==expected.Learned[1] && u.Skills[4]==expected.Learned[2],"次戦へ技反映");
        }
        Run fresh(seed,Difficulty::Beginner,HeroRole::Sword);
        check(fresh.Snapshot().Growth==initial.Growth,"次周で消失");
        run.ChooseNode(run.Snapshot().Revision,0);run.CompleteEvent(run.Snapshot().Revision,true);
        check(run.Snapshot().Growth==initial.Growth,"敗北で消失");
    }
    Run run(42,Difficulty::Beginner,HeroRole::Sword);run.ChooseNode(0,0);const auto before=run.Snapshot();
    check(!run.CompleteBattle(before.Revision,BattleOutcome::Ongoing) && run.Snapshot()==before,"未決着拒否");
    check(run.CompleteBattle(before.Revision,BattleOutcome::Lost) && run.Snapshot().Phase==RunPhase::Lost,"敗北は報酬なし");
    Run complete(10,Difficulty::Beginner,HeroRole::Sword);
    int battles=0,events=0;
    while(complete.Snapshot().Phase==RunPhase::Route) {
        complete.ChooseNode(complete.Snapshot().Revision,0);
        const auto event=complete.Snapshot();
        check(!complete.CompleteBattle(event.Revision+1,BattleOutcome::Won),"戦闘完了時の古い入力拒否");
        if(complete.CompleteBattle(event.Revision,BattleOutcome::Won)) {
            ++battles;
            check(complete.ChooseRecipient(complete.Snapshot().Revision,0),"各勝利後に受取人選択");
            const auto choices=complete.Snapshot().Rewards;
            check(battles<=3?static_cast<int>(choices[0])>=4:static_cast<int>(choices[0])<4,"周回を通して追加技上限3");
            check(complete.ClaimReward(complete.Snapshot().Revision,0),"各戦闘の報酬確定");
        } else {
            ++events;check(complete.Snapshot()==event,"非戦闘を勝利扱いにしない");
            complete.CompleteEvent(event.Revision);
        }
    }
    check(battles>=6 && events>0 && complete.Snapshot().Phase==RunPhase::Won,"中間・最終ボスを含めクリア");
    check(complete.Snapshot().Growth==Run(10,Difficulty::Beginner,HeroRole::Sword).Snapshot().Growth,"勝利終了で周回成長を消去");
    std::cout<<"RunGrowth: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
