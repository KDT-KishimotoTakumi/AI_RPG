#include "AI/BasicTurn.h"
#include "Application/Encounter.h"
#include "Infrastructure/SavePayload.h"
#include "../Fixtures/LegacyRun8Combat.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
using namespace GirlsSpire;
int TestSpiderEnemy()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL SpiderEnemy: "<<why<<'\n';}};
    const auto spider=static_cast<BattleRole>(9);
    for(unsigned seed=1;seed<=100;++seed)for(int movement=0;movement<=4;++movement)try {
        BattleUnit hero;hero.At={1,1};hero.Hp=hero.MaxHp=200;hero.Speed=1;hero.Movement=movement;
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.Role=spider;enemy.At={3,1};enemy.Speed=20;enemy.Movement=2;enemy.Range=3;enemy.Attack=10;
        Battle battle(8,8,std::vector<Tile>(64),{hero,enemy},seed);
        check(battle.Attack(0,100,hero.At),"絡新婦の通常攻撃");const bool hit=battle.LastHits()[0].Hit;
        check(battle.EndTurn(1,100,Facing::South),"対象の次手番へ");
        const int limit=hit?std::max(0,movement-2):movement;
        const auto reachable=battle.Reachable(0);
        check((limit==0)==reachable.empty(),"移動力0では到達候補なし");
        for(const auto at:reachable){const auto path=battle.PreviewMove(0,at);check(path.Legal && path.Path.size()<=static_cast<std::size_t>(limit),"移動範囲と実経路に同じ弱体を反映");}
        check(battle.PreviewMove(0,{1,2}).Legal==(limit>0),"1マス移動の境界");
        battle.EndTurn(battle.Snapshot().Revision,0,Facing::South);battle.EndTurn(battle.Snapshot().Revision,100,Facing::South);
        check(battle.Reachable(0).empty()==(movement==0) && battle.Snapshot().Units[0].Movement==movement,"手番終了で解除し基礎移動力を維持");
    }catch(const std::invalid_argument&){check(false,"絡新婦の戦場を作成できる");}
    for(const auto role:{BattleRole::Dancer,BattleRole::Ninja,BattleRole::Archer}) {
        BattleUnit hero;hero.Role=role;hero.At={1,1};hero.Hp=hero.MaxHp=200;hero.Speed=1;hero.Range=5;
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.Role=spider;enemy.At={3,1};enemy.Speed=20;enemy.Range=3;enemy.Hp=enemy.MaxHp=200;enemy.Accuracy=200;enemy.Skills[0]=true;
        Battle battle(8,8,std::vector<Tile>(64),{hero,enemy},42);
        check(battle.MovementAllowance(-1)==0,"存在しない対象の移動力は0");
        check(battle.Attack(0,100,hero.At) && battle.Snapshot().Units[0].MovementWebbed,"命中で移動弱体");
        battle.EndTurn(1,100,Facing::South);
        const auto before=battle.Checkpoint();
        check(battle.Attack(2,0,enemy.At) && battle.Snapshot().Units[0].MovementWebbed,"行動しても手番終了前は弱体を保持");
        check(battle.PreviewMove(0,{1,2}).Legal==(role!=BattleRole::Archer) && !battle.PreviewMove(0,{1,3}).Legal,"舞姫/忍の行動後移動も1マス、弓は行動後移動不可");
        const auto restored=Battle::Restore(battle.Checkpoint());
        check(restored && restored->MovementAllowance(0)==1 && restored->Reachable(0)==battle.Reachable(0),"行動後の弱体と移動候補を復元");
        auto skill=Battle::Restore(before);skill->EndTurn(2,0,Facing::South);
        check(skill->UseSkill(skill->Snapshot().Revision,100,Skill::PowerStrike,hero.At) && !skill->Snapshot().Units[0].MovementWebbed,"スキルでは移動弱体を付与しない");
    }
    for(unsigned seed=1;seed<=100;++seed) {
        BattleUnit hero;hero.At={1,1};hero.Hp=hero.MaxHp=200;hero.Speed=1;hero.Movement=4;
        BattleUnit first;first.Id=100;first.Side=Faction::Enemy;first.Role=spider;first.At={2,1};first.Speed=20;
        auto second=first;second.Id=101;second.At={1,2};second.Speed=19;
        Battle battle(8,8,std::vector<Tile>(64),{hero,first,second},seed);
        battle.Attack(0,100,hero.At);const bool hit=battle.LastHits()[0].Hit;battle.EndTurn(1,100,Facing::South);
        battle.Attack(2,101,hero.At);const bool any=hit || battle.LastHits()[0].Hit;battle.EndTurn(3,101,Facing::South);
        check(battle.MovementAllowance(0)==(any?2:4),"連続命中で更新しても移動−2が重ならない");
        battle.EndTurn(4,0,Facing::South);check(!battle.Snapshot().Units[0].MovementWebbed,"次の自身の手番終了で解除");
    }
    bool seen=false;
    for(unsigned seed=1;seed<=24;++seed) {
        RunSnapshot run;run.Day=5;run.Phase=RunPhase::Event;run.Active={NodeKind::Battle};run.Recruited={0,1,2,3};
        const auto battle=Encounter::Create(run,seed);
        for(const auto& unit:battle.Snapshot().Units)if(unit.Role==spider){seen=true;check(unit.Side==Faction::Enemy && unit.Movement==2 && unit.Range==3 && unit.Attack==10,"絡新婦の初期出現と能力");}
        run.Health[0].Hp=1;check(Encounter::Create(run,seed,nullptr,true).PotentialEnemyDamage(0)==0,"固定戦場でも初期即死を回避");
    }
    check(seen,"実戦に絡新婦が出現する");
    const auto frame=DecodeSave(std::string_view(LegacyRun8Combat,sizeof(LegacyRun8Combat)-1),SaveKind::Run);
    const auto legacy=frame.Value?DecodeRunPayload(frame.Value->Payload):std::nullopt;
    check(legacy && legacy->Combat && !legacy->Combat->Battle.State.Units[0].MovementWebbed,"旧v8を移動弱体なしで移行");
    const auto migrated=legacy?EncodeRunPayload(*legacy):std::nullopt;check(migrated && DecodeRunPayload(*migrated)==legacy,"旧v8をv9へ全状態を保って保存");
    Run run(42,Difficulty::Beginner,HeroRole::Sword);run.ChooseNode(0,0);
    auto setup=Encounter::Create(run.Snapshot(),run.RandomState()).Checkpoint();setup.State.Tiles.assign(setup.State.Tiles.size(),Tile{});
    for(auto& unit:setup.State.Units)if(unit.Side==Faction::Enemy){unit.Role=spider;unit.At={2,1};unit.Movement=2;unit.Range=3;unit.Attack=10;unit.Accuracy=200;}
    auto actual=Battle::Restore(setup);
    check(actual && actual->EndTurn(0,0,Facing::South) && actual->Attack(1,100,{1,1}) && actual->Snapshot().Units[0].MovementWebbed,"実戦条件で移動弱体を付与");
    if(actual) {
        RunSaveState save{run.Checkpoint(),SavedCombat{actual->Checkpoint(),CombatControl{}.Checkpoint()}};
        const auto bytes=EncodeRunPayload(save);check(bytes && DecodeRunPayload(*bytes)==save,"移動弱体をv9保存復元");
        if(bytes){auto old=*bytes;old[4]=8;check(!DecodeRunPayload(old),"旧v8への新役割と状態の偽装を拒否");}
    }
    std::cout<<"SpiderEnemy: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
