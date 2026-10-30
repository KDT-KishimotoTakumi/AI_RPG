#include "Domain/Battle.h"
#include "AI/BasicTurn.h"
#include <functional>
#include <iostream>
using namespace GirlsSpire;
int TestBattleRestore()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL BattleRestore: "<<why<<'\n';}};
    const auto verify=[&](const Battle& battle) {
        const auto saved=battle.Checkpoint();auto restored=Battle::Restore(saved);
        check(restored.has_value(),"戦闘途中・終了状態の復元");
        if(!restored) return;
        check(restored->Checkpoint()==saved,"地形・履歴・乱数・手番・結果一致");
        if(battle.Snapshot().Outcome==BattleOutcome::Ongoing) {
            auto original=battle;const auto action=ChooseBasicAction(original);
            check(ChooseBasicAction(*restored)==action,"復元後の行動候補一致");
            check(ApplyBasicAction(original,action)==ApplyBasicAction(*restored,action),"復元後の行動成功一致");
            check(original.Checkpoint()==restored->Checkpoint(),"行動結果・次手番・乱数一致");
        }
    };
    std::vector<BattleUnit> units;
    for(int id=0;id<5;++id) {BattleUnit u;u.Id=id;u.At={id+1,1};u.Range=8;u.Speed=50-id;u.Skills.fill(true);units.push_back(u);}
    BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={6,1};enemy.Hp=enemy.MaxHp=400;enemy.Range=8;units.push_back(enemy);
    BattleUnit neutral;neutral.Id=200;neutral.Side=Faction::Neutral;neutral.At={1,5};units.push_back(neutral);
    for(int seed=0;seed<100;++seed) for(auto goal:{BattleGoal::Eliminate,BattleGoal::Survive,BattleGoal::Rescue}) {
        Battle battle(8,8,std::vector<Tile>(64),units,seed,goal,3,2);
        for(int n=0;n<100 && battle.Snapshot().Outcome==BattleOutcome::Ongoing;++n) {
            verify(battle);
            const auto s=battle.Snapshot();const int id=s.ActiveId;
            if(id<5 && !s.Acted && battle.PreviewSkill(id,Skill::PowerStrike,{6,1}).Legal)
                battle.UseSkill(s.Revision,id,Skill::PowerStrike,{6,1});
            else ApplyBasicAction(battle,ChooseBasicAction(battle));
        }
        verify(battle);
    }
    Battle original(8,8,std::vector<Tile>(64),units,42);const auto saved=original.Checkpoint();
    const auto reject=[&](const std::function<void(BattleCheckpoint&)>& mutate) {
        auto bad=saved;mutate(bad);const auto before=bad;
        check(!Battle::Restore(bad),"不正状態拒否");
        check(bad==before && original.Checkpoint()==saved,"入力・元戦闘不変");
    };
    reject([](auto& c){c.State.Width=0;});reject([](auto& c){c.State.Tiles.pop_back();});
    reject([](auto& c){c.State.Units.clear();});reject([](auto& c){c.State.Units[1].Id=0;});
    reject([](auto& c){c.State.Units[0].Hp=-1;});reject([](auto& c){c.State.Units[0].Cooldowns[0]=4;});
    reject([](auto& c){c.State.Round=0;});reject([](auto& c){c.State.ActiveId=100;});
    reject([](auto& c){c.TurnIndex=100;});reject([](auto& c){c.State.Order[0]=999;});
    reject([](auto& c){c.State.Order.push_back(0);});reject([](auto& c){c.State.Order.pop_back();});
    reject([](auto& c){c.State.Combos.Round=2;});reject([](auto& c){c.State.Combos.LastSequence=1;});
    reject([](auto& c){c.State.Combos.Gauge=101;});reject([](auto& c){c.State.BondGains.Aid[0]=4;});
    reject([](auto& c){c.State.UsedSkill=5;});reject([](auto& c){c.State.Outcome=BattleOutcome::Won;});
    reject([](auto& c){c.Goal=static_cast<BattleGoal>(3);});reject([](auto& c){c.SurvivalRounds=0;});
    reject([](auto& c){c.LastHits={{999,true,1}};});reject([](auto& c){c.LastHits={{100,false,1}};});
    reject([](auto& c){c.State.LastCombo=ComboResolution{1,{0,0},10};});
    reject([](auto& c){std::swap(c.State.Order[0],c.State.Order[1]);});
    reject([](auto& c){c.State.UsedSkill=0;});
    reject([](auto& c){c.State.Revision=1;c.State.Combos.LastSequence=1;c.State.Combos.Pending={{1,999}};});
    reject([](auto& c){c.State.Revision=1;c.State.Combos.LastSequence=1;c.State.Combos.Pending={{1,0,16,{}}};});
    reject([](auto& c){c.State.Revision=1;c.State.Combos.LastSequence=1;c.State.Combos.Pending={{1,0,1,{999}}};});
    reject([](auto& c){c.State.LastCombo=ComboResolution{0,{0,1},10};});
    auto injured=units;injured[1].Hp=10;
    Battle medicine(8,8,std::vector<Tile>(64),injured,42,BattleGoal::Eliminate,3,2);
    check(medicine.UseMedicine(0,0,{2,1}),"傷薬使用の準備");verify(medicine);
    check(medicine.Snapshot().Medicines==1 && medicine.Snapshot().BondGains.Aid[0]==1,"傷薬残数・援護獲得を含む");
    std::vector<Tile> terrain(64);terrain[17].Trap=true;terrain[18].ObstacleHp=12;
    Battle changed(8,8,terrain,units,42);
    check(changed.Move(0,0,{1,2}),"罠移動の準備");verify(changed);
    check(changed.Attack(changed.Snapshot().Revision,0,{2,2}),"地形破壊の準備");verify(changed);
    check(!changed.Snapshot().Tiles[17].Trap && changed.Snapshot().Tiles[18].ObstacleHp==0,"変化済み地形を含む");
    auto ties=units;for(auto& unit:ties) unit.Speed=10;
    verify(Battle(8,8,std::vector<Tile>(64),ties,42));
    auto defeated=units;for(auto& unit:defeated) unit.Hp=0;
    verify(Battle(8,8,std::vector<Tile>(64),defeated,42));
    // 倒れた人物のマスへ生存者が移動した状態を保持できる。
    auto overlap=saved;overlap.State.Units[1].Hp=0;overlap.State.Units[1].At=overlap.State.Units[0].At;
    auto restored=Battle::Restore(overlap);
    check(restored && restored->Checkpoint()==overlap,"倒れた人物とのマス重複は合法");
    overlap.State.Units[1].Hp=1;check(!Battle::Restore(overlap),"生存者同士の重複拒否");
    auto lost=saved;for(auto& unit:lost.State.Units) if(unit.Side==Faction::Player) unit.Hp=0;
    lost.State.ActiveId=-1;lost.State.Outcome=BattleOutcome::Lost;
    check(Battle::Restore(lost).has_value(),"敗北の復元");
    std::cout<<"BattleRestore: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
