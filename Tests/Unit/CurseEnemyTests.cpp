#include "AI/BasicTurn.h"
#include "Application/Encounter.h"
#include "Infrastructure/SavePayload.h"
#include "../Fixtures/LegacyRun6Combat.h"
#include <iostream>
#include <stdexcept>
using namespace GirlsSpire;
int TestCurseEnemy()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL CurseEnemy: "<<why<<'\n';}};
    // 既存役割を維持し、呪詛僧を末尾へ追加する。
    const auto monk=static_cast<BattleRole>(7);
    for(unsigned seed=1;seed<=100;++seed)for(int mode=0;mode<3;++mode)try {
        BattleUnit hero;hero.At={1,1};hero.Hp=hero.MaxHp=200;hero.Speed=1;hero.Attack=mode==2?2:12;hero.Skills[0]=true;
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.Role=monk;enemy.At={2,1};enemy.Speed=20;enemy.Movement=2;enemy.Range=3;enemy.Attack=10;enemy.Hp=enemy.MaxHp=200;
        Battle battle(8,8,std::vector<Tile>(64),{hero,enemy},seed);
        if(mode==1){auto state=battle.Checkpoint();state.State.Units[1].Skills[0]=true;battle=*Battle::Restore(state);check(battle.UseSkill(0,100,Skill::PowerStrike,hero.At),"僧のスキルを実行");}
        else check(battle.Attack(0,100,hero.At),"僧の通常攻撃");
        const bool cursed=mode!=1 && battle.LastHits()[0].Hit;
        check(battle.EndTurn(battle.Snapshot().Revision,100,Facing::South),"主人公の次手番へ進む");
        const auto attack=battle.PreviewAttack(0,enemy.At);
        const int power=std::max(0,hero.Attack-(cursed?4:0));
        check(attack.Legal && attack.Hits[0].Damage==std::max(1,power-2),"通常命中だけ攻撃−4、最低ダメージ規則を維持");
        const auto skill=battle.PreviewSkill(0,Skill::PowerStrike,enemy.At);
        check(skill.Legal && skill.Attack.Hits[0].Damage==std::max(1,hero.Attack+8-(cursed?4:0)-2),"強撃へも弱体を反映");
        const auto before=battle.Checkpoint();const auto restored=Battle::Restore(before);
        check(restored && restored->PreviewAttack(0,enemy.At)==attack && battle.Checkpoint()==before,"復元と予告が状態/乱数を変えない");
        battle.EndTurn(battle.Snapshot().Revision,0,Facing::South);battle.EndTurn(battle.Snapshot().Revision,100,Facing::South);
        check(battle.PreviewAttack(0,enemy.At).Hits[0].Damage==std::max(1,hero.Attack-2),"対象の手番終了で解除され次の周回まで残らない");
    }catch(const std::invalid_argument&){check(false,"呪詛僧の戦場を作成できる");}
    for(unsigned seed=1;seed<=100;++seed) {
        BattleUnit hero;hero.At={1,1};hero.Hp=hero.MaxHp=200;hero.Speed=1;
        BattleUnit first;first.Id=100;first.Side=Faction::Enemy;first.Role=monk;first.At={2,1};first.Speed=20;first.Range=3;first.Attack=10;
        auto second=first;second.Id=101;second.At={1,2};second.Speed=19;
        Battle battle(8,8,std::vector<Tile>(64),{hero,first,second},seed);
        battle.Attack(0,100,hero.At);const bool hit=battle.LastHits()[0].Hit;battle.EndTurn(1,100,Facing::South);
        battle.Attack(2,101,hero.At);const bool any=hit || battle.LastHits()[0].Hit;battle.EndTurn(3,101,Facing::South);
        check(battle.Snapshot().Units[0].AttackCursed==any && battle.PreviewAttack(0,first.At).Hits[0].Damage==(any?6:10),"連続命中でも弱体が重ならず期限を更新");
        check(battle.EndTurn(4,0,Facing::South) && !battle.Snapshot().Units[0].AttackCursed,"対象が待機で手番終了しても解除");
    }
    {
        std::vector<BattleUnit> units;
        for(int id=0;id<5;++id){BattleUnit unit;unit.Id=id;unit.At={id==0?1:0,id==0?1:id};unit.Speed=id==0?30:1;units.push_back(unit);}
        units[0].AttackCursed=true;units[0].Skills.fill(true);
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={2,1};enemy.Hp=enemy.MaxHp=200;units.push_back(enemy);
        Battle battle(8,8,std::vector<Tile>(64),units,42);auto saved=battle.Checkpoint();saved.State.Combos.Gauge=100;
        battle=*Battle::Restore(saved);
        check(battle.PreviewUltimate(0,enemy.At).Hits[0].Damage==26,"個人必殺へ弱体を反映");
        check(battle.PreviewTeamUltimate(0,enemy.At).Hits[0].Damage==46,"全員必殺へ発動者の弱体を反映");
        for(int action=0;action<5;++action) {
            auto copy=battle;
            const bool used=action==0?copy.Attack(0,0,enemy.At):action==1?copy.UseSkill(0,0,Skill::PowerStrike,enemy.At):action==2?copy.UseSkill(0,0,Skill::Sweep,enemy.At):action==3?copy.UseUltimate(0,0,enemy.At):copy.UseTeamUltimate(0,0,enemy.At);
            const int damage=action==0 || action==2?6:action==1?14:action==3?26:46;
            check(used && copy.LastHits()[0].Damage==(copy.LastHits()[0].Hit?damage:0) && copy.Snapshot().Units[0].Attack==12,"各攻撃の実ダメージと基礎能力値の維持");
        }
    }
    bool seen=false;
    for(unsigned seed=1;seed<=24;++seed) {
        RunSnapshot run;run.Day=5;run.Phase=RunPhase::Event;run.Active={NodeKind::Battle};run.Recruited={0,1,2,3};
        const auto battle=Encounter::Create(run,seed);
        for(const auto& unit:battle.Snapshot().Units)if(unit.Role==monk) {
            seen=true;check(unit.Side==Faction::Enemy && unit.Movement==2 && unit.Range==3 && unit.Attack==10,"呪詛僧の初期出現と能力");
        }
        run.Health[0].Hp=1;const auto safe=Encounter::Create(run,seed,nullptr,true);
        check(safe.PotentialEnemyDamage(0)==0,"呪詛僧を含む初期即死を回避");
    }
    check(seen,"実戦に呪詛僧が出現する");
    {
        BattleUnit hero;hero.At={1,1};hero.Speed=1;hero.Hp=hero.MaxHp=200;
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.Role=monk;enemy.At={6,1};enemy.Movement=2;enemy.Range=3;enemy.Attack=10;enemy.Speed=20;enemy.Accuracy=200;
        Battle battle(8,8,std::vector<Tile>(64),{hero,enemy},42);
        const auto move=ChooseBasicAction(battle);
        check(move.Kind==BasicActionKind::Move && ApplyBasicAction(battle,move),"射程外では今手番に撃てる位置へ接近");
        const auto shot=ChooseBasicAction(battle);
        check(shot.Kind==BasicActionKind::Attack && ApplyBasicAction(battle,shot) && battle.Snapshot().Units[0].AttackCursed,"接近後の通常攻撃でも呪詛を付与");
    }
    const auto frame=DecodeSave(std::string_view(LegacyRun6Combat,sizeof(LegacyRun6Combat)-1),SaveKind::Run);
    const auto legacy=frame.Value?DecodeRunPayload(frame.Value->Payload):std::nullopt;
    check(legacy && legacy->Combat && !legacy->Combat->Battle.State.Units[0].AttackCursed,"旧v6を弱体なしで移行");
    const auto migrated=legacy?EncodeRunPayload(*legacy):std::nullopt;
    check(migrated && DecodeRunPayload(*migrated)==legacy,"旧v6をv7へ状態を保って再保存");
    Run run(42,Difficulty::Beginner,HeroRole::Sword);run.ChooseNode(0,0);
    auto setup=Encounter::Create(run.Snapshot(),run.RandomState()).Checkpoint();setup.State.Tiles.assign(setup.State.Tiles.size(),Tile{});
    for(auto& unit:setup.State.Units)if(unit.Side==Faction::Enemy){unit.Role=monk;unit.At={2,1};unit.Movement=2;unit.Range=3;unit.Attack=10;unit.Accuracy=200;}
    auto actual=Battle::Restore(setup);
    check(actual && actual->EndTurn(0,0,Facing::South) && actual->Attack(1,100,{1,1}) && actual->Snapshot().Units[0].AttackCursed,"実戦と同じ条件で弱体を付与");
    if(actual) {
        RunSaveState save{run.Checkpoint(),SavedCombat{actual->Checkpoint(),CombatControl{}.Checkpoint()}};
        const auto bytes=EncodeRunPayload(save);check(bytes && DecodeRunPayload(*bytes)==save,"僧の役割と弱体状態をv7保存復元");
        if(bytes){auto old=*bytes;old[4]=6;check(!DecodeRunPayload(old),"新しい役割と状態の旧v6偽装を拒否");}
    }
    std::cout<<"CurseEnemy: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
