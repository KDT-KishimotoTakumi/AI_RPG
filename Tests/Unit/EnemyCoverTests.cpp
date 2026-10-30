#include "Domain/Battle.h"
#include "Infrastructure/SavePayload.h"
#include "Application/Encounter.h"
#include "../Fixtures/LegacyRun3Combat.h"
#include <algorithm>
#include <iostream>
using namespace GirlsSpire;
namespace {
Battle CoverBattle(std::uint64_t seed,bool ability=true)
{
    BattleUnit hero;hero.Id=0;hero.At={3,3};hero.Speed=20;hero.Hp=hero.MaxHp=200;hero.Attack=20;hero.Range=3;
    BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={4,3};enemy.Hp=enemy.MaxHp=200;enemy.Defense=8;enemy.CreatesCover=ability;
    return Battle(8,8,std::vector<Tile>(64),{hero,enemy},seed);
}
bool ToCoverTurn(Battle& battle,int round)
{
    for(int i=0;i<40;++i) {
        const auto& s=battle.Snapshot();if(s.Round==round && s.ActiveId==100)return true;
        if(!battle.EndTurn(s.Revision,s.ActiveId,Facing::South))return false;
    }
    return false;
}
int CoverCount(const Battle& battle)
{
    return static_cast<int>(std::count_if(battle.Snapshot().Tiles.begin(),battle.Snapshot().Tiles.end(),[](const Tile& t){return t.ObstacleHp>0;}));
}
}
int TestEnemyCover()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL EnemyCover: "<<why<<'\n';}};
    for(int seed=0;seed<100;++seed) {
        auto battle=CoverBattle(seed);const auto random=battle.RandomState();
        check(ToCoverTurn(battle,1) && CoverCount(battle)==0,"1ラウンド目には生成しない");
        check(ToCoverTurn(battle,2) && CoverCount(battle)==1,"偶数ラウンドの手番で1個生成");
        const auto created=battle.Checkpoint();
        check(created.State.Tiles[2*8+4].ObstacleHp==20 && !created.State.Tiles[2*8+4].Wall,"北からの固定順で耐久20を生成");
        check(!battle.HasLineOfSight({4,0},{4,3}),"生成した障害物が射線を遮る");
        check(!created.State.Acted && !created.State.Moved && battle.RandomState()==random,"生成で行動・移動・乱数を消費しない");
        auto restored=Battle::Restore(created);
        check(restored && restored->Checkpoint()==created && CoverCount(*restored)==1,"復元時には二重生成しない");
        const bool attack=battle.Attack(battle.Snapshot().Revision,100,{3,3});
        check(attack && battle.Snapshot().Acted,"生成後も通常攻撃可能");
        auto rounds=CoverBattle(seed);ToCoverTurn(rounds,3);
        check(CoverCount(rounds)==1,"奇数ラウンドでは増えない");
        ToCoverTurn(rounds,4);check(CoverCount(rounds)==2,"次の偶数ラウンドで再生成");
        auto generic=CoverBattle(seed,false);ToCoverTurn(generic,4);check(CoverCount(generic)==0,"特性なしの敵は生成しない");
        auto destroy=CoverBattle(seed);ToCoverTurn(destroy,2);destroy.EndTurn(destroy.Snapshot().Revision,100,Facing::South);
        check(destroy.Attack(destroy.Snapshot().Revision,0,{4,2}) && destroy.Snapshot().Tiles[2*8+4].ObstacleHp==0,"通常攻撃で破壊できる");
    }
    BattleUnit hero;hero.Id=0;hero.At={3,1};hero.Speed=20;
    BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={3,3};enemy.CreatesCover=true;
    std::vector<Tile> corridor(64);for(auto& tile:corridor)tile.Wall=true;
    for(int y=1;y<=3;++y)corridor[y*8+3].Wall=false;
    Battle sealed(8,8,corridor,{hero,enemy},1);ToCoverTurn(sealed,2);
    check(CoverCount(sealed)==0 && sealed.ConnectedTerrain(hero.At).size()==3,"唯一の到達経路を塞がず不発にする");
    auto busy=CoverBattle(1).Checkpoint();busy.State.Tiles[2*8+4].Trap=true;
    busy.State.Tiles[3*8+5].Wall=true;busy.State.Tiles[4*8+4].ObstacleHp=20;
    auto noRoom=Battle::Restore(busy);check(noRoom.has_value(),"隣接マスの占有状態を準備");
    if(noRoom){const auto before=noRoom->Snapshot().Tiles;ToCoverTurn(*noRoom,2);check(noRoom->Snapshot().Tiles==before,"罠・壁・既存障害物・人物には重ねない");}
    const auto legacy=DecodeSave(std::string_view(LegacyRun3Combat,sizeof(LegacyRun3Combat)-1),SaveKind::Run);
    const auto decoded=legacy.Value?DecodeRunPayload(legacy.Value->Payload):std::nullopt;
    check(decoded && decoded->Combat,"v3の実戦保存を復元");
    if(decoded && decoded->Combat)for(const auto& unit:decoded->Combat->Battle.State.Units)check(!unit.CreatesCover,"旧形式の敵に新しい特性を勝手に付与しない");
    auto invalid=CoverBattle(1).Checkpoint();invalid.State.Units[0].CreatesCover=true;
    check(!Battle::Restore(invalid),"味方への敵固有特性の混入を拒否");
    Run run(42,Difficulty::Beginner,HeroRole::Sword);auto entry=run.Checkpoint();
    entry.State.Phase=RunPhase::Event;entry.State.Active={NodeKind::Battle};entry.State.Choices.clear();
    auto current=CoverBattle(42);ToCoverTurn(current,2);
    const RunSaveState saved{entry,SavedCombat{current.Checkpoint(),CombatControl{}.Checkpoint()}};
    const auto bytes=EncodeRunPayload(saved);
    check(bytes && DecodeRunPayload(*bytes)==saved,"新しい特性と生成済み地形を保存往復");
    for(int seed=0;seed<100;++seed) {
        auto units=CoverBattle(seed).Snapshot().Units;
        const auto layout=Battlefield::Generate(seed,units).Layout;
        Battle varied(layout.Width,layout.Height,layout.Tiles,units,seed);const auto before=varied.Snapshot().Tiles;
        const auto rng=varied.RandomState();ToCoverTurn(varied,2);
        int changed=0;for(std::size_t i=0;i<before.size();++i)if(before[i]!=varied.Snapshot().Tiles[i])++changed;
        check(changed<=1 && varied.RandomState()==rng,"100シードで生成は1マス以内・乱数不変");
        const auto reachable=varied.ConnectedTerrain(units[0].At);
        check(std::find(reachable.begin(),reachable.end(),units[1].At)!=reachable.end(),"生成後も双方の到達性を維持");
        check(Battle::Restore(varied.Checkpoint()).has_value(),"生成後の全地形が復元検証を通る");
    }
    for(int level=0;level<3;++level) {
        RunSnapshot elite;elite.Phase=RunPhase::Event;elite.Active={NodeKind::Elite};elite.Level=static_cast<Difficulty>(level);
        const auto encounter=Encounter::Create(elite,42).Snapshot();
        const auto armor=std::find_if(encounter.Units.begin(),encounter.Units.end(),[](const BattleUnit& unit){return unit.CreatesCover;});
        check(armor!=encounter.Units.end() && armor->Side==Faction::Enemy && armor->Defense>2,"全難易度の強敵戦へ高防御の絡繰鎧を接続");
    }
    auto cornerState=CoverBattle(1).Checkpoint();cornerState.State.Units[0].At={0,1};cornerState.State.Units[1].At={0,0};
    auto corner=Battle::Restore(cornerState);if(corner)ToCoverTurn(*corner,2);
    check(corner && corner->Snapshot().Tiles[1].ObstacleHp==20,"盤外候補を除外して隣接マスへ生成");
    auto occupied=CoverBattle(1).Checkpoint();occupied.State.Tiles[2*8+4].Cover=20;
    BattleUnit corpse;corpse.Id=200;corpse.Side=Faction::Neutral;corpse.At={5,3};corpse.Hp=0;occupied.State.Units.push_back(corpse);
    auto blocked=Battle::Restore(occupied);if(blocked)ToCoverTurn(*blocked,2);
    check(blocked && blocked->Snapshot().Tiles[4*8+4].ObstacleHp==20 && blocked->Snapshot().Tiles[3*8+5].ObstacleHp==0,"既存遮蔽と戦闘不能者のマスを避ける");
    std::cout<<"EnemyCover: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
