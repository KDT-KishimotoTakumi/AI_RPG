#include "Application/Encounter.h"
#include <algorithm>
#include <iostream>
using namespace GirlsSpire;
int TestRunLevel()
{
    int checks=0,failed=0;std::array<bool,9> visited{};
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL RunLevel: "<<why<<'\n';}};
    bool capped=false;
    for(int seed=0;seed<100;++seed) {
        Run run(seed,static_cast<Difficulty>(seed%3),static_cast<HeroRole>(seed%4));
        check(run.Snapshot().Levels[0]==LevelProgress{},"初期レベル1・経験値0");
        for(int step=0;step<24 && run.Snapshot().Phase==RunPhase::Route;++step) {
            int node=0;
            for(int i=0;i<static_cast<int>(run.Snapshot().Choices.size());++i)
                if(run.Snapshot().Choices[i].Kind==NodeKind::Elite) node=i;
            run.ChooseNode(run.Snapshot().Revision,node);
            const auto old=run.Snapshot();const auto kind=old.Active.Kind;
            visited[static_cast<int>(kind)]=true;
            const int amount=kind==NodeKind::Battle || kind==NodeKind::Rescue?100:kind==NodeKind::Elite?150:kind==NodeKind::MidBoss || kind==NodeKind::FinalBoss?200:0;
            if(amount==0) {
                if(kind==NodeKind::Rest) {
                    const int max=run.MaxHp(0),hp=run.CurrentHp(0);
                    check(run.Rest(old.Revision) && run.CurrentHp(0)==std::min(max,hp+(max+1)/2),"成長後の最大HPで休息回復");
                } else run.CompleteEvent(old.Revision);
                check(run.Snapshot().Levels==old.Levels,"非戦闘は経験値なし");continue;
            }
            auto end=Encounter::Create(old,seed).Snapshot();end.Outcome=BattleOutcome::Won;
            for(auto& u:end.Units) if(u.Side==Faction::Player) {
                const int lv=old.Levels[u.Id].Level;
                check(u.MaxHp==std::max(10,(u.Id==0?80:40)+old.Growth[u.Id].Bonuses[0]+5*(lv-1)-10*old.Health[u.Id].Wounds),"次戦の最大HP");
                check(u.Attack==(u.Id==0?18:12)+old.Growth[u.Id].Bonuses[1]+lv-1 && u.Defense==2+old.Growth[u.Id].Bonuses[2]+lv-1,"次戦の攻撃防御を一度だけ加算");
                u.Hp=u.Id==0 && !old.Recruited.empty() && step%2==0?0:1;
            }
            check(!run.CompleteBattle(old.Revision+1,end) && run.Snapshot()==old,"古い結果で経験値を付与しない");
            check(run.CompleteBattle(old.Revision,end),"経験値付き勝利");
            const auto after=run.Snapshot();
            check(!run.CompleteBattle(old.Revision,end) && run.Snapshot()==after,"二重付与しない");
            for(const auto& u:end.Units) if(u.Side==Faction::Player) {
                const int total=(old.Levels[u.Id].Level-1)*200+old.Levels[u.Id].Experience+amount;
                const int level=std::min(10,1+total/200),xp=level==10?0:total%200;
                capped|=level==10;
                check(after.Levels[u.Id]==LevelProgress{level,xp},"全員同量・上限・余剰");
                const int wounds=std::min(4,old.Health[u.Id].Wounds+(u.Hp==0?1:0));
                const int max=std::max(10,(u.Id==0?80:40)+old.Growth[u.Id].Bonuses[0]+5*(level-1)-10*wounds);
                check(run.MaxHp(u.Id)==max && run.CurrentHp(u.Id)==(u.Hp==0?(max+1)/2:std::min(max,1+(max+4)/5)),"成長→負傷→回復、追加5回復なし");
            }
            if(kind==NodeKind::Rescue) {
                const int id=old.Active.Candidate+1;
                check(after.Levels[id]==LevelProgress{after.Levels[0].Level,0},"加入は成長後の主人公レベル・経験値0");
                check(run.CurrentHp(id)==run.MaxHp(id) && after.Health[id].Wounds==0,"加入時の全快を維持");
            }
            // 同じ決着を再生しても経験値・抽選乱数を余分に消費しない。
            auto copy=run;run.ChooseRecipient(after.Revision,0);copy.ChooseRecipient(after.Revision,0);
            check(run.Snapshot()==copy.Snapshot() && run.RandomState()==copy.RandomState(),"固定シード再現");
            const auto lv=run.Snapshot().Levels;
            run.ClaimReward(run.Snapshot().Revision,0);
            if(run.Snapshot().PendingRescue>=0)run.ConverseRescue(run.Snapshot().Revision,ConversationChoice::Neutral);
            check(run.Snapshot().Levels==(kind==NodeKind::FinalBoss?RunSnapshot{}.Levels:lv),"三択は維持、勝利終了で消去");
        }
        check(run.Snapshot().Phase==RunPhase::Won,"12日完走");
    }
    check(capped,"実際の経路で上限10へ到達");
    for(bool seen:visited) check(seen,"全ノード種を検証");
    Run lost(42,Difficulty::Beginner,HeroRole::Sword);
    lost.ChooseNode(0,0);auto end=Encounter::Create(lost.Snapshot(),0).Snapshot();end.Outcome=BattleOutcome::Won;
    lost.CompleteBattle(lost.Snapshot().Revision,end);lost.ChooseRecipient(lost.Snapshot().Revision,0);lost.ClaimReward(lost.Snapshot().Revision,0);
    if(lost.Snapshot().PendingRescue>=0)lost.ConverseRescue(lost.Snapshot().Revision,ConversationChoice::Neutral);
    lost.ChooseNode(lost.Snapshot().Revision,0);lost.CompleteEvent(lost.Snapshot().Revision,true);
    check(lost.Snapshot().Levels==RunSnapshot{}.Levels,"敗北終了で経験値消去");
    std::cout<<"RunLevel: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
