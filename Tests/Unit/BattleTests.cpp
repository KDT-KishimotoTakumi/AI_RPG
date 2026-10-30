#include "Domain/Battle.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
GirlsSpire::BattleUnit Unit(int id, GirlsSpire::Faction side, int x, int y, int speed = 10)
{
    GirlsSpire::BattleUnit unit;
    unit.Id = id; unit.Side = side; unit.At = {x,y}; unit.Speed = speed;
    return unit;
}
}

int TestBattle()
{
    using namespace GirlsSpire;
    int checks = 0, failed = 0;
    const auto check = [&](bool ok, const char* label) {
        ++checks; if (!ok) { ++failed; if (failed <= 15) std::cerr << "FAIL Battle: " << label << '\n'; }
    };
    const auto has = [](const std::vector<Position>& positions, Position at) {
        return std::find(positions.begin(), positions.end(), at) != positions.end();
    };
    auto hero = Unit(1, Faction::Player, 1, 1, 20);
    auto ally = Unit(3, Faction::Player, 2, 1, 10);
    auto enemy = Unit(2, Faction::Enemy, 4, 1, 10);
    Battle movement(8,8,std::vector<Tile>(64),{enemy,ally,hero},42);
    check(movement.Snapshot().Order == std::vector<int>({1,3,2}), "素早さ降順・同速は味方優先");
    check(movement.Snapshot().ActiveId == 1, "最速者から開始");
    check(has(movement.Reachable(1), {3,1}), "味方を通過できる");
    check(!has(movement.Reachable(1), {2,1}), "味方の上には止まれない");
    check(!has(movement.Reachable(1), {4,1}), "敵の上には止まれない");
    check(!has(movement.Reachable(1), {1,1}), "現在位置は移動候補外");
    check(movement.Reachable(2).empty() && movement.Reachable(999).empty(), "手番外の移動拒否");
    auto before = movement.Snapshot();
    auto rng = movement.RandomState();
    check(!movement.Move(999,1,{3,1}) && !movement.Move(0,1,{-1,1}) && !movement.Move(0,2,{3,1}), "不正な移動入力を拒否");
    check(movement.Snapshot() == before && movement.RandomState() == rng, "無効操作は状態・乱数不変");
    check(movement.Move(0,1,{3,1}), "移動を実行");
    check(!movement.Move(movement.Snapshot().Revision,1,{3,2}), "移動は一手番一回");
    auto forecast = movement.PreviewAttack(1,{4,1});
    check(forecast.Legal && forecast.Hits == std::vector<PredictedHit>({{2,85,10}}), "命中・ダメージ予告");
    check(movement.RandomState() == rng, "予告で乱数を消費しない");
    check(!movement.Attack(0,1,{4,1}), "古い世代の攻撃拒否");
    check(movement.Attack(movement.Snapshot().Revision,1,{4,1}), "攻撃を実行");
    check(!movement.Attack(movement.Snapshot().Revision,1,{4,1}), "二重攻撃を拒否");
    check(!movement.EndTurn(movement.Snapshot().Revision,1,static_cast<Facing>(9)), "不正な向きを拒否");
    check(movement.EndTurn(movement.Snapshot().Revision,1,Facing::East), "向きを決めて終了");
    check(movement.Snapshot().ActiveId == 3 && !movement.Snapshot().Moved && !movement.Snapshot().Acted, "次の味方手番");
    movement.EndTurn(movement.Snapshot().Revision,3,Facing::North);
    check(movement.Snapshot().ActiveId == 2, "敵味方混在の順番");
    movement.EndTurn(movement.Snapshot().Revision,2,Facing::West);
    check(movement.Snapshot().Round == 2 && movement.Snapshot().ActiveId == 1, "ラウンド更新");

    for (auto role : {BattleRole::Standard,BattleRole::Dancer,BattleRole::Ninja,BattleRole::Archer,BattleRole::Gunner}) {
        auto actor = hero; actor.Role = role;
        auto target = enemy; target.At = {1,2};
        Battle battle(8,8,std::vector<Tile>(64),{actor,target},42);
        battle.Attack(0,1,{1,2});
        const bool mayMove = role == BattleRole::Dancer || role == BattleRole::Ninja;
        check(battle.Move(battle.Snapshot().Revision,1,{2,1}) == mayMove, "役割別の行動後移動");
    }

    auto tiles = std::vector<Tile>(64);
    tiles[1 * 8 + 2].Wall = true;
    tiles[2 * 8 + 1].Elevation = 2;
    Battle walls(8,8,tiles,{hero,enemy},42);
    check(!walls.HasLineOfSight({1,1},{4,1}), "壁で射線が切れる");
    check(!walls.HasLineOfSight({1,1},{2,2}), "壁の角越しに攻撃しない");
    check(!walls.HasLineOfSight({1,1},{1,3}), "高い地形が射線を遮る");
    check(walls.HasLineOfSight({1,1},{0,1}), "隣接した開放マス");
    check(!walls.HasLineOfSight({-1,0},{0,0}) && !walls.HasLineOfSight({0,0},{8,0}), "盤外射線は無効");
    check(!has(walls.Reachable(1),{1,2}) && !has(walls.Reachable(1),{2,1}), "高低差2と壁は移動不可");
    for (int y = 0; y < 8; ++y) for (int x = 0; x < 8; ++x)
        check(walls.HasLineOfSight({1,1},{x,y}) == walls.HasLineOfSight({x,y},{1,1}), "射線の対称性");

    hero.Range = 6;
    auto near = enemy; near.Id = 4; near.At = {4,2};
    auto friendly = ally; friendly.At = {3,2};
    tiles.assign(64, {}); tiles[1 * 8 + 4].Cover = 30;
    Battle area(8,8,tiles,{hero,enemy,near,friendly},7);
    const auto splash = area.PreviewAttack(1,{4,1},1);
    check(splash.Legal && splash.Hits.size() == 2, "範囲内の敵を対象にする");
    if (!splash.Hits.empty()) check(splash.Hits[0].Chance == 55, "遮蔽で命中低下");
    check(!area.PreviewAttack(1,{2,1},0).Legal, "対象なし攻撃拒否");
    check(!area.PreviewAttack(1,{3,2}).Legal, "味方への故意攻撃拒否");
    check(!area.PreviewAttack(1,{4,1},-1).Legal && !area.PreviewAttack(1,{4,1},4).Legal, "範囲の上限下限");
    check(!area.PreviewAttack(99,{4,1}).Legal && !area.PreviewAttack(1,{7,7}).Legal, "不存在と射程外");
    check(area.Attack(0,1,{4,1},1) && area.LastHits().size() == 2, "範囲攻撃の結果");

    for (std::uint64_t seed = 0; seed < 100; ++seed) {
        auto target = enemy; target.At = {1,2}; target.Hp = target.MaxHp = 1;
        Battle a(8,8,std::vector<Tile>(64),{hero,target},seed);
        Battle b(8,8,std::vector<Tile>(64),{hero,target},seed);
        const auto predicted = a.PreviewAttack(1,{1,2});
        a.Attack(0,1,{1,2}); b.Attack(0,1,{1,2});
        check(a.Snapshot() == b.Snapshot() && a.LastHits() == b.LastHits() && a.RandomState() == b.RandomState(), "固定シード攻撃再現");
        if (!a.LastHits().empty()) {
            const auto hit = a.LastHits()[0];
            check(hit.Damage == (hit.Hit ? 1 : 0), "過剰ダメージでHPを負にしない");
            check(a.Snapshot().Outcome == (hit.Hit ? BattleOutcome::Won : BattleOutcome::Ongoing), "全滅時勝利・回避時継続");
            if (hit.Hit) check(!a.EndTurn(a.Snapshot().Revision,1,Facing::South), "終了後の操作拒否");
        }
        check(predicted.Legal, "シードによらず同じ合法予告");
    }
    auto fragile = hero; fragile.Hp = fragile.MaxHp = 1; fragile.Speed = 0;
    auto killer = enemy; killer.At={1,2}; killer.Accuracy=200;
    Battle defeat(8,8,std::vector<Tile>(64),{fragile,killer},42);
    check(defeat.Attack(0,2,{1,1}) && defeat.Snapshot().Outcome == BattleOutcome::Lost, "味方全員戦闘不能で敗北");
    Battle survival(8,8,std::vector<Tile>(64),{hero,enemy},42,BattleGoal::Survive,2);
    for (int turn = 0; turn < 4; ++turn) {
        const auto state = survival.Snapshot();
        survival.EndTurn(state.Revision,state.ActiveId,Facing::South);
        check(survival.Snapshot().Outcome == (turn == 3 ? BattleOutcome::Won : BattleOutcome::Ongoing), "規定ラウンドの生存で勝利");
    }
    const auto invalid = [&](int width, int height, std::vector<Tile> cells, std::vector<BattleUnit> units) {
        try { Battle battle(width,height,std::move(cells),std::move(units),0); check(false,"無効な戦場を拒否"); }
        catch (const std::invalid_argument&) { check(true,"無効な戦場を拒否"); }
    };
    invalid(0,8,std::vector<Tile>(64),{hero,enemy});
    invalid(8,13,std::vector<Tile>(104),{hero,enemy});
    invalid(8,8,std::vector<Tile>(63),{hero,enemy});
    invalid(8,8,std::vector<Tile>(64),{});
    invalid(8,8,std::vector<Tile>(64),{hero,hero,enemy});
    auto bad = hero; bad.Hp = -1; invalid(8,8,std::vector<Tile>(64),{bad,enemy});
    bad = hero; bad.At = enemy.At; invalid(8,8,std::vector<Tile>(64),{bad,enemy});
    bad = hero; bad.Side=static_cast<Faction>(5); invalid(8,8,std::vector<Tile>(64),{bad,enemy});
    tiles.assign(64,{}); tiles[0].Cover=-1; invalid(8,8,tiles,{hero,enemy});
    invalid(13,8,std::vector<Tile>(104),{hero,enemy});
    invalid(8,7,std::vector<Tile>(56),{hero,enemy});
    invalid(8,8,std::vector<Tile>(64),{hero});
    invalid(8,8,std::vector<Tile>(64),{enemy});
    tiles.assign(64,{}); tiles[1 * 8 + 1].Wall=true; invalid(8,8,tiles,{hero,enemy});
    tiles.assign(64,{}); tiles[0].Elevation=4; invalid(8,8,tiles,{hero,enemy});
    tiles.assign(64,{}); tiles[0].Cover=51; invalid(8,8,tiles,{hero,enemy});
    for (int field = 0; field < 8; ++field) {
        bad = hero;
        switch (field) {
        case 0: bad.Id=-1; break;
        case 1: bad.Role=static_cast<BattleRole>(5); break;
        case 2: bad.Direction=static_cast<Facing>(-1); break;
        case 3: bad.At={0,-1}; break;
        case 4: bad.Hp=bad.MaxHp+1; break;
        case 5: bad.MaxHp=10001; break;
        case 6: bad.Range=0; break;
        case 7: bad.Movement=13; break;
        }
        invalid(8,8,std::vector<Tile>(64),{bad,enemy});
    }
    for (int variant = 0; variant < 3; ++variant) {
        try {
            Battle badGoal(8,8,std::vector<Tile>(64),{hero,enemy},0,
                variant == 0 ? static_cast<BattleGoal>(9) : BattleGoal::Survive, variant == 1 ? 0 : 101);
            check(false,"目的・生存ラウンド数を検証");
        } catch (const std::invalid_argument&) { check(true,"目的・生存ラウンド数を検証"); }
    }
    try { Battle noTarget(8,8,std::vector<Tile>(64),{hero,enemy},42,BattleGoal::Rescue); check(false,"救出対象は必須"); }
    catch (const std::invalid_argument&) { check(true,"救出対象は必須"); }
    auto neutral = Unit(9,Faction::Neutral,6,6); neutral.Hp=0;
    auto weakEnemy = enemy; weakEnemy.At={1,2}; weakEnemy.Hp=1;
    Battle rescue(8,8,std::vector<Tile>(64),{hero,weakEnemy,neutral},42,BattleGoal::Rescue);
    check(rescue.Attack(0,1,{1,2}) && rescue.Snapshot().Outcome == BattleOutcome::Won, "倒れた救出対象も敵全滅で救出");
    weakEnemy.Hp=0; hero.Hp=0;
    Battle bothDown(8,8,std::vector<Tile>(64),{hero,weakEnemy},42);
    check(bothDown.Snapshot().Outcome == BattleOutcome::Lost && bothDown.Snapshot().Order.empty(), "両陣営全滅は敗北優先");
    hero.Hp=hero.MaxHp;
    Battle alreadyWon(8,8,std::vector<Tile>(64),{hero,weakEnemy},42);
    check(alreadyWon.Snapshot().Outcome == BattleOutcome::Won, "敵戦闘不能のみなら勝利");
    weakEnemy.Hp=1; weakEnemy.Speed=19;
    auto remaining = enemy; remaining.Id=5; remaining.Speed=1;
    Battle skip(8,8,std::vector<Tile>(64),{hero,weakEnemy,remaining},42);
    skip.Attack(0,1,{1,2});
    skip.EndTurn(skip.Snapshot().Revision,1,Facing::South);
    check(skip.Snapshot().ActiveId == 5, "倒れた敵の手番を飛ばす");
    auto sameSpeed = ally; sameSpeed.Speed=20;
    Battle stable(8,8,std::vector<Tile>(64),{sameSpeed,hero,enemy},42);
    check(stable.Snapshot().Order == std::vector<int>({1,3,2}), "同陣営同速はID昇順");
    neutral.Hp=30; neutral.At={2,1};
    Battle guarded(8,8,std::vector<Tile>(64),{hero,enemy,neutral},42);
    check(!has(guarded.Reachable(1),{2,1}) && !guarded.PreviewAttack(1,{2,1}).Legal, "中立者の位置と安全を守る");
    auto rangedEnemy=enemy; rangedEnemy.Speed=30; rangedEnemy.Range=6;
    Battle threat(8,8,std::vector<Tile>(64),{hero,rangedEnemy,neutral},42);
    check(threat.PreviewAttack(2,{2,1}).Legal, "敵は救出対象を狙える");
    hero.Accuracy=0; hero.Attack=0; hero.Range=6;
    auto armored=enemy; armored.Defense=1000;
    Battle minimum(8,8,std::vector<Tile>(64),{hero,armored},42);
    const auto minimumHit=minimum.PreviewAttack(1,enemy.At);
    check(minimumHit.Legal && minimumHit.Hits[0].Chance == 5 && minimumHit.Hits[0].Damage == 1, "命中とダメージの下限");
    check(!minimum.PreviewAttack(1,{-1,0}).Legal, "攻撃予告の盤外入力");
    check(!minimum.EndTurn(999,1,Facing::South) && !minimum.EndTurn(0,2,Facing::South), "手番終了の世代・本人チェック");
    for (int seed = 0; seed < 12; ++seed) {
        Random terrainRandom(seed);
        tiles.assign(64,{});
        for (auto& tile : tiles) { tile.Wall=terrainRandom.Uniform(7)==0; tile.Elevation=static_cast<int>(terrainRandom.Uniform(3)); }
        tiles[9]={}; tiles[12]={};
        Battle symmetric(8,8,tiles,{hero,enemy},42);
        for (int y=0;y<8;++y) for (int x=0;x<8;++x) {
            check(symmetric.HasLineOfSight({1,1},{x,y}) == symmetric.HasLineOfSight({x,y},{1,1}), "混合地形の射線も対称");
        }
    }
    std::cout << "Battle: " << checks << " checks, " << failed << " failed\n";
    return failed;
}
