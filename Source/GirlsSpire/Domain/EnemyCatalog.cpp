#include "EnemyCatalog.h"
#include <algorithm>
#include <stdexcept>
namespace GirlsSpire {
const std::vector<EnemyDefinition>& EnemyCatalog()
{
    // 通常戦場・中級の基準値。精鋭戦のHPと難易度補正は生成側で従来どおり適用する。
    static const std::vector<EnemyDefinition> definitions={
        {EnemyKind::Footman,"足軽亡者","接近して隣接攻撃。",{.Id=100,.Side=Faction::Enemy,.Role=BattleRole::Standard,.At={},.Hp=22,.MaxHp=22,.Attack=10,.Movement=3,.Range=1}},
        {EnemyKind::Archer,"弓兵亡者","射線内から射撃。射程外では接近する。",{.Id=100,.Side=Faction::Enemy,.Role=BattleRole::Archer,.At={},.Hp=22,.MaxHp=22,.Attack=10,.Movement=3,.Range=4}},
        {EnemyKind::Gunner,"鉄砲亡者","射線内から射撃。射程外では接近する。",{.Id=100,.Side=Faction::Enemy,.Role=BattleRole::Gunner,.At={},.Hp=22,.MaxHp=22,.Attack=10,.Movement=2,.Range=5}},
        {EnemyKind::Cavalry,"騎馬亡者","直線3マス以上の移動で、その手番の通常攻撃だけ攻撃力＋6。",{.Id=100,.Side=Faction::Enemy,.Role=BattleRole::Cavalry,.At={},.Hp=22,.MaxHp=22,.Attack=10,.Movement=5,.Range=1}},
        {EnemyKind::Ninja,"骨忍","攻撃前に移動していなければ攻撃後に退避。被害を抑え、プレイヤー側から離れる。致死罠は避ける。",{.Id=100,.Side=Faction::Enemy,.Role=BattleRole::Ninja,.At={},.Hp=22,.MaxHp=22,.Attack=10,.Movement=4,.Range=1}},
        {EnemyKind::Oni,"鬼武者","予測被害が通常攻撃より高ければ強撃（攻撃力＋8・クールダウン2）を優先。",{.Id=100,.Side=Faction::Enemy,.Role=BattleRole::Oni,.At={},.Hp=22,.MaxHp=22,.Attack=16,.Movement=2,.Range=1,.Skills={true,false}}},
        {EnemyKind::CurseMonk,"呪詛僧","通常攻撃の命中で相手の次の手番終了まで攻撃力−4（下限0）。重複せず、再命中で期限更新。",{.Id=100,.Side=Faction::Enemy,.Role=BattleRole::CurseMonk,.At={},.Hp=22,.MaxHp=22,.Attack=10,.Movement=2,.Range=3}},
        {EnemyKind::Fox,"妖狐","射程3・射線内のHP50％以下の味方を優先回復（20・クールダウン2）。HP割合が低い者を優先（同率時の順は固定）。",{.Id=100,.Side=Faction::Enemy,.Role=BattleRole::Fox,.At={},.Hp=22,.MaxHp=22,.Attack=8,.Movement=3,.Range=3,.Skills={false,true}}},
        {EnemyKind::Spider,"絡新婦","通常攻撃の命中で相手の次の手番終了まで移動力−2（下限0）。重複せず、再命中で期限更新。",{.Id=100,.Side=Faction::Enemy,.Role=BattleRole::Spider,.At={},.Hp=22,.MaxHp=22,.Attack=10,.Movement=2,.Range=3}},
        {EnemyKind::Armour,"絡繰鎧","偶数ラウンドの手番開始に隣接空きマスへ耐久20の遮蔽を生成し、通常行動も行う。経路を塞ぐ配置では生成しない。",{.Id=100,.Side=Faction::Enemy,.Role=BattleRole::Standard,.At={},.Hp=35,.MaxHp=35,.Attack=10,.Defense=8,.Movement=3,.Range=1,.CreatesCover=true}},
        {EnemyKind::Princess,"落城姫","HP50％以下で通常攻撃を対象中心・上下左右1マスの範囲攻撃へ変更。味方を巻き込まず壁越しに攻撃しない。",{.Id=100,.Side=Faction::Enemy,.Role=BattleRole::FallenPrincess,.At={},.Hp=160,.MaxHp=160,.Attack=18,.Defense=4,.Speed=12,.Movement=2,.Range=2}},
        {EnemyKind::Lord,"歴史喰らいの城主","3の倍数ラウンドの手番開始に隣接空きマスへ足軽を召喚（存命最大3体）し、通常行動も行う。配下が残っていても城主撃破で勝利。",{.Id=100,.Side=Faction::Enemy,.Role=BattleRole::CastleLord,.At={},.Hp=240,.MaxHp=240,.Attack=22,.Defense=6,.Speed=15,.Movement=2,.Range=3}}
    };
    return definitions;
}
const EnemyDefinition* FindEnemyDefinition(int id)
{
    const auto& definitions=EnemyCatalog();const auto at=std::find_if(definitions.begin(),definitions.end(),[id](const auto& item){return static_cast<int>(item.Kind)==id;});
    return at==definitions.end()?nullptr:&*at;
}
BattleUnit CreateEnemy(EnemyKind kind,int id,Position at)
{
    const auto definition=FindEnemyDefinition(static_cast<int>(kind));
    if(!definition || id<100 || id>9999)throw std::invalid_argument("敵種または戦闘人物IDが無効です");
    auto unit=definition->Base;unit.Id=id;unit.At=at;return unit;
}
std::optional<EnemyKind> EnemyKindOf(const BattleUnit& unit)
{
    if(unit.Side!=Faction::Enemy || unit.Id<100 || unit.Id>9999)return std::nullopt;
    if(unit.CreatesCover)return EnemyKind::Armour;
    switch(unit.Role){
    case BattleRole::Standard:return EnemyKind::Footman;
    case BattleRole::Archer:return EnemyKind::Archer;case BattleRole::Gunner:return EnemyKind::Gunner;
    case BattleRole::Cavalry:return EnemyKind::Cavalry;case BattleRole::Ninja:return EnemyKind::Ninja;
    case BattleRole::Oni:return EnemyKind::Oni;case BattleRole::CurseMonk:return EnemyKind::CurseMonk;
    case BattleRole::Fox:return EnemyKind::Fox;case BattleRole::Spider:return EnemyKind::Spider;
    case BattleRole::FallenPrincess:return EnemyKind::Princess;case BattleRole::CastleLord:return EnemyKind::Lord;
    default:return std::nullopt;
    }
}
bool DiscoverEnemies(Profile& profile,const std::vector<BattleUnit>& units)
{
    if(!ValidProfile(profile))return false;
    for(const auto& unit:units)if(const auto kind=EnemyKindOf(unit)){
        const int id=static_cast<int>(*kind);const auto at=std::lower_bound(profile.DiscoveredEnemies.begin(),profile.DiscoveredEnemies.end(),id);
        if(at==profile.DiscoveredEnemies.end() || *at!=id)profile.DiscoveredEnemies.insert(at,id);
    }
    return true;
}
}
