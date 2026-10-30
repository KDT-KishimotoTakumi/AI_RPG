#include "Domain/Battle.h"
#include "AI/Intent.h"
#include "Generation/Battlefield.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <functional>
#include <tuple>
int TestTraps()
{
    using namespace GirlsSpire;
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* label) { ++checks; if(!ok){++failed; std::cerr<<"FAIL Traps: "<<label<<'\n';} };
    BattleUnit hero; hero.At={1,1}; hero.Speed=30; hero.Movement=4; hero.Defense=1000; hero.Evasion=200;
    BattleUnit enemy; enemy.Id=100; enemy.Side=Faction::Enemy; enemy.At={7,7}; enemy.Movement=0;
    std::vector<Tile> tiles(64); tiles[10].Trap=true;
    const auto make=[&] {return Battle(8,8,tiles,{hero,enemy},42);};
    auto battle=make(); const auto snapshot=battle.Snapshot(); const auto rng=battle.RandomState();
    const auto detour=battle.PreviewMove(0,{3,1});
    check(detour.Legal && detour.Path.size()==4 && detour.Damage==0,"歩数より罠回避を優先");
    check(detour==battle.PreviewMove(0,{3,1}),"同率経路は決定論的");
    check(battle.Snapshot()==snapshot && battle.RandomState()==rng,"予告で状態不変");
    hero.Movement=2; battle=make(); const auto direct=battle.PreviewMove(0,{3,1});
    check(direct.Legal && direct.Path==std::vector<Position>{{2,1},{3,1}} && direct.Damage==10 && !direct.KnockedOut,"移動力内の罠経路を予告");
    check(!battle.Move(99,0,{3,1}) && battle.Snapshot().Tiles[10].Trap,"古い入力は罠不変");
    check(battle.Move(0,0,{3,1}) && battle.Snapshot().Units[0].Hp==20,"通過時10被害・防御回避無視");
    check(!battle.Snapshot().Tiles[10].Trap && battle.RandomState()==rng,"一回消滅・乱数不変");
    check(!battle.PreviewMove(0,{2,1}).Legal && !battle.Move(1,0,{2,1}),"二度目の移動不可");
    battle.EndTurn(1,0,Facing::South); battle.EndTurn(2,100,Facing::South);
    check(battle.PreviewMove(0,{1,1}).Damage==0 && battle.Move(3,0,{1,1}) && battle.Snapshot().Units[0].Hp==20,"消費した罠は再発動なし");
    check(!make().PreviewMove(-1,{2,1}).Legal && !make().PreviewMove(100,{2,1}).Legal,"不正人物・手番外拒否");
    check(!make().PreviewMove(0,{-1,1}).Legal && !make().PreviewMove(0,{1,1}).Legal && !make().PreviewMove(0,{7,7}).Legal,"盤外・現在地・占有拒否");
    hero.Hp=15; tiles[11].Trap=true; tiles[12].Trap=true; hero.Movement=3;
    auto lethal=make(); const auto death=lethal.PreviewMove(0,{4,1});
    check(death.Legal && death.Damage==15 && death.KnockedOut && death.Path==std::vector<Position>{{2,1},{3,1}},"致死地点までの実被害と経路");
    check(lethal.Move(0,0,{4,1}),"警告了承後の致死移動をルールが実行");
    check(lethal.Snapshot().Units[0].Hp==0 && lethal.Snapshot().Units[0].At==Position{3,1} && lethal.Snapshot().Outcome==BattleOutcome::Lost,"致死マスで停止して敗北");
    check(!lethal.Snapshot().Tiles[10].Trap && !lethal.Snapshot().Tiles[11].Trap && lethal.Snapshot().Tiles[12].Trap,"未通過の罠は残す");
    check(!lethal.PreviewMove(0,{2,2}).Legal,"終了戦闘は移動拒否");
    BattleUnit ally=hero; ally.Id=1; ally.At={0,0}; ally.Speed=20; ally.Hp=30;
    auto survivors=Battle(8,8,tiles,{hero,ally,enemy},42);
    check(survivors.Move(0,0,{4,1}) && survivors.Snapshot().ActiveId==1 && survivors.Snapshot().Outcome==BattleOutcome::Ongoing,"味方生存時は次手番");
    check(!survivors.Snapshot().Moved && !survivors.Snapshot().Acted && survivors.Snapshot().Revision==1,"次手番の行動枠と世代");
    auto forecast=IntentForecast::Build(make(),{{BasicActionKind::Move,{4,1}}},75);
    check(forecast.Fallback()==IntentFallback::NoSafeCandidate,"仲間AIは罠で即死する経路を除外");
    hero.Hp=20; hero.Defense=2; hero.Movement=1; tiles=std::vector<Tile>(64); tiles[10].Trap=true;
    enemy.At={3,1}; enemy.Attack=12;
    auto danger=make();
    forecast=IntentForecast::Build(danger,{{BasicActionKind::Move,{2,1}}},75);
    check(forecast.Fallback()==IntentFallback::NoSafeCandidate,"罠10と次の敵攻撃10の合計致死を除外");
    hero.Hp=21; auto safe=make(); forecast=IntentForecast::Build(safe,{{BasicActionKind::Move,{2,1}}},75);
    check(forecast.Candidates().size()==1 && forecast.Candidates()[0].PotentialDamage==20 && forecast.Fallback()==IntentFallback::None,"罠と敵被害の合計上限を予告");
    Random choice(9); check(forecast.Execute(safe,choice) && safe.Snapshot().Units[0].Hp==11,"仲間の実行にも罠を適用");
    enemy.Speed=40; enemy.Hp=30;
    auto hostile=make(); check(hostile.Move(0,100,{2,1})==false,"移動力0の敵は罠へ移動不可");
    enemy.Movement=1; hostile=make();
    check(hostile.Move(0,100,{2,1}) && hostile.Snapshot().Units[1].Hp==20 && !hostile.Snapshot().Tiles[10].Trap,"敵にも同じ罠効果");
    enemy.Hp=5; hostile=make(); check(hostile.Move(0,100,{2,1}) && hostile.Snapshot().Outcome==BattleOutcome::Won,"最後の敵が罠死で勝利");
    auto survival=Battle(8,8,tiles,{hero,enemy},42,BattleGoal::Survive);
    check(survival.Move(0,100,{2,1}) && survival.Snapshot().ActiveId==0 && survival.Snapshot().Outcome==BattleOutcome::Ongoing,"生存戦は敵死亡後も次手番へ");
    enemy.Speed=10; enemy.Hp=30; enemy.At={7,7}; hero.Hp=30; hero.Movement=2;
    for(int invalid=0;invalid<3;++invalid) {
        auto bad=tiles; if(invalid==0) bad[10].Wall=true; else if(invalid==1) bad[10].ObstacleHp=20; else bad[9].Trap=true;
        bool threw=false; try{Battle b(8,8,bad,{hero,enemy},42);}catch(const std::invalid_argument&){threw=true;}
        check(threw,"罠と壁・障害物・初期人物の重複拒否");
    }
    auto layout=TerrainLayout{8,8,tiles};
    check(Battlefield::Validate(layout,{hero,enemy},BattleGoal::Eliminate)==FieldFailure::None,"任意の罠に安全な迂回路がある配置");
    for(int y=0;y<8;++y) layout.Tiles[y*8+2].Trap=true;
    check(Battlefield::Validate(layout,{hero,enemy},BattleGoal::Eliminate)==FieldFailure::Disconnected,"罠通過必須の生成を拒否");
    bool sawTrap=false;
    for(int seed=0;seed<100;++seed) {
        const auto field=Battlefield::Generate(seed,{hero,enemy});
        check(Battlefield::Validate(field.Layout,{hero,enemy},BattleGoal::Eliminate)==FieldFailure::None,"100シード罠付き安全検証");
        for(const auto& tile:field.Layout.Tiles) sawTrap=sawTrap||tile.Trap;
    }
    check(sawTrap,"罠を自動生成");
    check(make().ConnectedTerrain({2,1},true).empty() && !make().ConnectedTerrain({2,1}).empty(),"罠を避ける接続検査と通常接続を区別");
    // 実装の動的計画法とは別に、全単純経路を列挙して最適性を照合する。
    hero.Movement=4; hero.Hp=hero.MaxHp=80;
    for(int seed=0;seed<100;++seed) {
        Random random(seed); std::vector<Tile> field(64);
        for(auto& tile:field) {
            const auto roll=random.Uniform(100);
            tile.Wall=roll<10; tile.ObstacleHp=roll>=10 && roll<20?20:0;
            tile.Trap=roll>=20 && roll<50; tile.Elevation=roll>=85?2:0;
        }
        field[9]={}; field[63]={};
        auto actual=Battle(8,8,field,{hero,enemy},seed);
        for(const Position target:std::vector<Position>{{3,1},{1,3},{3,3},{0,0}}) {
            std::pair<int,int> best{10000,10000}; std::vector<bool> visited(64,false); visited[9]=true;
            std::function<void(Position,int,int)> search=[&](Position from,int steps,int cost) {
                if(from==target) {best=std::min(best,std::pair{cost,steps});return;}
                if(steps==4) return;
                for(const Position direction:std::vector<Position>{{0,-1},{1,0},{0,1},{-1,0}}) {
                    const Position next{from.X+direction.X,from.Y+direction.Y};
                    if(next.X<0 || next.Y<0 || next.X>=8 || next.Y>=8) continue;
                    const int i=next.Y*8+next.X;
                    if(visited[i] || field[i].Wall || field[i].ObstacleHp>0 || next==enemy.At ||
                        std::abs(field[i].Elevation-field[from.Y*8+from.X].Elevation)>1) continue;
                    visited[i]=true; search(next,steps+1,cost+(field[i].Trap?10:0)); visited[i]=false;
                }
            };
            search(hero.At,0,0);
            const auto preview=actual.PreviewMove(0,target);
            check(preview.Legal==(best.first!=10000),"独立探索と到達判定が一致");
            if(!preview.Legal) continue;
            check(preview.Damage==best.first && static_cast<int>(preview.Path.size())==best.second,"100シードで被害最小・同被害なら最短");
            auto executed=actual;
            check(executed.Move(0,0,target) && executed.Snapshot().Units[0].At==target && executed.Snapshot().Units[0].Hp==80-preview.Damage,"予告と実移動の被害・到達位置が一致");
            check(executed.RandomState()==actual.RandomState(),"どの経路も乱数不消費");
        }
    }
    std::cout<<"Traps: "<<checks<<" checks, "<<failed<<" failed\n"; return failed;
}
