#include "Domain/Battle.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace GirlsSpire;
int TestMedicine()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL Medicine: "<<why<<'\n';}};
    for(int seed=0;seed<100;++seed) for(int actor:{0,1}) for(int hp:{1,39,40}) {
        BattleUnit a;a.Id=actor;a.At={2,2};a.MaxHp=40;a.Hp=hp;a.Speed=30;
        BattleUnit b;b.Id=actor==0?1:0;b.At={2,3};b.MaxHp=40;b.Hp=1;
        BattleUnit e;e.Id=100;e.Side=Faction::Enemy;e.At={7,7};
        Battle battle(8,8,std::vector<Tile>(64),{a,b,e},seed,BattleGoal::Eliminate,3,2);
        const auto before=battle.Snapshot();const auto rng=battle.RandomState();
        const auto self=battle.PreviewMedicine(actor,a.At);
        check(self.Legal==(hp<40) && self.Healing==(hp<40?std::min(30,40-hp):0),"自身の回復予告・上限");
        check(battle.PreviewMedicine(actor,b.At).Legal,"上下左右の味方");
        for(Position invalid: {Position{-1,0},Position{8,8},Position{3,3},e.At})
            check(!battle.PreviewMedicine(actor,invalid).Legal && !battle.UseMedicine(0,actor,invalid) && battle.Snapshot()==before,"範囲外・空・敵は無消費");
        check(!battle.UseMedicine(1,actor,b.At) && battle.Snapshot()==before,"古い予告拒否");
        auto copy=battle;
        check(battle.UseMedicine(0,actor,b.At),"主人公・仲間の手動使用");
        copy.UseMedicine(0,actor,b.At);
        check(battle.Snapshot()==copy.Snapshot() && battle.RandomState()==rng,"決定的・乱数不変");
        check(battle.Snapshot().Medicines==1 && battle.Snapshot().Acted && battle.Snapshot().Revision==1 && battle.Snapshot().Units[1].Hp==31,"1個・1行動・30回復");
        check(battle.Snapshot().Combos==before.Combos,"連携ゲージを獲得しない");
        const auto used=battle.Snapshot();
        check(!battle.UseMedicine(0,actor,b.At) && !battle.UseMedicine(1,actor,b.At) && battle.Snapshot()==used,"二重使用と行動済み拒否");
    }
    BattleUnit a;a.At={1,1};a.Hp=1;a.Speed=30;
    BattleUnit e;e.Id=100;e.Side=Faction::Enemy;e.At={7,7};
    for(int count:{-1,6}) {
        bool threw=false;try{Battle b(8,8,std::vector<Tile>(64),{a,e},0,BattleGoal::Eliminate,3,count);}catch(const std::invalid_argument&){threw=true;}
        check(threw,"不正薬数拒否");
    }
    Battle empty(8,8,std::vector<Tile>(64),{a,e},0);
    check(!empty.PreviewMedicine(0,a.At).Legal && !empty.UseMedicine(0,0,a.At),"薬なし");
    for(int scenario=0;scenario<5;++scenario) {
        auto actor=a;BattleUnit ally;ally.Id=1;ally.At={2,1};ally.Hp=1;
        if(scenario==0) ally.Hp=0;
        if(scenario==1) ally.At={2,2};
        if(scenario==2) ally.At={3,1};
        if(scenario==3) ally.Side=Faction::Neutral;
        if(scenario==4) actor.Side=Faction::Enemy;
        Battle b(8,8,std::vector<Tile>(64),{actor,ally,e},0,BattleGoal::Eliminate,3,1);
        const auto before=b.Snapshot();
        check(!b.PreviewMedicine(0,ally.At).Legal && !b.UseMedicine(0,0,ally.At) && b.Snapshot()==before,"倒れた味方・斜め・遠方・中立・敵の使用拒否");
        check(!b.PreviewMedicine(999,ally.At).Legal,"存在しない使用者拒否");
    }
    std::cout<<"Medicine: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
