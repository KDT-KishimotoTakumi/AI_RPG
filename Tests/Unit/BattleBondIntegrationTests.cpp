#include "Application/Encounter.h"
#include <algorithm>
#include <iostream>
using namespace GirlsSpire;
int TestBattleBondIntegration()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL BattleBondIntegration: "<<why<<'\n';}};
    for(int seed=0;seed<100;++seed) {
        BattleUnit hero;hero.Id=0;hero.At={1,1};hero.Speed=40;
        BattleUnit ally;ally.Id=1;ally.At={2,1};ally.Speed=30;ally.Hp=1;ally.Range=8;ally.Skills.fill(true);
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={6,1};enemy.Hp=enemy.MaxHp=1000;
        Battle battle(8,8,std::vector<Tile>(64),{hero,ally,enemy},seed,BattleGoal::Eliminate,3,1);
        const auto before=battle.Snapshot();
        battle.PreviewMedicine(0,ally.At);
        check(battle.Snapshot()==before,"予告不変");
        check(!battle.UseMedicine(1,0,ally.At) && battle.Snapshot()==before,"古い援護拒否");
        check(battle.UseMedicine(0,0,ally.At),"他者へ傷薬");
        check(battle.Snapshot().BondGains.Aid[0]==1,"傷薬の援護");
        battle.EndTurn(battle.Snapshot().Revision,0,Facing::South);
        battle.Attack(battle.Snapshot().Revision,1,enemy.At);
        check(battle.Snapshot().BondGains.Combo[0]==(battle.LastHits()[0].Hit?1:0),"実連携の命中時だけ加算");
        auto copy=battle;check(copy.Snapshot()==battle.Snapshot(),"集計コピー");
    }
    for(auto skill:{Skill::FirstAid,Skill::Heal}) for(int actor:{0,1,2}) for(int target:{0,1,2}) {
        std::vector<BattleUnit> units;
        for(int id=0;id<3;++id) {BattleUnit u;u.Id=id;u.At={id,1};u.Speed=id==actor?50:10;u.Hp=1;u.Skills.fill(true);units.push_back(u);}
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={7,7};units.push_back(enemy);
        Battle battle(8,8,std::vector<Tile>(64),units,0);
        check(battle.UseSkill(0,actor,skill,units[target].At),"回復技");
        for(int id=1;id<=2;++id) check(battle.Snapshot().BondGains.Aid[id-1]==(actor!=target && (id==actor || id==target)?1:0),"回復技双方・自己除外");
    }
    for(int seed=0;seed<100;++seed) {
        Profile profile;profile.Bonds.fill(seed);
        Run run(seed,Difficulty::Beginner,HeroRole::Sword,profile);
        run.ChooseNode(run.Snapshot().Revision,0);run.CompleteBattle(run.Snapshot().Revision,BattleOutcome::Won);
        run.ChooseRecipient(run.Snapshot().Revision,0);run.ClaimReward(run.Snapshot().Revision,0);
        while(run.Snapshot().Phase==RunPhase::Route) {
            run.ChooseNode(run.Snapshot().Revision,0);
            const auto kind=run.Snapshot().Active.Kind;
            if(kind==NodeKind::Battle || kind==NodeKind::Elite || kind==NodeKind::Rescue || kind==NodeKind::MidBoss || kind==NodeKind::FinalBoss) break;
            run.CompleteEvent(run.Snapshot().Revision);
        }
        const auto entry=run.Snapshot();const int id=entry.Recruited.front()+1;
        auto end=Encounter::Create(entry,seed).Snapshot();end.BondGains.Combo[id-1]=5;end.BondGains.Aid[id-1]=3;
        for(auto outcome:{BattleOutcome::Won,BattleOutcome::Lost}) {
            auto fork=run;auto result=end;result.Outcome=outcome;
            if(outcome==BattleOutcome::Lost) for(auto& unit:result.Units) if(unit.Side==Faction::Player) unit.Hp=0;
            auto bad=result;bad.BondGains.Aid[id-1]=4;
            check(!fork.CompleteBattle(entry.Revision,bad) && fork.Snapshot()==entry && fork.ProfileState()==profile,"破損集計拒否");
            for(int other=1;other<=8;++other) if(other!=id) {
                bad=result;bad.BondGains.Combo[other-1]=1;
                check(!fork.CompleteBattle(entry.Revision,bad) && fork.ProfileState()==profile,"未参加人物拒否");
            }
            check(!fork.CompleteBattle(entry.Revision+1,result) && fork.ProfileState()==profile,"古い結果拒否");
            check(fork.CompleteBattle(entry.Revision,result),"勝敗両方確定");
            auto expected=profile;expected.Bonds[id-1]=std::min(100,seed+8);
            check(fork.ProfileState()==expected,"勝敗両方の獲得保持");
            check(!fork.CompleteBattle(entry.Revision,result) && fork.ProfileState()==expected,"二重加算拒否");
            Run next(seed,Difficulty::Beginner,HeroRole::Sword,fork.ProfileState());
            check(next.ProfileState()==expected,"次周へ保持");
        }
    }
    std::cout<<"BattleBondIntegration: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
