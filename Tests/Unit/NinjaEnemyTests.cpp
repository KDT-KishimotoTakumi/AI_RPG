#include "AI/BasicTurn.h"
#include "Application/Encounter.h"
#include "Infrastructure/SavePayload.h"
#include <iostream>
using namespace GirlsSpire;
int TestNinjaEnemy()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL NinjaEnemy: "<<why<<'\n';}};
    for(unsigned seed=1;seed<=100;++seed)for(int mode=0;mode<5;++mode) {
        BattleUnit hero;hero.At={1,1};hero.Hp=hero.MaxHp=200;hero.Speed=1;hero.Movement=1;
        BattleUnit ninja;ninja.Id=100;ninja.Side=Faction::Enemy;ninja.Role=BattleRole::Ninja;ninja.At={2,1};ninja.Speed=20;ninja.Movement=4;ninja.Range=1;
        if(mode==1)ninja.Role=BattleRole::Standard;
        std::vector<Tile> tiles(64);
        if(mode==2)for(int y=0;y<8;++y)for(int x=0;x<8;++x)if(Position{x,y}!=hero.At && Position{x,y}!=ninja.At)tiles[y*8+x].Trap=true;
        if(mode==2)ninja.Hp=10;
        Battle battle(8,8,tiles,{hero,ninja},seed);
        if(mode==3){check(battle.Move(0,100,{1,2}),"攻撃前に移動");}
        const auto attack=ChooseBasicAction(battle);
        check(attack.Kind==BasicActionKind::Attack,"隣接していれば先に通常攻撃");
        check(ApplyBasicAction(battle,attack),"通常攻撃を適用");
        const auto before=battle.Checkpoint();
        const auto next=ChooseBasicAction(battle);
        check(battle.Checkpoint()==before && ChooseBasicAction(battle)==next,"退避選択は状態と乱数を消費せず再現");
        if(mode==1 || mode==2 || mode==3)check(next.Kind==BasicActionKind::Wait,"通常役割/致死罠/移動済みなら退避しない");
        else {
            check(next.Kind==BasicActionKind::Move,"未移動の忍は攻撃後に退避");
            if(next.Kind==BasicActionKind::Move){
                check(battle.PreviewMove(100,next.Target).Legal,"共通移動ルールに従う");
                check(ApplyBasicAction(battle,next),"退避を実行");
                check(ChooseBasicAction(battle).Kind==BasicActionKind::Wait,"二度目の移動をしない");
                check(std::abs(next.Target.X-1)+std::abs(next.Target.Y-1)==5,"同じ被害なら最寄りプレイヤーから最遠へ");
                const auto restored=Battle::Restore(battle.Checkpoint());
                check(restored && ChooseBasicAction(*restored)==ChooseBasicAction(battle),"退避済み復元後も待機");
            }
        }
    }
    for(int mode=0;mode<4;++mode) {
        BattleUnit hero;hero.At={1,1};hero.Hp=hero.MaxHp=200;hero.Speed=1;hero.Movement=0;hero.Range=8;hero.Attack=30;
        BattleUnit ninja;ninja.Id=100;ninja.Side=Faction::Enemy;ninja.Role=BattleRole::Ninja;ninja.At={2,1};ninja.Speed=20;ninja.Movement=4;ninja.Range=1;
        if(mode==1)ninja.Movement=0;
        if(mode==2){ninja.Skills[1]=true;ninja.Hp=20;}
        std::vector<Tile> tiles(64);
        // 遠いマスが罠の場合、距離より被害最小を優先する。
        if(mode==0)for(int y=0;y<8;++y)for(int x=0;x<8;++x)
            if(std::abs(x-1)+std::abs(y-1)>=4)tiles[y*8+x].Trap=true;
        Battle battle(8,8,tiles,{hero,ninja},42);
        check(battle.PotentialHostileDamage(100)==28 && battle.PotentialEnemyDamage(100)==0,"双方の予測を共有し従来の敵対象APIは0を維持");
        check(battle.PotentialHostileDamage(-1)==0,"存在しない対象の予測は0");
        if(mode==2)check(battle.UseSkill(0,100,Skill::FirstAid,ninja.At),"攻撃以外の回復行動");
        else check(battle.Attack(0,100,hero.At),"退避境界の攻撃を実行");
        const auto action=ChooseBasicAction(battle);
        if(mode==0)check(action.Kind==BasicActionKind::Move && battle.PreviewMove(100,action.Target).Damage==0 && std::abs(action.Target.X-1)+std::abs(action.Target.Y-1)==3,"被害を増やして距離だけ稼ぐ経路を選ばない");
        if(mode==1 || mode==2)check(action.Kind==BasicActionKind::Wait,"移動不可/回復後は待機");
        const auto restored=Battle::Restore(battle.Checkpoint());
        check(restored && ChooseBasicAction(*restored)==action,"復元後も同じ退避を選ぶ");
    }
    bool seen=false;
    for(unsigned seed=1;seed<=24;++seed) {
        RunSnapshot run;run.Day=5;run.Phase=RunPhase::Event;run.Active={NodeKind::Battle};run.Recruited={0,1,2,3};
        const auto battle=Encounter::Create(run,seed);
        for(const auto& unit:battle.Snapshot().Units)if(unit.Side==Faction::Enemy && unit.Role==BattleRole::Ninja) {
            seen=true;check(unit.Movement==4 && unit.Range==1,"骨忍の実戦出現と移動4/射程1");
        }
        run.Health[0].Hp=1;const auto safe=Encounter::Create(run,seed);
        check(safe.PotentialEnemyDamage(0)==0,"骨忍を含む初手即死を回避");
    }
    check(seen,"5日目以降に骨忍が出現する");
    Run run(42,Difficulty::Beginner,HeroRole::Sword);run.ChooseNode(0,0);
    auto setup=Encounter::Create(run.Snapshot(),run.RandomState()).Checkpoint();
    setup.State.Tiles.assign(setup.State.Tiles.size(),Tile{});
    for(auto& unit:setup.State.Units)if(unit.Side==Faction::Enemy){unit.Role=BattleRole::Ninja;unit.At={2,1};unit.Movement=4;unit.Range=1;}
    auto actual=Battle::Restore(setup);
    check(actual && actual->EndTurn(0,0,Facing::South) && actual->Attack(1,100,{1,1}),"実戦と同じ救出条件で攻撃後状態を準備");
    if(actual)for(int stage=0;stage<2;++stage) {
        RunSaveState save{run.Checkpoint(),SavedCombat{actual->Checkpoint(),CombatControl{}.Checkpoint()}};
        const auto bytes=EncodeRunPayload(save);const auto decoded=bytes?DecodeRunPayload(*bytes):std::nullopt;
        check(decoded && *decoded==save,"骨忍の攻撃後/退避後を既存v5形式で保存復元");
        if(decoded){const auto restored=Battle::Restore(decoded->Combat->Battle);check(restored && ChooseBasicAction(*restored)==ChooseBasicAction(*actual),"保存復元後の次行動も一致");}
        if(stage==0)check(ChooseBasicAction(*actual).Kind==BasicActionKind::Move && ApplyBasicAction(*actual,ChooseBasicAction(*actual)),"攻撃後に退避を適用");
    }
    std::cout<<"NinjaEnemy: "<<checks<<" checks, "<<failed<<" failed\n";
    return failed;
}
