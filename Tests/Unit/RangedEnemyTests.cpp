#include "AI/BasicTurn.h"
#include "Application/Encounter.h"
#include <iostream>
using namespace GirlsSpire;
int TestRangedEnemy()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL RangedEnemy: "<<why<<'\n';}};
    for(const auto role:{BattleRole::Archer,BattleRole::Gunner})for(unsigned seed=1;seed<=100;++seed) {
        BattleUnit hero;hero.Id=0;hero.At={5,3};hero.Speed=1;hero.Hp=hero.MaxHp=100;
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.Role=role;enemy.At={1,3};enemy.Speed=20;enemy.Range=4;enemy.Movement=3;
        std::vector<Tile> tiles(64);tiles[3*8+3].Wall=true;
        Battle blocked(8,8,tiles,{hero,enemy},seed);
        bool canFire=false;
        for(const auto at:blocked.Reachable(100)){auto trial=blocked;if(trial.Move(trial.Snapshot().Revision,100,at) && trial.PreviewAttack(100,hero.At).Legal)canFire=true;}
        check(canFire,"試験戦場に移動後の射撃可能地点が存在する");
        const auto saved=blocked.Checkpoint();const auto random=blocked.RandomState();
        const auto move=ChooseBasicAction(blocked);
        check(move.Kind==BasicActionKind::Move,"壁越しに撃たず射線位置へ移動する");
        check(blocked.Checkpoint()==saved && blocked.RandomState()==random,"思考では状態も乱数も消費しない");
        auto projected=blocked;
        const bool moved=ApplyBasicAction(projected,move);check(moved,"選択した移動が合法");
        check(moved && projected.PreviewAttack(100,hero.At).Legal,"距離だけの接近より移動後の射線を優先する");
        check(moved && ChooseBasicAction(projected).Kind==BasicActionKind::Attack,"移動後はその手番で射撃する");
        const auto restored=Battle::Restore(saved);
        check(restored && ChooseBasicAction(*restored)==move,"復元後も同じ行動を選ぶ");
        Battle clear(8,8,std::vector<Tile>(64),{hero,enemy},seed);
        check(ChooseBasicAction(clear)==BasicAction{BasicActionKind::Attack,hero.At},"射程内かつ射線ありでは移動せず射撃");
        auto friendUnit=enemy;friendUnit.Id=101;friendUnit.At={2,3};friendUnit.Speed=0;
        Battle friendly(8,8,std::vector<Tile>(64),{hero,enemy,friendUnit},seed);
        const auto shot=ChooseBasicAction(friendly);
        check(shot==BasicAction{BasicActionKind::Attack,hero.At},"射程内の友軍を攻撃対象にしない");
        check(ApplyBasicAction(friendly,shot) && friendly.Snapshot().Units[2].Hp==friendUnit.Hp,"射撃しても友軍HPを減らさない");
        hero.At={7,7};Battle distant(8,8,std::vector<Tile>(64),{hero,enemy},seed);
        check(ChooseBasicAction(distant).Kind==BasicActionKind::Move,"今手番で撃てなくても接近する");
        enemy.Movement=0;Battle immobile(8,8,std::vector<Tile>(64),{hero,enemy},seed);
        check(ChooseBasicAction(immobile).Kind==BasicActionKind::Wait,"移動も射撃も不可能なら待機");
    }
    for(const auto kind:{NodeKind::Battle,NodeKind::Rescue,NodeKind::Elite})for(int difficulty=0;difficulty<3;++difficulty)for(unsigned seed=1;seed<=12;++seed) {
        RunSnapshot run;run.Phase=RunPhase::Event;run.Active={kind,0};run.Level=static_cast<Difficulty>(difficulty);
        const auto battle=Encounter::Create(run,seed);const auto& unit=battle.Snapshot().Units[1];
        const auto type=(seed+run.Day)%3;
        if(kind==NodeKind::Elite)check(unit.CreatesCover,"強敵の絡繰鎧は維持する");
        else {
            const auto expected=type==1?BattleRole::Archer:type==2?BattleRole::Gunner:BattleRole::Standard;
            check(unit.Role==expected,"同じシードから近接・弓兵・鉄砲兵を再現する");
            check(unit.Range==(type==1?4:type==2?5:1),"射程は既存の役割基準を使う");
            check(unit.Movement==(type==2?2:3),"鉄砲兵の移動力を区別する");
        }
        const auto restored=Battle::Restore(battle.Checkpoint());
        check(restored && restored->Checkpoint()==battle.Checkpoint(),"射撃敵の役割と射程も復元する");
        run.Health[0].Hp=1;
        const auto fragile=Encounter::Create(run,seed);
        check(fragile.Snapshot().Units[0].Hp==1 && fragile.PotentialEnemyDamage(0)==0,"低HPを補充せず射程と移動力の外へ配置する");
    }
    std::cout<<"RangedEnemy: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
