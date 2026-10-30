#include "Generation/Battlefield.h"
#include <iostream>
#include <set>
#include <stdexcept>

int TestGeneration()
{
    using namespace GirlsSpire;
    int checks=0,failed=0;
    const auto check=[&](bool pass,const char* label) { ++checks; if(!pass) { ++failed; std::cerr<<"FAIL Generation: "<<label<<'\n'; } };
    BattleUnit hero; hero.Id=0; hero.At={1,1}; hero.Hp=hero.MaxHp=80; hero.Speed=20;
    BattleUnit enemy; enemy.Id=100; enemy.Side=Faction::Enemy; enemy.At={4,1}; enemy.Attack=10;
    BattleUnit target; target.Id=200; target.Side=Faction::Neutral; target.At={6,6};
    const std::vector<BattleUnit> units={hero,enemy};
    TerrainLayout plain{8,8,std::vector<Tile>(64)};
    check(Battlefield::Validate(plain,units,BattleGoal::Eliminate)==FieldFailure::None,"平坦な戦場は有効");
    auto battle=Battle(8,8,plain.Tiles,units,42);
    check(battle.ConnectedTerrain({1,1}).size()==64,"地形接続範囲");
    check(battle.ConnectedTerrain({-1,0}).empty() && battle.ConnectedTerrain({8,0}).empty(),"盤外開始を拒否");
    auto blockedStart=plain; blockedStart.Tiles[0].Wall=true;
    check(Battle(8,8,blockedStart.Tiles,units,42).ConnectedTerrain({0,0}).empty(),"壁の開始位置は接続なし");
    auto separated=plain; for(int y=0;y<8;++y) separated.Tiles[y*8+3].Wall=true;
    check(Battlefield::Validate(separated,units,BattleGoal::Eliminate)==FieldFailure::Disconnected,"敵側へ到達不能なら拒否");
    auto tall=plain; for(int y=0;y<8;++y) tall.Tiles[y*8+3].Elevation=3;
    check(Battlefield::Validate(tall,units,BattleGoal::Eliminate)==FieldFailure::Disconnected,"越えられない段差は拒否");
    auto invalid=plain; invalid.Tiles.pop_back();
    check(Battlefield::Validate(invalid,units,BattleGoal::Eliminate)==FieldFailure::Invalid,"タイル数不正を拒否");
    check(Battlefield::Validate(plain,{},BattleGoal::Eliminate)==FieldFailure::Invalid,"参加者なしを拒否");
    check(Battlefield::Validate(plain,units,BattleGoal::Rescue)==FieldFailure::Invalid,"救出対象不在を拒否");
    auto lethal=units; lethal[0].Hp=8;
    check(Battlefield::Validate(plain,lethal,BattleGoal::Eliminate)==FieldFailure::Unsafe,"初期即死の恐れを拒否");
    auto ended=units; ended[1].Hp=0;
    check(Battlefield::Validate(plain,ended,BattleGoal::Eliminate)==FieldFailure::Invalid,"開始時に勝利済みを拒否");
    const auto fallback=Battlefield::Generate(42,units,BattleGoal::Eliminate,0);
    check(fallback.UsedFallback && fallback.Attempts==0,"試行0は固定戦場");
    check(fallback.Layout.Width==12 && fallback.Layout.Height==12,"固定戦場の寸法");
    check(fallback.Layout==Battlefield::Generate(99,units,BattleGoal::Eliminate,0).Layout,"固定戦場の形状はシードに依存しない");
    check(Battlefield::Validate(fallback.Layout,units,BattleGoal::Eliminate)==FieldFailure::None,"固定戦場も同じ検証を通る");
    int tries=0;
    const auto exhausted=Battlefield::Generate(42,units,BattleGoal::Eliminate,8,[&](Random&) { ++tries; return TerrainLayout{}; });
    check(tries==8 && exhausted.Attempts==8 && exhausted.UsedFallback,"検証失敗を8回再試行してから退避");
    tries=0;
    const auto recovered=Battlefield::Generate(42,units,BattleGoal::Eliminate,8,[&](Random&) { return ++tries==1?TerrainLayout{}:plain; });
    check(tries==2 && recovered.Attempts==2 && !recovered.UsedFallback,"再試行で有効な戦場が得られたら採用");
    std::set<std::pair<int,int>> dimensions; int automatic=0; bool changed=false;
    bool sawWall=false,sawElevation=false,sawCover=false;
    const auto first=Battlefield::Generate(0,units);
    for(int seed=0;seed<100;++seed) {
        for(auto goal : {BattleGoal::Eliminate,BattleGoal::Survive,BattleGoal::Rescue}) {
            auto participants=units; if(goal==BattleGoal::Rescue) participants.push_back(target);
            const auto a=Battlefield::Generate(seed,participants,goal),b=Battlefield::Generate(seed,participants,goal);
            check(a==b,"シード・目標・試行状態が再現");
            check(a.Layout.Width>=8 && a.Layout.Width<=12 && a.Layout.Height>=8 && a.Layout.Height<=12,"8～12の寸法");
            check(a.Attempts>=1 && a.Attempts<=8,"再試行は上限以内");
            check(Battlefield::Validate(a.Layout,participants,goal)==FieldFailure::None,"100シード全目標の検証");
            if(!a.UsedFallback) ++automatic;
            dimensions.insert({a.Layout.Width,a.Layout.Height});
            changed=changed || a.Layout!=first.Layout;
            for(const auto& tile:a.Layout.Tiles) {
                sawWall=sawWall || tile.Wall; sawElevation=sawElevation || tile.Elevation>0; sawCover=sawCover || tile.Cover>0;
            }
            Battle actual(a.Layout.Width,a.Layout.Height,a.Layout.Tiles,participants,seed,goal);
            check(actual.PotentialEnemyDamage(0)<hero.Hp,"生成した戦闘で味方HPが初期被害上限を上回る");
            check(actual.ConnectedTerrain(hero.At).size()>participants.size(),"人物以外にも接続した歩行領域がある");
        }
    }
    check(automatic>0 && changed && dimensions.size()>1,"固定戦場だけでなく異なる地形を生成");
    check(sawWall && sawElevation && sawCover,"壁・高低差・遮蔽を生成");
    auto edgeUnits=units; edgeUnits[1].At={11,11};
    const auto edge=Battlefield::Generate(42,edgeUnits);
    check(edge.Layout.Width==12 && edge.Layout.Height==12,"12マス端の人物を配置できる寸法へ調整");
    auto reordered=units; std::swap(reordered[0],reordered[1]);
    check(Battlefield::Generate(42,units).Layout==Battlefield::Generate(42,reordered).Layout,"参加者配列順で地形が変わらない");
    auto outside=units; outside[0].At={12,0};
    try { (void)Battlefield::Generate(42,outside); check(false,"最大戦場でも盤外の人物は拒否"); }
    catch(const std::invalid_argument&) { check(true,"最大戦場でも盤外の人物は拒否"); }
    for(int cap : {-1,9}) {
        try { (void)Battlefield::Generate(42,units,BattleGoal::Eliminate,cap); check(false,"無効な試行数拒否"); }
        catch(const std::invalid_argument&) { check(true,"無効な試行数拒否"); }
    }
    try { (void)Battlefield::Generate(42,lethal,BattleGoal::Eliminate,0); check(false,"固定戦場でも危険なら明示失敗"); }
    catch(const std::invalid_argument&) { check(true,"固定戦場でも危険なら明示失敗"); }
    std::cout<<"Generation: "<<checks<<" checks, "<<failed<<" failed\n";
    return failed;
}
