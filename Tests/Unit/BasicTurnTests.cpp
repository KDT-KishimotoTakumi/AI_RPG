#include "AI/BasicTurn.h"
#include <iostream>

int TestBasicTurn()
{
    using namespace GirlsSpire;
    int checks=0, failed=0;
    const auto check = [&](bool pass,const char* label) { ++checks; if(!pass){++failed; std::cerr<<"FAIL BasicTurn: "<<label<<'\n';} };
    BattleUnit hero; hero.Id=0; hero.At={1,1}; hero.Speed=1;
    BattleUnit enemy; enemy.Id=10; enemy.At={1,2}; enemy.Side=Faction::Enemy;
    Battle attack(8,8,std::vector<Tile>(64),{hero,enemy},42);
    const auto original=attack.Snapshot(); const auto random=attack.RandomState();
    const auto action=ChooseBasicAction(attack);
    check(action == BasicAction{BasicActionKind::Attack,{1,1}},"射程内の敵を攻撃");
    check(attack.Snapshot()==original && attack.RandomState()==random,"思考で状態・乱数を変更しない");
    check(ApplyBasicAction(attack,action),"決定した合法手を実行");
    check(ChooseBasicAction(attack).Kind == BasicActionKind::Wait,"行動済みなら待機");
    check(ApplyBasicAction(attack,ChooseBasicAction(attack)) && attack.Snapshot().ActiveId==0,"手番を進める");
    enemy.At={6,6};
    Battle approach(8,8,std::vector<Tile>(64),{hero,enemy},42);
    const auto move=ChooseBasicAction(approach);
    check(move.Kind==BasicActionKind::Move,"射程外なら接近");
    check(ApplyBasicAction(approach,move),"合法な移動を実行");
    check(ChooseBasicAction(approach).Kind==BasicActionKind::Wait,"接近後も射程外なら待機");
    auto tiles=std::vector<Tile>(64);
    tiles[5*8+6].Wall=true; tiles[6*8+5].Wall=true; tiles[6*8+7].Wall=true; tiles[7*8+6].Wall=true;
    Battle trapped(8,8,tiles,{hero,enemy},42);
    check(ChooseBasicAction(trapped).Kind==BasicActionKind::Wait,"合法候補なしは待機");
    check(!ApplyBasicAction(trapped,{static_cast<BasicActionKind>(9),{0,0}}),"不正な行動種別拒否");
    hero.Hp=0;
    Battle ended(8,8,std::vector<Tile>(64),{hero,enemy},42);
    check(ChooseBasicAction(ended).Kind==BasicActionKind::Wait && !ApplyBasicAction(ended,{}),"終了した戦闘で行動しない");
    std::cout<<"BasicTurn: "<<checks<<" checks, "<<failed<<" failed\n";
    return failed;
}
