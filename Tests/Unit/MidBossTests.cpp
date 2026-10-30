#include "AI/BasicTurn.h"
#include "Application/Encounter.h"
#include "Infrastructure/SavePayload.h"
#include "../Fixtures/LegacyRun9Combat.h"
#include <iostream>
#include <stdexcept>
using namespace GirlsSpire;
int TestMidBoss()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL MidBoss: "<<why<<'\n';}};
    const auto princess=static_cast<BattleRole>(10);
    for(unsigned seed=1;seed<=100;++seed)for(int hp:{81,80,79})try {
        BattleUnit hero;hero.At={1,1};hero.Hp=hero.MaxHp=200;hero.Speed=1;
        auto ally=hero;ally.Id=1;ally.At={1,2};
        BattleUnit boss;boss.Id=100;boss.Side=Faction::Enemy;boss.Role=princess;boss.At={3,1};boss.Speed=12;boss.Movement=2;boss.Range=2;boss.Attack=18;boss.Defense=4;boss.Hp=hp;boss.MaxHp=160;
        Battle battle(8,8,std::vector<Tile>(64),{hero,ally,boss},seed);
        const auto before=battle.Checkpoint();const auto preview=battle.PreviewAttack(100,hero.At);
        check(preview.Legal && preview.Hits.size()==static_cast<std::size_t>(hp<=80?2:1),"HP半分以下で通常攻撃だけ範囲化");
        check(battle.Checkpoint()==before,"予告は状態/乱数を変更しない");
        check(battle.Attack(0,100,hero.At) && battle.LastHits().size()==preview.Hits.size(),"予告と同じ範囲で実行");
        check(battle.Snapshot().Acted && battle.Snapshot().Revision==1 && ChooseBasicAction(battle).Kind==BasicActionKind::Wait,"範囲化でも追加手番を発生させない");
        check(battle.Snapshot().Units[2].Attack==18,"基礎攻撃力を維持");
        auto restored=Battle::Restore(battle.Checkpoint());check(restored && restored->Checkpoint()==battle.Checkpoint(),"半分以下の攻撃後状態を復元");
    }catch(const std::invalid_argument&){check(false,"落城姫の戦場を作成できる");}
    for(unsigned seed=1;seed<=100;++seed) {
        BattleUnit hero;hero.At={1,1};hero.Hp=hero.MaxHp=200;hero.Speed=1;
        auto ally=hero;ally.Id=1;ally.At={1,2};
        BattleUnit boss;boss.Id=100;boss.Side=Faction::Enemy;boss.Role=princess;boss.At={3,1};boss.Speed=20;boss.Movement=0;boss.Range=2;boss.Attack=18;boss.Hp=80;boss.MaxHp=160;boss.Skills[0]=true;
        BattleUnit friendUnit;friendUnit.Id=101;friendUnit.Side=Faction::Enemy;friendUnit.At={0,1};friendUnit.Speed=0;friendUnit.Movement=0;
        Battle battle(8,8,std::vector<Tile>(64),{hero,ally,boss,friendUnit},seed);
        const auto before=battle.Checkpoint();
        check(battle.PotentialEnemyDamage(1)==16,"射程外でも別の味方を中心とした範囲被害を予測");
        check(battle.Checkpoint()==before && battle.NormalAttackRadius(-1)==0,"危険予測が状態を変えず無効IDの範囲は0");
        check(battle.PreviewSkill(100,Skill::PowerStrike,hero.At).Attack.Hits.size()==1,"強撃を通常攻撃の段階で範囲化しない");
        auto normal=battle;normal.Attack(0,100,hero.At);
        check(normal.Snapshot().Units[3].Hp==friendUnit.Hp && normal.LastHits().size()==2,"通常範囲攻撃で友軍を巻き込まない");
        check(battle.UseSkill(0,100,Skill::PowerStrike,hero.At) && battle.LastHits().size()==1 && battle.Snapshot().Units[1].Hp==200,"スキルの実行も単体のまま");
        std::vector<Tile> tiles(64);tiles[8+2].Wall=true;
        Battle blocked(8,8,tiles,{hero,ally,boss},seed);
        check(!blocked.PreviewAttack(100,hero.At).Legal && ChooseBasicAction(blocked).Kind==BasicActionKind::Wait,"壁越しの範囲攻撃は選ばない");
        check(blocked.PotentialEnemyDamage(0)==0,"壁越しの被害を誤って予測しない");
    }
    {
        BattleUnit hero;hero.At={3,0};hero.Hp=hero.MaxHp=200;hero.Speed=1;
        auto ally=hero;ally.Id=1;ally.At={1,2};auto other=hero;other.Id=2;other.At={1,3};
        BattleUnit boss;boss.Id=100;boss.Side=Faction::Enemy;boss.Role=princess;boss.At={3,2};boss.Speed=20;boss.Range=2;boss.Attack=18;boss.Hp=80;boss.MaxHp=160;
        Battle grouped(8,8,std::vector<Tile>(64),{hero,ally,other,boss},42);
        const auto selected=ChooseBasicAction(grouped);
        check(selected.Kind==BasicActionKind::Attack && selected.Target==ally.At,"範囲内の全対象の予測被害で攻撃中心を選ぶ");
    }
    for(int difficulty=0;difficulty<3;++difficulty)for(unsigned seed=1;seed<=8;++seed) {
        RunSnapshot run;run.Day=6;run.Phase=RunPhase::Event;run.Active={NodeKind::MidBoss};run.Recruited={0,1,2,3};run.Level=static_cast<Difficulty>(difficulty);
        const auto battle=Encounter::Create(run,seed);
        const auto& boss=battle.Snapshot().Units.back();
        check(boss.Role==princess && boss.MaxHp==160*(80+20*difficulty)/100 && boss.Attack==18 && boss.Defense==4 && boss.Movement==2 && boss.Range==2 && boss.Speed==12,"6日目の承認能力と共通の難易度補正");
        run.Health[0].Hp=1;check(Encounter::Create(run,seed,nullptr,true).PotentialEnemyDamage(0)==0,"固定戦場でもボスの初期即死を回避");
    }
    const auto frame=DecodeSave(std::string_view(LegacyRun9Combat,sizeof(LegacyRun9Combat)-1),SaveKind::Run);
    const auto legacy=frame.Value?DecodeRunPayload(frame.Value->Payload):std::nullopt;
    check(legacy && legacy->Combat && legacy->Combat->Battle.State.Units[0].MovementWebbed,"旧v9の移動弱体を失わず移行");
    const auto migrated=legacy?EncodeRunPayload(*legacy):std::nullopt;check(migrated && DecodeRunPayload(*migrated)==legacy,"旧v9をv10へ全状態を保って保存");
    Run run(42,Difficulty::Beginner,HeroRole::Sword);run.ChooseNode(0,0);
    auto setup=Encounter::Create(run.Snapshot(),run.RandomState()).Checkpoint();setup.State.Tiles.assign(setup.State.Tiles.size(),Tile{});
    for(auto& unit:setup.State.Units)if(unit.Side==Faction::Enemy){unit.Role=princess;unit.At={3,1};unit.Movement=2;unit.Range=2;unit.MaxHp=160;unit.Hp=80;unit.Attack=18;unit.Defense=4;unit.Speed=12;}
    auto actual=Battle::Restore(setup);check(actual.has_value(),"中断検証の半分以下状態を準備");
    if(actual) {
        RunSaveState save{run.Checkpoint(),SavedCombat{actual->Checkpoint(),CombatControl{}.Checkpoint()}};
        const auto bytes=EncodeRunPayload(save);const auto loaded=bytes?DecodeRunPayload(*bytes):std::nullopt;
        check(loaded && *loaded==save && Battle::Restore(loaded->Combat->Battle)->NormalAttackRadius(100)==1,"HP段階・能力と範囲をv10で保存復元");
        if(bytes){auto old=*bytes;old[4]=9;check(!DecodeRunPayload(old),"旧v9への落城姫役割の偽装を拒否");}
    }
    std::cout<<"MidBoss: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
