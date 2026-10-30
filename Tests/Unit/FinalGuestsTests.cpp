#include "Application/Encounter.h"
#include "Infrastructure/SavePayload.h"
#include <bit>
#include <algorithm>
#include <iostream>
#include <set>
using namespace GirlsSpire;
int TestFinalGuests()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL FinalGuests: "<<why<<'\n';}};
    std::set<int> seenEnemy,seenNeutral;
    for(unsigned seed=0;seed<32;++seed) {
        RunSnapshot run;run.Day=12;run.Phase=RunPhase::Event;run.Active={NodeKind::FinalBoss};
        for(int i=0;i<4;++i)run.Recruited.push_back((seed+i)%8);
        const auto battle=Encounter::Create(run,seed);const auto same=Encounter::Create(run,seed);
        check(battle.Checkpoint()==same.Checkpoint(),"同じシードと編成から全状態を再現");
        std::set<int> found;
        for(const auto& unit:battle.Snapshot().Units)if(unit.Id>=1 && unit.Id<=8) {
            const bool joined=std::find(run.Recruited.begin(),run.Recruited.end(),unit.Id-1)!=run.Recruited.end();
            if(joined)check(unit.Side==Faction::Player,"加入者の陣営を維持");
            else {
                found.insert(unit.Id-1);check(unit.Side==Faction::Enemy || unit.Side==Faction::Neutral,"未救出者の陣営は敵か中立");
                if(unit.Side==Faction::Enemy)seenEnemy.insert(unit.Id);else seenNeutral.insert(unit.Id);
                check(unit.Skills[0] && unit.Skills[1],"加入時と同じ初期技能を保持");
                check((std::find(battle.Snapshot().Order.begin(),battle.Snapshot().Order.end(),unit.Id)!=battle.Snapshot().Order.end())==(unit.Side==Faction::Enemy),"中立者は手番を持たず敵は手番を持つ");
            }
        }
        check(found.size()==4 && battle.Snapshot().Units.size()==10,"主人公＋加入4人＋未救出4人＋城主が登場");
        auto restored=Battle::Restore(battle.Checkpoint());check(restored && restored->Checkpoint()==battle.Checkpoint(),"再開で陣営と行動順を変えない");
        RunSaveState save{{run,seed,RunPhase::Route},SavedCombat{battle.Checkpoint(),CombatControl{}.Checkpoint()}};
        const auto payload=EncodeRunPayload(save);const auto loaded=payload?DecodeRunPayload(*payload):std::nullopt;
        check(loaded && *loaded==save,"未救出者を含む戦闘全体の保存復元");
    }
    check(seenEnemy.size()==8 && seenNeutral.size()==8,"全8人物がシードにより敵と中立の両方で再登場");
    for(unsigned mask=0;mask<256;++mask)if(std::popcount(mask)==4) {
        RunSnapshot run;run.Day=12;run.Phase=RunPhase::Event;run.Active={NodeKind::FinalBoss};
        for(int i=0;i<8;++i)if(mask&(1u<<i))run.Recruited.push_back(i);
        for(int i=0;i<9;++i)run.Health[i].Hp=1;
        GenerationResult details;const auto battle=Encounter::Create(run,mask,&details,true);
        check(details.UsedFallback && battle.Snapshot().Units.size()==10,"全70編成を固定戦場でも配置できる");
        for(const auto& unit:battle.Snapshot().Units)if(unit.Side==Faction::Player)
            check(battle.PotentialEnemyDamage(unit.Id)==0,"瀕死の全編成員を初手攻撃圏外へ配置");
    }
    std::cout<<"FinalGuests: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
