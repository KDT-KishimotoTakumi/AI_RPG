#include "Application/Run.h"
#include "Application/Encounter.h"
#include <iostream>
int TestRunHealth()
{
    using namespace GirlsSpire;
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* label){++checks;if(!ok){++failed;std::cerr<<"FAIL RunHealth: "<<label<<'\n';}};
    const auto claim=[](Run& run) {
        run.ChooseRecipient(run.Snapshot().Revision,0);
        run.ClaimReward(run.Snapshot().Revision,0);
        if(run.Snapshot().PendingRescue>=0)run.ConverseRescue(run.Snapshot().Revision,ConversationChoice::Neutral);
    };
    const auto battle=[](Run& run) {
        for(int i=0;i<24 && run.Snapshot().Phase==RunPhase::Route;++i) {
            run.ChooseNode(run.Snapshot().Revision,0);
            const auto kind=run.Snapshot().Active.Kind;
            if(kind==NodeKind::Battle || kind==NodeKind::Elite || kind==NodeKind::Rescue || kind==NodeKind::MidBoss || kind==NodeKind::FinalBoss) return true;
            run.CompleteEvent(run.Snapshot().Revision);
        }
        return false;
    };
    for(int seed=0;seed<20;++seed) {
        Run run(seed,Difficulty::Beginner,HeroRole::Sword);
        check(run.CurrentHp(0)==80 && run.MaxHp(0)==80 && run.CurrentHp(1)==0 && run.MaxHp(-1)==0,"初期HPと未加入拒否");
        check(!run.Rest(0) && battle(run),"戦闘準備");
        auto end=Encounter::Create(run.Snapshot(),0).Snapshot();end.Outcome=BattleOutcome::Won;end.Units[0].Hp=1;
        const auto saved=run.Snapshot();
        check(!run.CompleteBattle(saved.Revision+1,end) && run.Snapshot()==saved,"古い結果拒否");
        auto invalid=end;invalid.Units.erase(invalid.Units.begin());
        check(!run.CompleteBattle(saved.Revision,invalid) && run.Snapshot()==saved,"参加者欠落拒否");
        invalid=end;invalid.Units[0].MaxHp=81;
        check(!run.CompleteBattle(saved.Revision,invalid) && run.Snapshot()==saved,"最大HP改変拒否");
        invalid=end;invalid.Units[0].Hp=81;
        check(!run.CompleteBattle(saved.Revision,invalid) && run.Snapshot()==saved,"HP超過拒否");
        for(int id:{-1,9,100}) {
            invalid=end;invalid.Units[0].Id=id;
            check(!run.CompleteBattle(saved.Revision,invalid) && run.Snapshot()==saved,"所属外の人物拒否");
        }
        invalid=end;invalid.Units.push_back(invalid.Units[0]);
        check(!run.CompleteBattle(saved.Revision,invalid) && run.Snapshot()==saved,"参加者重複拒否");
        invalid=end;invalid.Units[0].Hp=-1;
        check(!run.CompleteBattle(saved.Revision,invalid) && run.Snapshot()==saved,"負のHP拒否");
        invalid=end;invalid.Units[0].Hp=0;
        check(!run.CompleteBattle(saved.Revision,invalid) && run.Snapshot()==saved,"全滅なのに勝利は拒否");
        for(auto outcome:{BattleOutcome::Lost,BattleOutcome::Ongoing}) {
            invalid=end;invalid.Outcome=outcome;
            check(!run.CompleteBattle(saved.Revision,invalid) && run.Snapshot()==saved,"不整合な決着拒否");
        }
        check(run.CompleteBattle(saved.Revision,end),"実戦結果反映");
        if(run.Snapshot().Phase!=RunPhase::Reward) continue;
        check(run.CurrentHp(0)==17 && run.Snapshot().Health[0].Wounds==0,"1から17へ回復");
        const auto after=run.Snapshot();check(!run.CompleteBattle(saved.Revision,end) && run.Snapshot()==after,"回復二重適用拒否");
        claim(run);check(battle(run),"次戦準備");
        end=Encounter::Create(run.Snapshot(),0).Snapshot();
        check(end.Units[0].Hp==17,"次戦へHP引継ぎ");
        end.Units[0].Hp=0;end.Outcome=BattleOutcome::Won;
        check(run.CompleteBattle(run.Snapshot().Revision,end) && run.CurrentHp(0)==38 && run.MaxHp(0)==75,"Lv2成長後に負傷して復帰");
        claim(run);check(battle(run),"負傷後の次戦");
        const auto next=Encounter::Create(run.Snapshot(),0).Snapshot();
        check(next.Units[0].Hp==38 && next.Units[0].MaxHp==75,"成長・負傷とHPを次戦へ");
        end=next;end.Outcome=BattleOutcome::Lost;for(auto& unit:end.Units) if(unit.Side==Faction::Player) unit.Hp=0;
        check(run.CompleteBattle(run.Snapshot().Revision,end) && run.Snapshot().Health==RunSnapshot{}.Health,"全滅時消去");
    }
    for(bool treat:{false,true}) {
        Run run(42,Difficulty::Beginner,HeroRole::Sword);
        // 仲間がいる勝利結果を準備し、以降は周回APIを通して休息を選ぶ。
        battle(run);run.CompleteEvent(run.Snapshot().Revision);
        bool rested=false;
        for(int n=0;n<24 && run.Snapshot().Phase==RunPhase::Route;++n) {
            int index=0;bool rest=false;
            for(int i=0;i<static_cast<int>(run.Snapshot().Choices.size());++i) if(run.Snapshot().Choices[i].Kind==NodeKind::Rest) {index=i;rest=true;break;}
            run.ChooseNode(run.Snapshot().Revision,index);
            if(rest) {
                const auto saved=run.Snapshot();
                check(!run.Rest(saved.Revision+1) && !run.Rest(saved.Revision,9) && !run.Rest(saved.Revision,-2) && run.Snapshot()==saved,"休息の古い入力・無効人物拒否");
                const int ally=saved.Recruited[0]+1;
                check(!run.Rest(saved.Revision,ally) && run.Snapshot()==saved,"負傷なしの治療拒否");
                if(treat && saved.Health[0].Wounds==0) {run.CompleteEvent(saved.Revision);continue;}
                const int hp=run.CurrentHp(0),max=run.MaxHp(0),wounds=saved.Health[0].Wounds;
                check(run.Rest(saved.Revision,treat?0:-1),"休息確定");
                check(treat?(run.CurrentHp(0)==hp && run.Snapshot().Health[0].Wounds==wounds-1):run.CurrentHp(0)==std::min(max,hp+(max+1)/2),"休息効果");
                check(!run.Rest(saved.Revision,treat?0:-1),"休息二重消費不可");rested=true;break;
            }
            try {
                auto end=Encounter::Create(run.Snapshot(),0).Snapshot();end.Outcome=BattleOutcome::Won;end.Units[0].Hp=0;
                if(run.CompleteBattle(run.Snapshot().Revision,end)) claim(run); else break;
            } catch(const std::invalid_argument&) {run.CompleteEvent(run.Snapshot().Revision);}
        }
        check(rested,"休息に到達して実行");
    }
    int hpRewards=0;
    for(int seed=0;seed<20;++seed) {
        Run run(seed,Difficulty::Beginner,HeroRole::Sword);battle(run);
        auto end=Encounter::Create(run.Snapshot(),0).Snapshot();end.Outcome=BattleOutcome::Won;end.Units[0].Hp=1;
        run.CompleteBattle(run.Snapshot().Revision,end);run.ChooseRecipient(run.Snapshot().Revision,0);
        for(int i=0;i<3;++i) if(run.Snapshot().Rewards[i]==Reward::MaxHp) {
            ++hpRewards;check(run.ClaimReward(run.Snapshot().Revision,i) && run.CurrentHp(0)==27 && run.MaxHp(0)==90,"最大HP報酬で現在HPも10加算");
            if(run.Snapshot().PendingRescue>=0)run.ConverseRescue(run.Snapshot().Revision,ConversationChoice::Neutral);
            battle(run);const auto next=Encounter::Create(run.Snapshot(),0).Snapshot();
            check(next.Units[0].Hp==27 && next.Units[0].MaxHp==90,"次戦で報酬を二重加算しない");break;
        }
    }
    check(hpRewards>0,"最大HP報酬を実測");
    RunSnapshot wounded{};wounded.Phase=RunPhase::Event;wounded.Recruited={0};wounded.Health[0]={1,4};wounded.Health[1]={1,3};
    for(int seed=0;seed<100;++seed) {
        bool safe=false;
        try {
            const auto field=Encounter::Create(wounded,seed);
            safe=field.Snapshot().Units[0].Hp==1 && field.Snapshot().Units[1].MaxHp==10 && field.PotentialEnemyDamage(0)==0 && field.PotentialEnemyDamage(1)==0;
        } catch(const std::invalid_argument&) {}
        check(safe,"低HPを維持して安全な次戦を生成");
    }
    RunSnapshot invalidRun{};invalidRun.Phase=RunPhase::Event;
    GenerationResult fallback;
    const auto safeFallback=Encounter::Create(wounded,0,&fallback,true);
    check(fallback.UsedFallback && safeFallback.Snapshot().Units[0].Hp==1 && safeFallback.PotentialEnemyDamage(0)==0,"低HPでも固定戦場へ安全に退避");
    for(int hp:{-2,0,81}) {
        invalidRun.Health[0].Hp=hp;bool threw=false;
        try{Encounter::Create(invalidRun,0);}catch(const std::invalid_argument&){threw=true;}
        check(threw,"不正HPで戦闘生成しない");
    }
    std::cout<<"RunHealth: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
