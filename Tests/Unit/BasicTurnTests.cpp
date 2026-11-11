#include "AI/BasicTurn.h"
#include "AI/Intent.h"
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
    // 壁の裏で直線距離が縮まない局所最小でも、経路距離で迂回して最終的に接触する（全思考）。
    {
        BattleUnit target;target.Id=0;target.At={3,0};target.Speed=1;target.Hp=target.MaxHp=500;
        BattleUnit chaser;chaser.Id=10;chaser.Side=Faction::Enemy;chaser.At={3,3};chaser.Movement=2;
        auto wallTiles=std::vector<Tile>(64);for(int x=2;x<=4;++x)wallTiles[2*8+x].Wall=true;
        for(auto mode:{EnemyThinking::Legacy,EnemyThinking::Basic,EnemyThinking::Disrupt,EnemyThinking::Tactical}) {
            auto checkpoint=Battle(8,8,wallTiles,{target,chaser},42).Checkpoint();checkpoint.State.EnemyAI=mode;
            checkpoint.State.AbilityRules=AbilityRuleSet::CompleteCharacters;
            const auto restored=Battle::Restore(checkpoint);check(restored.has_value(),"試験局面を復元できる");if(!restored)continue;
            auto chase=*restored;
            check(ChooseBasicAction(chase).Kind==BasicActionKind::Move,"壁の裏の局所最小でも迂回して接近する");
            bool reached=false;
            for(int turn=0;turn<20 && !reached;++turn) {
                const auto& snapshot=chase.Snapshot();
                if(snapshot.ActiveId==chaser.Id) ApplyBasicAction(chase,ChooseBasicAction(chase));
                else chase.EndTurn(snapshot.Revision,snapshot.ActiveId,Facing::South);
                reached=chase.Snapshot().Units[0].Hp<target.MaxHp;
            }
            check(reached,"迂回の末に壁の向こうの相手へ攻撃が届く");
        }
        auto closed=wallTiles;for(int x=0;x<8;++x)closed[2*8+x].Wall=true;
        check(ChooseBasicAction(Battle(8,8,closed,{target,chaser},42)).Kind==BasicActionKind::Wait,"経路が存在しなければ待機のまま");
        const auto before=Battle(8,8,wallTiles,{target,chaser},42);const auto snapshot=before.Snapshot();
        ChooseBasicAction(before);check(before.Snapshot()==snapshot,"経路探索で状態を変更しない");
    }
    // 味方の自動接近も、壁の裏の局所最小で待機せず経路距離で迂回する。
    {
        BattleUnit ally;ally.Id=0;ally.At={3,3};ally.Movement=2;ally.Hp=ally.MaxHp=500;
        BattleUnit foe;foe.Id=10;foe.Side=Faction::Enemy;foe.At={3,0};foe.Hp=foe.MaxHp=500;
        auto wallTiles=std::vector<Tile>(64);for(int x=2;x<=4;++x)wallTiles[2*8+x].Wall=true;
        const Battle blocked(8,8,wallTiles,{ally,foe},42);const auto snapshot=blocked.Snapshot();
        const auto ranked=RankIntentActions(blocked,{});
        check(!ranked.empty() && ranked.front().Kind==BasicActionKind::Move,"味方も壁の裏の局所最小で迂回して接近する");
        check(blocked.Snapshot()==snapshot,"味方の迂回探索で状態を変更しない");
        auto closed=wallTiles;for(int x=0;x<8;++x)closed[2*8+x].Wall=true;
        const auto sealed=RankIntentActions(Battle(8,8,closed,{ally,foe},42),{});
        check(sealed.size()==1 && sealed.front().Kind==BasicActionKind::Wait,"経路が存在しなければ待機のまま");
    }
    hero.Hp=0;
    Battle ended(8,8,std::vector<Tile>(64),{hero,enemy},42);
    check(ChooseBasicAction(ended).Kind==BasicActionKind::Wait && !ApplyBasicAction(ended,{}),"終了した戦闘で行動しない");
    std::cout<<"BasicTurn: "<<checks<<" checks, "<<failed<<" failed\n";
    return failed;
}
