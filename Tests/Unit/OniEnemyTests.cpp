#include "AI/BasicTurn.h"
#include "Application/Encounter.h"
#include "Infrastructure/SavePayload.h"
#include "../Fixtures/LegacyRun5Combat.h"
#include <iostream>
#include <stdexcept>
using namespace GirlsSpire;
int TestOniEnemy()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL OniEnemy: "<<why<<'\n';}};
    // 既存の役割値を維持して鬼武者を末尾へ追加する。
    const auto oni=static_cast<BattleRole>(6);
    for(unsigned seed=1;seed<=100;++seed)for(int mode=0;mode<6;++mode)try {
        BattleUnit hero;hero.At={1,1};hero.Hp=hero.MaxHp=200;hero.Speed=1;
        if(mode==1)hero.Hp=5;
        if(mode==2)hero.Defense=100;
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.Role=oni;enemy.At={2,1};enemy.Speed=20;enemy.Movement=2;enemy.Attack=16;enemy.Range=1;enemy.Skills[0]=mode!=3;
        std::vector<Tile> tiles(64);if(mode==4)enemy.At={3,1};if(mode==5){enemy.At={3,1};tiles[8+2].Wall=true;}
        Battle battle(8,8,tiles,{hero,enemy},seed);
        const auto before=battle.Checkpoint();const auto action=ChooseBasicAction(battle);
        check(ChooseBasicAction(battle)==action && battle.Checkpoint()==before,"思考は乱数/状態を変えず再現");
        if(mode==4 || mode==5) {
            check(action.Kind!=BasicActionKind::UseSkill && action.Kind!=BasicActionKind::Attack,"射程外/壁越しには攻撃しない");
            check(ApplyBasicAction(battle,action),"射程外は合法な接近または待機");
        } else {
            check(action.Kind==(mode<=1?BasicActionKind::UseSkill:BasicActionKind::Attack),"予告ダメージが増える場合だけ強撃を選ぶ");
            check(action.Target==hero.At,"味方ではなくプレイヤーを狙う");
            check(ApplyBasicAction(battle,action),"選んだ攻撃を適用");
            const auto expected=mode==0?22:mode==1?5:mode==2?1:14;
            check(battle.LastHits().size()==1 && battle.LastHits()[0].Damage==(battle.LastHits()[0].Hit?expected:0),"実ダメージが予告と一致");
            check(battle.Snapshot().Units[1].Attack==16,"強撃後も基礎攻撃力16を維持");
            if(mode==0) {
                check(battle.Snapshot().Units[1].Cooldowns[0]==2,"強撃の共通再使用待ち2");
                battle.EndTurn(battle.Snapshot().Revision,100,Facing::South);battle.EndTurn(battle.Snapshot().Revision,0,Facing::South);
                check(ChooseBasicAction(battle).Kind==BasicActionKind::Attack,"次手番は再使用待ちで通常攻撃");
                for(int turn=0;turn<2;++turn) {
                    check(ApplyBasicAction(battle,ChooseBasicAction(battle)) && battle.EndTurn(battle.Snapshot().Revision,100,Facing::South) && battle.EndTurn(battle.Snapshot().Revision,0,Facing::South),"再使用待ち中の通常攻撃と手番経過");
                }
                check(ChooseBasicAction(battle).Kind==BasicActionKind::UseSkill,"再使用待ち0で強撃を再選択");
            }
        }
    }catch(const std::invalid_argument&){check(false,"鬼武者役割の戦場を作成できる");}
    for(unsigned seed=1;seed<=100;++seed) {
        BattleUnit hero;hero.At={1,1};hero.Hp=hero.MaxHp=200;hero.Speed=1;
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.Role=oni;enemy.At={2,1};enemy.Speed=20;enemy.Attack=16;enemy.Movement=2;enemy.Skills[0]=true;
        auto friendUnit=enemy;friendUnit.Id=101;friendUnit.At={2,2};friendUnit.Speed=0;friendUnit.Hp=1;
        Battle battle(8,8,std::vector<Tile>(64),{hero,enemy,friendUnit},seed);
        check(ChooseBasicAction(battle).Target==hero.At && ApplyBasicAction(battle,ChooseBasicAction(battle)) && battle.Snapshot().Units[2].Hp==1,"強撃でも味方を狙わず巻き込まない");
        const auto restored=Battle::Restore(battle.Checkpoint());
        check(restored && restored->Checkpoint()==battle.Checkpoint() && ChooseBasicAction(*restored)==ChooseBasicAction(battle),"強撃直後も乱数/再使用待ち/次行動を復元");
    }
    bool seen=false;
    for(unsigned seed=1;seed<=24;++seed) {
        RunSnapshot run;run.Day=5;run.Phase=RunPhase::Event;run.Active={NodeKind::Battle};run.Recruited={0,1,2,3};
        const auto battle=Encounter::Create(run,seed);
        for(const auto& unit:battle.Snapshot().Units)if(unit.Side==Faction::Enemy && unit.Role==oni) {
            seen=true;check(unit.Movement==2 && unit.Range==1 && unit.Attack==16 && unit.Skills[0],"鬼武者の実戦出現と承認能力値");
        }
        run.Health[0].Hp=25;const auto safe=Encounter::Create(run,seed,nullptr,true);
        check(safe.PotentialEnemyDamage(0)<25,"固定戦場でも強撃込みの初手即死を防ぐ");
    }
    check(seen,"5日目以降に鬼武者が出現する");
    for(int invalid=0;invalid<2;++invalid) {
        BattleUnit hero;hero.At={1,1};hero.Role=invalid==0?oni:static_cast<BattleRole>(7);
        BattleUnit enemy;enemy.Side=Faction::Enemy;enemy.Id=100;enemy.At={5,5};
        try{Battle illegal(8,8,std::vector<Tile>(64),{hero,enemy},42);check(false,"鬼武者を味方に設定/不正な役割を拒否");}
        catch(const std::invalid_argument&){check(true,"鬼武者を味方に設定/不正な役割を拒否");}
    }
    const auto frame=DecodeSave(std::string_view(LegacyRun5Combat,sizeof(LegacyRun5Combat)-1),SaveKind::Run);
    const auto legacy=frame.Value?DecodeRunPayload(frame.Value->Payload):std::nullopt;
    check(legacy && legacy->Combat && InspectPayload(frame.Value->Payload,SaveKind::Run)==SaveError::None,"固定した旧v5戦闘を受け入れる");
    const auto migrated=legacy?EncodeRunPayload(*legacy):std::nullopt;
    check(migrated && DecodeRunPayload(*migrated)==legacy,"旧v5を全状態を失わずv6へ保存する");
    Run run(42,Difficulty::Beginner,HeroRole::Sword);run.ChooseNode(0,0);
    auto setup=Encounter::Create(run.Snapshot(),run.RandomState()).Checkpoint();setup.State.Tiles.assign(setup.State.Tiles.size(),Tile{});
    for(auto& unit:setup.State.Units)if(unit.Side==Faction::Enemy){unit.Role=oni;unit.At={2,1};unit.Movement=2;unit.Attack=16;unit.Skills[0]=true;}
    auto actual=Battle::Restore(setup);
    check(actual && actual->EndTurn(0,0,Facing::South) && ApplyBasicAction(*actual,ChooseBasicAction(*actual)),"実戦条件で鬼武者の強撃後を準備");
    if(actual) {
        RunSaveState saved{run.Checkpoint(),SavedCombat{actual->Checkpoint(),CombatControl{}.Checkpoint()}};
        const auto bytes=EncodeRunPayload(saved);
        check(bytes && DecodeRunPayload(*bytes)==saved,"鬼武者と強撃再使用待ちをv6へ保存復元");
        if(bytes){auto forged=*bytes;forged[4]=5;check(!DecodeRunPayload(forged),"旧v5へ新しい鬼武者役割を紛れ込ませない");}
    }
    std::cout<<"OniEnemy: "<<checks<<" checks, "<<failed<<" failed\n";
    return failed;
}
