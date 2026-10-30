#include "Application/CombatControl.h"
#include <iostream>
#include <limits>

using namespace GirlsSpire;
int TestCombatControl()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* label) {
        ++checks; if(!ok) {++failed;std::cerr<<"FAIL CombatControl: "<<label<<'\n';}
    };
    BattleUnit ally;ally.Id=1;ally.At={1,1};ally.Speed=20;
    BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={6,6};
    const Battle source(8,8,std::vector<Tile>(64),{ally,enemy},42);
    CombatControl control;
    check(control.Checkpoint()==CombatControlCheckpoint{},"初期状態");
    const auto roundtrip=[&](const CombatControl& original,const Battle& battle) {
        const auto saved=original.Checkpoint();
        auto restored=CombatControl::Restore(saved,battle);
        check(restored.has_value(),"復元成功");
        if(restored) check(restored->Checkpoint()==saved,"全作戦・信頼・抽選位置・表示モード一致");
        check(original.Checkpoint()==saved,"元状態を変更しない");
        return restored;
    };
    roundtrip(control,source);
    for(int id=0;id<9;++id) {
        control.Trust[id]=id*12;
        control.Orders[id].Kind=static_cast<TacticalIntent>(id%4);
        if(id%2) control.Orders[id].TargetId=100;
        else control.Orders[id].Point=Position{7,7};
    }
    control.ProfileDriven=true;
    control.ChoiceRandom=Random(std::numeric_limits<std::uint64_t>::max());
    roundtrip(control,source);
    auto dead=source.Checkpoint();dead.State.Units[1].Hp=0;
    dead.State.Outcome=BattleOutcome::Won;dead.State.ActiveId=-1;
    auto finished=Battle::Restore(dead);
    check(finished.has_value(),"終了戦闘の準備");
    if(finished) roundtrip(control,*finished); // 倒れた対象への指示も履歴として残す。
    const auto valid=control.Checkpoint();
    const auto reject=[&](CombatControlCheckpoint bad) {
        const auto before=bad;
        check(!CombatControl::Restore(bad,source),"不正状態を拒否");
        check(bad==before && control.Checkpoint()==valid,"失敗時に入力と元状態を保持");
    };
    for(int id=0;id<9;++id) {
        for(int trust : {-1,101}) {auto bad=valid;bad.Trust[id]=trust;reject(bad);}
        for(int kind : {-1,4}) {auto bad=valid;bad.Orders[id].Kind=static_cast<TacticalIntent>(kind);reject(bad);}
        auto bad=valid;bad.Orders[id].TargetId=999;bad.Orders[id].Point.reset();reject(bad);
        bad=valid;bad.Orders[id].TargetId=100;bad.Orders[id].Point=Position{1,1};reject(bad);
        for(const Position point : {Position{-1,0},{0,-1},{8,0},{0,8}}) {
            bad=valid;bad.Orders[id].TargetId.reset();bad.Orders[id].Point=point;reject(bad);
        }
    }
    for(int trust : {0,24,25,49,50,74,75,100}) for(int seed=0;seed<100;++seed) {
        CombatControl uninterrupted;
        uninterrupted.Trust.fill(trust);
        uninterrupted.Orders[1]={TacticalIntent::Assault,100,{}};
        uninterrupted.ChoiceRandom=Random(seed);
        uninterrupted.ChoiceRandom.Next(); // 戦闘シードからの初期化では復元できない消費済み位置。
        auto resumed=roundtrip(uninterrupted,source);
        if(!resumed) continue;
        auto a=source,b=source;
        const std::vector<BasicAction> ranked={{BasicActionKind::Move,{2,1}},{BasicActionKind::Move,{1,2}},{BasicActionKind::Wait,{}}};
        const auto first=IntentForecast::Build(a,ranked,uninterrupted.Trust[1]);
        const auto second=IntentForecast::Build(b,ranked,resumed->Trust[1]);
        check(first.Candidates()==second.Candidates(),"復元後の確率と候補一致");
        check(first.Execute(a,uninterrupted.ChoiceRandom) && second.Execute(b,resumed->ChoiceRandom),"復元前後とも抽選行動成功");
        check(a.Checkpoint()==b.Checkpoint(),"戦闘結果と戦闘乱数一致");
        check(uninterrupted.Checkpoint()==resumed->Checkpoint(),"仲間抽選の消費位置一致");
    }
    std::cout<<"CombatControl: "<<checks<<" checks, "<<failed<<" failed\n";
    return failed;
}
