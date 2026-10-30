#include "Domain/Battle.h"
#include "Generation/Battlefield.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>

int TestTerrain()
{
    using namespace GirlsSpire;
    int checks=0,failed=0;
    const auto check=[&](bool pass,const char* label) { ++checks; if(!pass) { ++failed; std::cerr<<"FAIL Terrain: "<<label<<'\n'; } };
    BattleUnit hero; hero.At={1,1}; hero.Speed=20; hero.Attack=12; hero.Range=4;
    BattleUnit enemy; enemy.Id=100; enemy.Side=Faction::Enemy; enemy.At={5,1}; enemy.Movement=0;
    std::vector<Tile> tiles(64); tiles[10].ObstacleHp=20;
    const auto make=[&] { return Battle(8,8,tiles,{hero,enemy},42); };
    auto battle=make();
    const auto before=battle.Snapshot(); const auto rng=battle.RandomState();
    const auto reachable=battle.Reachable(0);
    check(std::find(reachable.begin(),reachable.end(),Position{2,1})==reachable.end(),"障害物に停止不可");
    check(!battle.HasLineOfSight({1,1},{5,1}) && !battle.HasLineOfSight({5,1},{1,1}),"射線を双方向に遮断");
    check(!battle.HasLineOfSight({1,1},{2,1}),"障害物マスへの通常射線は遮断");
    check(battle.ConnectedTerrain({2,1}).empty(),"障害物起点は接続なし");
    check(battle.ConnectedTerrain({1,1}).size()==63,"障害物を通行領域から除外");
    auto preview=battle.PreviewAttack(0,{2,1});
    check(preview.Legal && preview.Hits.empty() && preview.TerrainDamage==12,"障害物の確定被害予告");
    check(battle.Snapshot()==before && battle.RandomState()==rng,"予告は状態乱数不変");
    check(!battle.Attack(99,0,{2,1}) && battle.Snapshot()==before,"古い入力は拒否");
    check(!battle.PreviewAttack(0,{2,1},1).Legal,"地形破壊は通常単体攻撃のみ");
    check(battle.Attack(0,0,{2,1}),"障害物を攻撃");
    check(battle.Snapshot().Tiles[10].ObstacleHp==8 && battle.Snapshot().Acted && battle.Snapshot().Revision==1,"耐久減少と行動消費");
    check(battle.RandomState()==rng && battle.LastHits().empty(),"確定命中は乱数不消費・人物命中履歴なし");
    const auto once=battle.Snapshot();
    check(!battle.Attack(1,0,{2,1}) && battle.Snapshot()==once,"二回行動拒否");
    battle.EndTurn(1,0,Facing::South); battle.EndTurn(2,100,Facing::South);
    preview=battle.PreviewAttack(0,{2,1});
    check(preview.TerrainDamage==8,"残耐久までの被害予告");
    check(battle.Attack(3,0,{2,1}) && battle.Snapshot().Tiles[10]==Tile{},"破壊で平地");
    check(battle.HasLineOfSight({1,1},{5,1}),"破壊後射線回復");
    battle.EndTurn(4,0,Facing::South); battle.EndTurn(5,100,Facing::South);
    check(!battle.PreviewAttack(0,{2,1}).Legal,"空いた平地への攻撃拒否");
    check(battle.Move(6,0,{2,1}),"破壊後に進入");
    hero.Range=1; tiles[10].ObstacleHp=0; tiles[11].ObstacleHp=20;
    check(!make().PreviewAttack(0,{3,1}).Legal,"射程外の障害物");
    hero.Range=4; tiles[10].Wall=true;
    check(!make().PreviewAttack(0,{3,1}).Legal,"手前の壁が射線を遮る");
    tiles[10].Wall=false; tiles[10].ObstacleHp=20;
    check(!make().PreviewAttack(0,{3,1}).Legal,"手前の別障害物が射線を遮る");
    tiles[10].ObstacleHp=0; tiles[11].ObstacleHp=20; hero.Attack=0;
    auto zero=make();
    check(zero.PreviewAttack(0,{3,1}).Legal && zero.PreviewAttack(0,{3,1}).TerrainDamage==0,"攻撃力0を水増ししない");
    check(zero.Attack(0,0,{3,1}) && zero.Snapshot().Tiles[11].ObstacleHp==20 && zero.Snapshot().Acted,"0被害でも行動消費");
    hero.Attack=1000; tiles[11].Elevation=2; tiles[11].Cover=20;
    auto strong=make(); check(strong.Attack(0,0,{3,1}) && strong.Snapshot().Tiles[11]==Tile{},"過剰被害でも耐久0・平地化");
    enemy.Speed=30; enemy.Range=4; enemy.Attack=20;
    auto enemyTurn=make(); check(enemyTurn.Attack(0,100,{3,1}) && enemyTurn.Snapshot().Tiles[11]==Tile{},"敵も同じ破壊規則");
    for(int invalid : {-1,21}) {
        tiles[11].ObstacleHp=invalid; bool threw=false;
        try { (void)make(); } catch(const std::invalid_argument&) { threw=true; }
        check(threw,"耐久範囲外拒否");
    }
    tiles[11].ObstacleHp=20; tiles[11].Wall=true;
    bool threw=false; try { (void)make(); } catch(const std::invalid_argument&) { threw=true; }
    check(threw,"壁と障害物の重複拒否");
    tiles[11].Wall=false; hero.At={3,1}; threw=false;
    try { (void)make(); } catch(const std::invalid_argument&) { threw=true; }
    check(threw,"人物と障害物の重複拒否");
    hero.At={1,1}; hero.Attack=12; enemy.Speed=10; enemy.Attack=12;
    auto layout=TerrainLayout{8,8,std::vector<Tile>(64)}; layout.Tiles[10].ObstacleHp=20;
    check(Battlefield::Validate(layout,{hero,enemy},BattleGoal::Eliminate)==FieldFailure::None,"障害物があっても迂回できれば生成有効");
    for(int y=0;y<8;++y) layout.Tiles[y*8+2].ObstacleHp=20;
    check(Battlefield::Validate(layout,{hero,enemy},BattleGoal::Eliminate)==FieldFailure::Disconnected,"障害物による完全分断は拒否");
    bool generated=false;
    for(int seed=0;seed<100;++seed) {
        const auto field=Battlefield::Generate(seed,{hero,enemy});
        for(const auto& tile:field.Layout.Tiles) generated=generated || tile.ObstacleHp>0;
        check(Battlefield::Validate(field.Layout,{hero,enemy},BattleGoal::Eliminate)==FieldFailure::None,"100シードの障害物付き安全検証");
    }
    check(generated,"自動生成に障害物を含む");
    std::cout<<"Terrain: "<<checks<<" checks, "<<failed<<" failed\n";
    return failed;
}
