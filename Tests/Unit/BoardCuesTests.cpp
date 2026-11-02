#include "Application/BoardCues.h"
#include <algorithm>
#include <iostream>
namespace {
GirlsSpire::Battle Arena(bool wall=false,int hp=80){
    using namespace GirlsSpire;BattleUnit hero;hero.At={1,1};hero.Range=3;hero.Hp=hero.MaxHp=hp;hero.Speed=30;
    auto ally=hero;ally.Id=1;ally.At={2,2};ally.Speed=20;
    auto enemy=hero;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={3,1};enemy.Range=1;enemy.Attack=20;enemy.Speed=10;enemy.Movement=0;
    auto neutral=hero;neutral.Id=2;neutral.Side=Faction::Neutral;neutral.At={1,2};
    std::vector<Tile> tiles(64);tiles[2+8].Wall=wall;tiles[1].Trap=true;tiles[3+8*3].ObstacleHp=20;
    return Battle(8,8,std::move(tiles),{hero,ally,enemy,neutral},42);
}
}
int TestBoardCues()
{
    using namespace GirlsSpire;int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL BoardCues: "<<why<<'\n';}};
    check(BoardCue{Faction::Player}.Symbols()=="○" && BoardCue{Faction::Enemy}.Symbols()=="△" && BoardCue{Faction::Neutral}.Symbols()=="◇","陣営を異なる形で表示");
    check(BoardCue{{},true,true,true,10,20}.Symbols()=="＋×！","移動/射程/危険を独立表示");
    check(BoardCue{}.Symbols().empty(),"無関係なセルへ記号を付けない");
    check(BoardCue{{},false,false,false,1,0}.Symbols()=="！" && BoardCue{{},false,false,false,0,1}.Symbols()=="！","経路被害と敵被害を各々危険表示");
    for(bool wall:{false,true})for(int hp:{5,80}){
        const auto battle=Arena(wall,hp);const auto before=battle.Checkpoint();
        for(bool threats:{false,true}){
            const auto cues=ReadBoardCues(battle,threats);check(cues.size()==64,"盤面全セルを返す");if(cues.size()!=64)continue;
            check(cues[9].Side==Faction::Player && cues[11].Side==Faction::Enemy && cues[17].Side==Faction::Neutral,"生存者の陣営を対応");
            check(cues[11].Attack==!wall && !cues[17].Attack && !cues[18].Move,"射線・陣営・停止禁止の正規判定を共用");
            check(cues[1].Trap && cues[1].Move && cues[1].RouteTrapDamage==std::min(hp,10) && cues[1].Symbols().find("！")!=std::string::npos,"罠と経路被害を表示");
            for(int index=0;index<64;++index){const Position at{index%8,index/8};const auto& cue=cues[index];
                const auto moves=battle.Reachable(0);check(cue.Move==(std::find(moves.begin(),moves.end(),at)!=moves.end()),"移動先は合法到達先だけ");
                check(cue.Attack==battle.PreviewAttack(0,at).Legal,"通常攻撃対象を正規予告から取得");
                int expected=0;if(threats){auto projection=battle;if(at==Position{1,1} || (cue.Move && projection.Move(before.State.Revision,0,at)))expected=projection.PotentialEnemyDamage(0);}
                check(cue.EnemyDamage==expected,"危険は正規の移動後/現地点の上限予測");
            }
            check(battle.Checkpoint()==before,"表示計算で全状態/乱数を変更しない");
        }
    }
    auto enemyTurn=Arena();
    check(enemyTurn.EndTurn(enemyTurn.Snapshot().Revision,0,Facing::South),"主人公の手番を終了");
    check(enemyTurn.EndTurn(enemyTurn.Snapshot().Revision,1,Facing::South),"仲間の手番を終了");
    const auto enemyBefore=enemyTurn.Checkpoint();
    for(const auto& cue:ReadBoardCues(enemyTurn,true))check(cue.EnemyDamage==0,"敵手番に味方用の被害予測を混同しない");
    check(enemyTurn.Checkpoint()==enemyBefore,"敵手番の表示も状態不変");
    BattleUnit winner;winner.At={1,1};winner.Attack=1000;winner.Accuracy=200;winner.Speed=30;
    auto loser=winner;loser.Id=100;loser.At={2,1};loser.Side=Faction::Enemy;loser.Speed=10;
    Battle ended(8,8,std::vector<Tile>(64),{winner,loser},42);
    check(ended.Attack(ended.Snapshot().Revision,0,{2,1}) && ended.Snapshot().Outcome==BattleOutcome::Won,"勝利済み盤面を準備");
    const auto endedBefore=ended.Checkpoint();
    const auto endedCues=ReadBoardCues(ended,true);
    check(!endedCues[10].Side,"戦闘不能者に生存陣営記号を付けない");
    for(const auto& cue:endedCues)check(!cue.Move && !cue.Attack && cue.EnemyDamage==0,"終了後に合法行動や被害を捏造しない");
    check(ended.Checkpoint()==endedBefore,"終了盤面の表示も不変");
    std::cout<<"BoardCues: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
