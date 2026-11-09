#include "Application/Encounter.h"
#include <iostream>
#include <stdexcept>

int TestEncounter()
{
    using namespace GirlsSpire;
    int checks=0,failed=0;
    const auto check=[&](bool pass,const char* message){++checks;if(!pass){++failed;std::cerr<<"FAIL Encounter: "<<message<<'\n';}};
    RunSnapshot run; run.Level=Difficulty::Beginner; run.Role=HeroRole::Sword;
    run.Phase=RunPhase::Event; run.Active={NodeKind::Rescue,3};
    for(int difficulty=0;difficulty<3;++difficulty) for(int role=0;role<4;++role) {
        run.Level=static_cast<Difficulty>(difficulty); run.Role=static_cast<HeroRole>(role);
        try {
            GenerationResult generation;
            const auto battle=Encounter::Create(run,42,&generation);
            const auto& state=battle.Snapshot();
            check(state.Units.size()==3,"主人公・敵・救出対象");
            check(state.Units[0].Skills==std::array<bool,SkillCount>{true,true},"主人公に開発用2技");
            check(state.Width>=8 && state.Width<=12 && state.Height>=8 && state.Height<=12,"生成戦場の寸法");
            check(generation.Attempts>=1 && state.Tiles==generation.Layout.Tiles,"生成結果を戦闘に反映");
            check(state.Units[1].MaxHp==22*(80+20*difficulty)/100,"難易度によるHP80/100/120%");
            check(state.Units[0].Range==(role==0?1:role==1?2:role==2?3:5),"武器別射程");
        } catch(const std::exception&) {check(false,"戦闘を生成できる");}
    }
    run.Recruited={0,1,2,4}; run.Day=12;
    for(auto kind : {NodeKind::Battle,NodeKind::Elite,NodeKind::MidBoss,NodeKind::FinalBoss}) {
        run.Active={kind};
        try {
            const auto battle=Encounter::Create(run,42);
            int players=0;for(const auto& u:battle.Snapshot().Units) if(u.Side==Faction::Player)++players;
            check(players==5,"加入者を含む5人で出撃");
            for(const auto& unit:battle.Snapshot().Units)
                check(unit.Skills==std::array<bool,SkillCount>{unit.Id<9 || unit.Role==BattleRole::Oni,unit.Id<9 || unit.Role==BattleRole::Fox},"人物は初期技、鬼武者は強撃、妖狐は応急手当、他の敵と中立は技なし");
        } catch(const std::exception&) {check(false,"ボスと通常戦の生成");}
    }
    const auto reject=[&](const RunSnapshot& invalid) {
        try{(void)Encounter::Create(invalid,42);check(false,"無効な周回入力の拒否");}
        catch(const std::invalid_argument&){check(true,"無効な周回入力の拒否");}
    };
    auto invalid=run;invalid.Phase=RunPhase::Route;reject(invalid);
    invalid=run;invalid.Active={NodeKind::Shop};reject(invalid);
    invalid=run;invalid.Recruited={0,0};reject(invalid);
    invalid=run;invalid.Active={NodeKind::Rescue,0};reject(invalid);
    invalid=run;invalid.Active={NodeKind::Rescue,-1};reject(invalid);
    invalid=run;invalid.Role=static_cast<HeroRole>(9);reject(invalid);
    invalid=run;invalid.Level=static_cast<Difficulty>(9);reject(invalid);
    invalid=run;invalid.Day=13;reject(invalid);
    GenerationResult fallback;
    const auto fallbackBattle=Encounter::Create(run,42,&fallback,true);
    check(fallback.UsedFallback && fallback.Attempts==8 && fallbackBattle.Snapshot().Width==12,"8回失敗後の固定戦場を戦闘へ接続");
    for(int seed=0;seed<100;++seed) {
        for(int day : {1,5,9,12}) {
            auto daily=run; daily.Day=day; daily.Active={day==12?NodeKind::FinalBoss:NodeKind::Battle};
            daily.Recruited=day==1?std::vector<int>{}:std::vector<int>{0,1,2,4};
            const auto first=Encounter::Create(daily,seed),second=Encounter::Create(daily,seed);
            check(first.Snapshot()==second.Snapshot(),"100シードの編成・敵数・ボス戦場再現");
            for(const auto& u:first.Snapshot().Units)
                if(u.Side==Faction::Player) check(first.PotentialEnemyDamage(u.Id)<u.Hp,"全編成員の初期安全性");
        }
    }
    // FR-022: 精鋭戦の一部は「一定ターン生存」で勝利する。他の種別は従来の目的を保つ。
    {
        int surviveCount=0,eliminateCount=0;
        for(int seed=0;seed<100;++seed) for(int day=1;day<=11;++day) {
            auto elite=run; elite.Day=day; elite.Recruited={0,1,2,4}; elite.Active={NodeKind::Elite};
            const auto battle=Encounter::Create(elite,seed);
            const auto goal=battle.Checkpoint().Goal;
            check(goal==BattleGoal::Survive || goal==BattleGoal::Eliminate,"精鋭戦の目的は生存か全滅");
            check((goal==BattleGoal::Survive)==(day%2==0),"精鋭戦の生存化は偶数日で決まる");
            check(battle.Checkpoint().SurvivalRounds==3,"生存戦は既定の3ラウンド");
            (goal==BattleGoal::Survive?surviveCount:eliminateCount)++;
            check(Encounter::Create(elite,seed).Checkpoint()==battle.Checkpoint(),"精鋭戦の目的決定が再現");
            for(auto kind:{NodeKind::Battle,NodeKind::MidBoss,NodeKind::FinalBoss}) {
                auto other=elite; other.Active={kind}; if(kind==NodeKind::MidBoss)other.Day=6; if(kind==NodeKind::FinalBoss)other.Day=12;
                check(Encounter::Create(other,seed).Checkpoint().Goal!=BattleGoal::Survive,"精鋭以外は生存戦にならない");
            }
        }
        check(surviveCount>0 && eliminateCount>0,"精鋭戦に生存と全滅の両方が出る");
    }
    std::cout<<"Encounter: "<<checks<<" checks, "<<failed<<" failed\n";
    return failed;
}
