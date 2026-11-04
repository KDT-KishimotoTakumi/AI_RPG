#pragma once
#include "Battle.h"
#include <optional>
#include <string_view>
namespace GirlsSpire {
struct Profile;
// 恒久保存の敵種ID。既存値を並べ替えない。戦闘中の人物IDとは別。
enum class EnemyKind { Footman=1,Archer=2,Gunner=3,Cavalry=4,Ninja=5,Oni=6,CurseMonk=7,Fox=8,Spider=9,Armour=10,Princess=11,Lord=12 };
inline constexpr int EnemyKindCount=12;
struct EnemyDefinition {EnemyKind Kind;std::string_view Name,Description;BattleUnit Base;};
const std::vector<EnemyDefinition>& EnemyCatalog();
const EnemyDefinition* FindEnemyDefinition(int id);
BattleUnit CreateEnemy(EnemyKind kind,int id,Position at);
std::optional<EnemyKind> EnemyKindOf(const BattleUnit& unit);
bool DiscoverEnemies(Profile& profile,const std::vector<BattleUnit>& units);
}
