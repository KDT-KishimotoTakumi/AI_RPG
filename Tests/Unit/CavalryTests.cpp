#include "Domain/Battle.h"
#include "Application/Encounter.h"
#include "Infrastructure/SavePayload.h"
#include "../Fixtures/LegacyRun4Combat.h"
#include <iostream>
#include <stdexcept>
using namespace GirlsSpire;
int TestCavalry()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL Cavalry: "<<why<<'\n';}};
    // 新しい騎馬役割は既存値をずらさず末尾へ追加する。
    const auto cavalry=BattleRole::Cavalry;
    for(unsigned seed=1;seed<=100;++seed)for(int steps=2;steps<=5;++steps) {
        try {
            BattleUnit hero;hero.At={steps+2,1};hero.Speed=1;hero.Hp=hero.MaxHp=200;hero.Defense=2;
            BattleUnit rider;rider.Id=100;rider.Side=Faction::Enemy;rider.Role=cavalry;rider.At={1,1};rider.Movement=5;rider.Speed=20;rider.Attack=10;rider.Skills[0]=true;
            Battle battle(8,8,std::vector<Tile>(64),{hero,rider},seed);
            check(battle.Move(0,100,{steps+1,1}),"移動力5の騎馬が移動できる");
            const auto attack=battle.PreviewAttack(100,hero.At);
            if(!attack.Legal){check(false,"移動後に通常攻撃できる");continue;}
            check(attack.Hits.front().Damage==(steps>=3?14:8),"2マスは通常、直線3～5マスは攻撃力＋6");
            const auto before=battle.Checkpoint();
            const auto skill=battle.PreviewSkill(100,Skill::PowerStrike,hero.At);
            check(skill.Legal && skill.Attack.Hits.front().Damage==16,"強撃へ突進を加算しない");
            check(battle.Checkpoint()==before,"予告で突進状態や乱数を変更しない");
            const auto restored=Battle::Restore(before);
            check(restored && restored->PreviewAttack(100,hero.At)==attack,"移動直後の復元でも予告が一致");
            check(battle.EndTurn(battle.Snapshot().Revision,100,Facing::South) && battle.EndTurn(battle.Snapshot().Revision,0,Facing::South),"次の騎馬手番へ進む");
            check(battle.PreviewAttack(100,hero.At).Hits.front().Damage==8,"次手番へ突進を持ち越さない");
            check(battle.Move(battle.Snapshot().Revision,100,{steps,1}),"未行動の次手番は移動可能");
            check(!battle.Reachable(100).size(),"移動は手番につき一回");
        }catch(const std::invalid_argument&){check(false,"騎馬役割の戦場を作成できる");}
    }
    for(unsigned seed=1;seed<=100;++seed)for(int mode=0;mode<3;++mode) {
        BattleUnit hero;hero.At=mode==0?Position{4,2}:Position{5,1};hero.Speed=1;hero.Hp=hero.MaxHp=200;
        BattleUnit rider;rider.Id=100;rider.Side=Faction::Enemy;rider.Role=cavalry;rider.At={1,1};rider.Movement=5;rider.Speed=20;rider.Attack=10;
        std::vector<Tile> tiles(64);if(mode==1)tiles[1*8+2].Wall=true;if(mode==2)tiles[1*8+2].Trap=true;
        Battle battle(8,8,tiles,{hero,rider},seed);const Position target=mode==0?Position{3,2}:Position{4,1};
        const auto path=battle.PreviewMove(100,target);
        check(path.Legal && path.Path.size()>=3,"曲がった実経路が成立する");
        check(battle.Move(0,100,target),"曲がる移動を実行");
        const auto attack=battle.PreviewAttack(100,hero.At);
        check(attack.Legal && attack.Hits.front().Damage==8,"曲がる経路・壁迂回・罠回避に突進を付けない");
        check(battle.Snapshot().Units[1].Hp==rider.Hp,"罠回避経路を維持する");
    }
    for(unsigned seed=1;seed<=100;++seed) {
        BattleUnit hero;hero.At={5,1};hero.Speed=1;hero.Hp=hero.MaxHp=200;
        BattleUnit rider;rider.Id=100;rider.Side=Faction::Enemy;rider.Role=cavalry;rider.At={1,1};rider.Movement=5;rider.Speed=20;rider.Attack=10;rider.Skills[0]=true;
        Battle battle(8,8,std::vector<Tile>(64),{hero,rider},seed);battle.Move(0,100,{4,1});
        const auto prepared=battle.Checkpoint();
        check(battle.Attack(battle.Snapshot().Revision,100,hero.At),"突進通常攻撃を実行");
        check(battle.LastHits().front().Damage==(battle.LastHits().front().Hit?14:0),"命中時だけ突進ダメージを与える");
        check(!battle.Snapshot().ChargeReady && battle.Snapshot().Units[1].Attack==10,"外れでも準備を消費し基礎攻撃力を変えない");
        auto skill=Battle::Restore(prepared);
        check(skill && skill->UseSkill(skill->Snapshot().Revision,100,Skill::PowerStrike,hero.At),"突進準備中に強撃を使う");
        check(skill && skill->LastHits().front().Damage==(skill->LastHits().front().Hit?16:0) && !skill->Snapshot().ChargeReady,"スキルの実ダメージへも加算しない");
        auto invalidSkill=Battle::Restore(prepared);
        check(invalidSkill && !invalidSkill->UseSkill(invalidSkill->Snapshot().Revision,100,Skill::PowerStrike,{4,1}) && invalidSkill->Checkpoint()==prepared,"無効なスキルでは突進準備を消費しない");
        for(int invalid=0;invalid<5;++invalid) {
            auto bad=prepared;
            if(invalid==0)bad.State.Moved=false;if(invalid==1)bad.State.Acted=true;if(invalid==2)bad.State.Units[1].Role=BattleRole::Standard;
            if(invalid==3)bad.State.ActiveId=-1;if(invalid==4)bad.State.Units[1].Hp=0;
            check(!Battle::Restore(bad),"不正な突進準備を復元しない");
        }
        rider.At={4,1};Battle stationary(8,8,std::vector<Tile>(64),{hero,rider},seed);
        check(stationary.Attack(0,100,hero.At) && stationary.Reachable(100).empty(),"未移動でも行動後の移動は禁止");
    }
    {
        BattleUnit hero;hero.At={6,1};hero.Speed=30;hero.Hp=hero.MaxHp=100;
        BattleUnit rider;rider.Id=100;rider.Side=Faction::Enemy;rider.Role=cavalry;rider.At={1,1};rider.Movement=5;rider.Attack=10;
        Battle danger(8,8,std::vector<Tile>(64),{hero,rider},42);const auto before=danger.Checkpoint();
        check(danger.PotentialEnemyDamage(0)==14,"仲間の危険予測も敵の直線突進を含める");
        check(danger.Checkpoint()==before,"危険予測で本体や乱数を変更しない");
    }
    for(int hp:{5,40}) {
        BattleUnit hero;hero.At={5,1};hero.Speed=1;
        BattleUnit rider;rider.Id=100;rider.Side=Faction::Enemy;rider.Role=cavalry;rider.At={1,1};rider.Movement=5;rider.Speed=20;rider.Hp=rider.MaxHp=hp;
        std::vector<Tile> corridor(64);for(auto& tile:corridor)tile.Wall=true;for(int x=0;x<8;++x)corridor[8+x].Wall=false;corridor[8+2].Trap=true;
        Battle trapped(8,8,corridor,{hero,rider},42);
        check(trapped.Move(0,100,{4,1}),"直線経路の罠を共通ルールで処理");
        check(trapped.Snapshot().Units[1].Hp==(hp==5?0:30) && trapped.Snapshot().ChargeReady==(hp==40),"生存時だけ突進準備、致死罠では途中停止");
        check(Battle::Restore(trapped.Checkpoint()).has_value(),"罠通過後も復元可能");
    }
    {
        BattleUnit hero;hero.At={7,7};hero.Speed=1;
        BattleUnit rider;rider.Id=100;rider.Side=Faction::Enemy;rider.Role=cavalry;rider.At={1,1};rider.Movement=5;rider.Speed=20;rider.Attack=10;rider.Skills[0]=true;
        std::vector<Tile> tiles(64);tiles[8+5].ObstacleHp=20;Battle terrain(8,8,tiles,{hero,rider},42);
        check(terrain.Move(0,100,{4,1}),"地形攻撃用の直線移動");
        check(terrain.PreviewAttack(100,{5,1}).TerrainDamage==16,"通常の地形攻撃にも攻撃力補正");
        check(terrain.PreviewSkill(100,Skill::PowerStrike,{5,1}).Attack.TerrainDamage==18,"地形へのスキルに突進を加算しない");
        check(terrain.Attack(terrain.Snapshot().Revision,100,{5,1}) && terrain.Snapshot().Tiles[8+5].ObstacleHp==4,"地形の実耐久減少も一致");
    }
    {
        BattleUnit hero;hero.At={5,1};hero.Speed=1;hero.Defense=15;
        BattleUnit rider;rider.Id=100;rider.Side=Faction::Enemy;rider.Role=cavalry;rider.At={1,1};rider.Movement=5;rider.Speed=20;rider.Attack=10;
        Battle armored(8,8,std::vector<Tile>(64),{hero,rider},42);armored.Move(0,100,{4,1});
        check(armored.PreviewAttack(100,hero.At).Hits.front().Damage==1,"突進は防御計算後の固定ダメージではなく攻撃力を加算");
    }
    bool seen=false;
    for(unsigned seed=1;seed<=18;++seed) {
        RunSnapshot later;later.Day=5;later.Phase=RunPhase::Event;later.Active={NodeKind::Battle};later.Recruited={0,1,2,3};
        const auto encounter=Encounter::Create(later,seed);
        for(const auto& unit:encounter.Snapshot().Units)if(unit.Role==cavalry) {
            seen=true;check(unit.Side==Faction::Enemy && unit.Movement==5 && unit.Range==1,"5日目以降の騎馬出現と能力値");
        }
        later.Health[0].Hp=21;
        try {
            const auto safe=Encounter::Create(later,seed,nullptr,true);
            check(safe.Snapshot().Units[0].Hp==21 && safe.PotentialEnemyDamage(0)<21,"固定戦場でも突進込みで初手即死を防ぐ");
        }catch(const std::invalid_argument&){check(false,"低HPの固定戦場を安全に生成");}
    }
    check(seen,"実戦に騎馬亡者が出現する");
    const auto oldFrame=DecodeSave(std::string_view(LegacyRun4Combat,sizeof(LegacyRun4Combat)-1),SaveKind::Run);
    const auto legacy=oldFrame.Value?DecodeRunPayload(oldFrame.Value->Payload):std::nullopt;
    check(legacy && legacy->Combat && !legacy->Combat->Battle.State.ChargeReady,"旧v4の戦闘を突進準備なしで移行");
    const auto migrated=legacy?EncodeRunPayload(*legacy):std::nullopt;
    check(migrated && DecodeRunPayload(*migrated)==legacy,"旧状態を失わずv5へ再保存");
    Run run(42,Difficulty::Beginner,HeroRole::Sword);run.ChooseNode(0,0);
    const auto encounter=Encounter::Create(run.Snapshot(),run.RandomState()).Checkpoint();auto units=encounter.State.Units;
    for(auto& unit:units)if(unit.Side==Faction::Enemy){unit.Role=cavalry;unit.At={6,1};unit.Speed=30;unit.Movement=5;unit.Range=1;unit.Attack=10;}
    Battle charged(encounter.State.Width,encounter.State.Height,encounter.State.Tiles,units,42,BattleGoal::Rescue);
    check(charged.Move(0,100,{2,1}) && charged.Snapshot().ChargeReady,"保存検証用の直線4マス移動");
    RunSaveState saved{run.Checkpoint(),SavedCombat{charged.Checkpoint(),CombatControl{}.Checkpoint()}};
    const auto bytes=EncodeRunPayload(saved);
    check(bytes && DecodeRunPayload(*bytes)==saved,"突進準備を役割と共にv5へ保存復元");
    if(bytes){auto wrong=*bytes;wrong[4]=4;check(!DecodeRunPayload(wrong),"旧版へ新しい役割を紛れ込ませない");}
    std::cout<<"Cavalry: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
