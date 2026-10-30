#include "AI/BasicTurn.h"
#include "Application/Encounter.h"
#include "Infrastructure/SavePayload.h"
#include "../Fixtures/LegacyRun10Combat.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace GirlsSpire;
int TestFinalBoss()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL FinalBoss: "<<why<<'\n';}};
    const auto lordRole=static_cast<BattleRole>(11);const auto goal=static_cast<BattleGoal>(3);
    const auto legacyFrame=DecodeSave(std::string_view(LegacyRun10Combat,sizeof(LegacyRun10Combat)-1),SaveKind::Run);
    const auto legacy=legacyFrame.Value?DecodeRunPayload(legacyFrame.Value->Payload):std::nullopt;
    check(legacy && legacy->Combat && std::any_of(legacy->Combat->Battle.State.Units.begin(),legacy->Combat->Battle.State.Units.end(),[](const BattleUnit& unit){return unit.Role==BattleRole::FallenPrincess;}),"旧v10の落城姫を保持して読込み");
    const auto migrated=legacy?EncodeRunPayload(*legacy):std::nullopt;
    check(migrated && DecodeRunPayload(*migrated)==legacy,"旧v10から新形式へ全状態を維持");
    for(unsigned seed=1;seed<=50;++seed)try {
        BattleUnit hero;hero.At={1,1};hero.Hp=hero.MaxHp=5000;hero.Speed=20;
        BattleUnit lord;lord.Id=100;lord.Side=Faction::Enemy;lord.Role=lordRole;lord.At={5,1};lord.Hp=lord.MaxHp=240;lord.Speed=15;lord.Range=3;lord.Movement=2;lord.Attack=22;lord.Defense=6;
        Battle battle(8,8,std::vector<Tile>(64),{hero,lord},seed,goal);
        for(int round=1;round<=12;++round) {
            int steps=0;
            while((battle.Snapshot().Round!=round || battle.Snapshot().ActiveId!=100) && steps++<100)
                if(!battle.EndTurn(battle.Snapshot().Revision,battle.Snapshot().ActiveId,Facing::South))break;
            const auto& state=battle.Snapshot();
            const auto count=std::count_if(state.Units.begin(),state.Units.end(),[](const BattleUnit& unit){return unit.Side==Faction::Enemy && unit.Id!=100 && unit.Hp>0;});
            check(state.Round==round && state.ActiveId==100 && count==std::min(round/3,3),"3の倍数ラウンドに1体、存命最大3体を召喚");
            check(!state.Acted && !state.Moved,"召喚で通常の行動・移動を消費しない");
            const auto restored=Battle::Restore(battle.Checkpoint());check(restored && restored->Checkpoint()==battle.Checkpoint(),"召喚後の行動順と状態を復元");
        }
        hero.Attack=1000;hero.Range=8;hero.Accuracy=200;lord.Hp=20;
        BattleUnit soldier;soldier.Id=1000;soldier.Side=Faction::Enemy;soldier.At={6,6};
        Battle victory(8,8,std::vector<Tile>(64),{hero,lord,soldier},seed,goal);
        check(victory.Attack(0,0,lord.At),"城主を攻撃");
        const bool killed=victory.Snapshot().Units[1].Hp==0;
        check(victory.Snapshot().Outcome==(killed?BattleOutcome::Won:BattleOutcome::Ongoing) && victory.Snapshot().Units[2].Hp==soldier.Hp,"配下が残っても城主撃破で勝利");
    }catch(const std::invalid_argument&){check(false,"城主と専用の撃破目標を作成できる");}
    for(int percent:{80,100,120}) {
        BattleUnit hero;hero.At={1,1};hero.Hp=hero.MaxHp=5000;hero.Speed=20;
        BattleUnit lord;lord.Id=100;lord.Side=Faction::Enemy;lord.Role=lordRole;lord.At={5,1};lord.Speed=15;
        Battle setup(8,8,std::vector<Tile>(64),{hero,lord},42,goal,3,0,percent);
        int steps=0;while((setup.Snapshot().Round!=3 || setup.Snapshot().ActiveId!=100) && steps++<30)setup.EndTurn(setup.Snapshot().Revision,setup.Snapshot().ActiveId,Facing::South);
        const auto summoned=setup.Checkpoint();
        Run progress(42,Difficulty::Intermediate,HeroRole::Sword);auto runState=progress.Checkpoint();
        runState.State.Day=12;runState.State.Phase=RunPhase::Event;runState.State.Active={NodeKind::FinalBoss};runState.State.Choices.clear();
        RunSaveState saved{runState,SavedCombat{summoned,CombatControl{}.Checkpoint()}};
        const auto payload=EncodeRunPayload(saved);const auto decoded=payload?DecodeRunPayload(*payload):std::nullopt;
        check(decoded && *decoded==saved,"城主・召喚兵・行動順を新形式で保存復元");
        if(payload){auto forged=*payload;forged[4]=10;check(!DecodeRunPayload(forged),"城主の新状態を旧v10として偽装できない");}
        check(summoned.State.Units.size()==3 && summoned.State.Units.back().SummonerId==100 && summoned.State.Units.back().Hp==22*percent/100,"召喚兵に難易度HP補正と所有者を保持");
        check(summoned.RandomState==42 && summoned.State.ActiveId==100,"召喚で乱数と手番を消費しない");
        check(ApplyBasicAction(setup,ChooseBasicAction(setup)),"召喚直後に城主が通常行動できる");
        auto invalid=summoned;invalid.State.Units.back().SummonerId=9999;check(!Battle::Restore(invalid),"不明な召喚主を拒否");
        invalid=summoned;invalid.EnemyHpPercent=99;check(!Battle::Restore(invalid),"不正な難易度補正を拒否");
        invalid=summoned;invalid.State.Units[1].Side=Faction::Player;check(!Battle::Restore(invalid),"プレイヤー側の城主を拒否");
        invalid=summoned;invalid.State.Units.back().Role=BattleRole::Oni;check(!Battle::Restore(invalid),"足軽以外の召喚兵を拒否");
        auto retry=summoned;retry.State.Units.back().Hp=0;retry.State.Round=6;retry.State.Combos.Round=6;
        // 再開では手番開始効果を再発動しない。次の周期で死亡スロットを再利用する。
        auto revived=Battle::Restore(retry);check(revived.has_value(),"召喚兵死亡後を復元");
        if(revived) {
            int guard=0;while((revived->Snapshot().Round!=9 || revived->Snapshot().ActiveId!=100) && guard++<40)revived->EndTurn(revived->Snapshot().Revision,revived->Snapshot().ActiveId,Facing::South);
            check(revived->Snapshot().Units.size()==3 && revived->Snapshot().Units.back().Id==summoned.State.Units.back().Id && revived->Snapshot().Units.back().Hp>0,"死亡した召喚兵のスロットを再利用");
        }
        auto blocked=Battle(8,8,std::vector<Tile>(64),{hero,lord},42,goal).Checkpoint();
        blocked.State.Round=3;blocked.State.Combos.Round=3;
        for(Position at:std::vector<Position>{{5,0},{6,1},{5,2},{4,1}})blocked.State.Tiles[at.Y*8+at.X].Wall=true;
        auto closed=Battle::Restore(blocked);check(closed.has_value(),"隣接配置不可の状態を復元");
        if(closed) {closed->EndTurn(closed->Snapshot().Revision,0,Facing::South);check(closed->Snapshot().Units.size()==2 && closed->Snapshot().ActiveId==100,"配置不可でも召喚せず城主の通常手番を維持");}
    }
    for(int difficulty=0;difficulty<3;++difficulty)for(unsigned seed=1;seed<=8;++seed) {
        RunSnapshot run;run.Day=12;run.Phase=RunPhase::Event;run.Active={NodeKind::FinalBoss};run.Recruited={0,1,2,3};run.Level=static_cast<Difficulty>(difficulty);
        const auto battle=Encounter::Create(run,seed);
        const auto& boss=battle.Snapshot().Units.back();
        check(boss.Role==lordRole && boss.MaxHp==240*(80+20*difficulty)/100 && boss.Attack==22 && boss.Defense==6 && boss.Movement==2 && boss.Range==3 && boss.Speed==15 && battle.Checkpoint().Goal==goal,"12日目に承認済みの城主能力と勝利条件を接続");
    }
    std::cout<<"FinalBoss: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
