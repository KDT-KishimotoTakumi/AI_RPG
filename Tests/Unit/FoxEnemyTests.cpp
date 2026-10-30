#include "AI/BasicTurn.h"
#include "Application/Encounter.h"
#include "Infrastructure/SavePayload.h"
#include "../Fixtures/LegacyRun7Combat.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace GirlsSpire;
int TestFoxEnemy()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL FoxEnemy: "<<why<<'\n';}};
    const auto foxRole=static_cast<BattleRole>(8);
    for(unsigned seed=1;seed<=100;++seed)for(int mode=0;mode<11;++mode)try {
        BattleUnit hero;hero.At={1,1};hero.Hp=hero.MaxHp=200;hero.Speed=1;
        BattleUnit fox;fox.Id=100;fox.Side=Faction::Enemy;fox.Role=foxRole;fox.At={2,1};fox.Speed=20;fox.Hp=50;fox.MaxHp=100;fox.Attack=8;fox.Range=3;fox.Skills[1]=true;
        BattleUnit ally;ally.Id=101;ally.Side=Faction::Enemy;ally.At={2,2};ally.Speed=0;ally.Hp=20;ally.MaxHp=100;
        if(mode==1){fox.Hp=20;ally.Hp=50;}
        if(mode==2){fox.Hp=51;ally.Hp=51;}
        if(mode==3){fox.Hp=100;ally.Hp=0;}
        if(mode==4){fox.Hp=100;ally.At={4,1};}
        if(mode==5){fox.Hp=100;ally.At={6,1};}
        if(mode==8){fox.Hp=40;fox.MaxHp=200;}
        if(mode==9){fox.Hp=100;ally.Hp=15;ally.MaxHp=30;}
        if(mode==10){fox.Hp=40;fox.MaxHp=200;ally.Hp=30;}
        std::vector<Tile> tiles(64);if(mode==4)tiles[8+3].Wall=true;
        Battle battle(8,8,tiles,{hero,fox,ally},seed);
        if(mode==6 || mode==7){auto saved=battle.Checkpoint();if(mode==6)saved.State.Units[1].Cooldowns[1]=2;else saved.State.Units[1].Skills[1]=false;battle=*Battle::Restore(saved);}
        const auto before=battle.Checkpoint();const auto action=ChooseBasicAction(battle);
        check(battle.Checkpoint()==before && ChooseBasicAction(battle)==action,"判断で状態/乱数を変更しない");
        const bool heal=mode==0 || mode==1 || mode>=8;
        check(action.Kind==(heal?BasicActionKind::UseSkill:BasicActionKind::Attack),"合法な回復がある場合だけ射撃より優先");
        const int recipient=mode==0 || mode==9?2:1;
        check(action.Target==(heal?before.State.Units[recipient].At:hero.At),"HP割合を比較し同率は人物ID順で選ぶ");
        check(ApplyBasicAction(battle,action),"選択を実行");
        if(heal) {
            check(action.Ability==Skill::FirstAid && battle.Snapshot().Units[recipient].Hp==std::min(before.State.Units[recipient].MaxHp,before.State.Units[recipient].Hp+20),"自身または味方を最大HPまで20回復");
            check(battle.Snapshot().Units[0].Hp==200 && battle.Snapshot().Units[1].Cooldowns[1]==2,"回復は攻撃せず再使用待ち2");
        } else check(battle.LastHits().size()==1 && battle.LastHits()[0].TargetId==0,"回復不可時はプレイヤーだけを射撃");
        check(ChooseBasicAction(battle).Kind==BasicActionKind::Wait,"回復/攻撃後は二度行動しない");
        const auto restored=Battle::Restore(battle.Checkpoint());
        check(restored && restored->Checkpoint()==battle.Checkpoint() && ChooseBasicAction(*restored)==ChooseBasicAction(battle),"回復/射撃後の乱数・再使用待ちと次行動を復元");
    }catch(const std::invalid_argument&){check(false,"妖狐の戦場を作成できる");}
    bool seen=false;
    for(unsigned seed=1;seed<=24;++seed) {
        RunSnapshot run;run.Day=5;run.Phase=RunPhase::Event;run.Active={NodeKind::Battle};run.Recruited={0,1,2,3};
        const auto battle=Encounter::Create(run,seed);
        for(const auto& unit:battle.Snapshot().Units)if(unit.Role==foxRole) {
            seen=true;check(unit.Side==Faction::Enemy && unit.Movement==3 && unit.Range==3 && unit.Attack==8 && unit.Skills[1] && !unit.Skills[0],"妖狐の初期出現と承認能力/技能");
        }
        run.Health[0].Hp=1;check(Encounter::Create(run,seed,nullptr,true).PotentialEnemyDamage(0)==0,"固定戦場でも初期即死を回避");
    }
    check(seen,"実戦で妖狐が出現する");
    {
        BattleUnit hero;hero.At={1,1};hero.Hp=hero.MaxHp=200;hero.Speed=1;
        BattleUnit fox;fox.Id=100;fox.Side=Faction::Enemy;fox.Role=foxRole;fox.At={2,1};fox.Speed=20;fox.Range=3;fox.Hp=20;fox.MaxHp=100;fox.Skills[1]=true;
        Battle battle(8,8,std::vector<Tile>(64),{hero,fox},42);
        check(ChooseBasicAction(battle).Kind==BasicActionKind::UseSkill && ApplyBasicAction(battle,ChooseBasicAction(battle)),"再使用待ち検証の自己回復");
        for(int turn=0;turn<3;++turn) {
            battle.EndTurn(battle.Snapshot().Revision,100,Facing::South);battle.EndTurn(battle.Snapshot().Revision,0,Facing::South);
            check(ChooseBasicAction(battle).Kind==(turn<2?BasicActionKind::Attack:BasicActionKind::UseSkill),"再使用待ち中は射撃、0で回復へ戻る");
            if(turn<2)ApplyBasicAction(battle,ChooseBasicAction(battle));
        }
        fox.At={6,1};fox.Hp=100;Battle ranged(8,8,std::vector<Tile>(64),{hero,fox},42);
        check(ChooseBasicAction(ranged).Kind==BasicActionKind::Move && ApplyBasicAction(ranged,ChooseBasicAction(ranged)),"回復不要で射程外なら射撃位置へ接近");
        check(ChooseBasicAction(ranged).Kind==BasicActionKind::Attack,"接近後は通常射撃");
        hero.Role=foxRole;
        try{Battle invalid(8,8,std::vector<Tile>(64),{hero,fox},42);check(false,"妖狐は敵限定");}
        catch(const std::invalid_argument&){check(true,"妖狐は敵限定");}
    }
    const auto frame=DecodeSave(std::string_view(LegacyRun7Combat,sizeof(LegacyRun7Combat)-1),SaveKind::Run);
    const auto legacy=frame.Value?DecodeRunPayload(frame.Value->Payload):std::nullopt;
    check(legacy && legacy->Combat && legacy->Combat->Battle.State.Units[0].AttackCursed,"旧v7の弱体を失わず移行");
    const auto migrated=legacy?EncodeRunPayload(*legacy):std::nullopt;
    check(migrated && DecodeRunPayload(*migrated)==legacy,"旧v7の全状態をv8へ保存復元");
    Run run(42,Difficulty::Beginner,HeroRole::Sword);run.ChooseNode(0,0);
    auto setup=Encounter::Create(run.Snapshot(),run.RandomState()).Checkpoint();setup.State.Tiles.assign(setup.State.Tiles.size(),Tile{});
    for(auto& unit:setup.State.Units)if(unit.Side==Faction::Enemy){unit.Role=foxRole;unit.At={2,1};unit.Movement=3;unit.Range=3;unit.Attack=8;unit.MaxHp=100;unit.Hp=20;unit.Skills[1]=true;}
    auto actual=Battle::Restore(setup);
    check(actual && actual->EndTurn(0,0,Facing::South) && ApplyBasicAction(*actual,ChooseBasicAction(*actual)),"実戦条件で自己回復後を準備");
    if(actual) {
        RunSaveState save{run.Checkpoint(),SavedCombat{actual->Checkpoint(),CombatControl{}.Checkpoint()}};
        const auto bytes=EncodeRunPayload(save);check(bytes && DecodeRunPayload(*bytes)==save,"妖狐の回復後HP/再使用待ちをv8保存復元");
        if(bytes){auto old=*bytes;old[4]=7;check(!DecodeRunPayload(old),"旧v7への妖狐役割の偽装を拒否");}
    }
    std::cout<<"FoxEnemy: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
