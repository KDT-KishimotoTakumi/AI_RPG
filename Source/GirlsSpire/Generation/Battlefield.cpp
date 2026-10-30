#include "Battlefield.h"
#include <algorithm>
#include <cstdlib>
#include <stdexcept>
namespace GirlsSpire {
namespace {
TerrainLayout MakeDraft(Random& random,const std::vector<BattleUnit>& units)
{
    TerrainLayout layout;
    layout.Width=8+static_cast<int>(random.Uniform(5));
    layout.Height=8+static_cast<int>(random.Uniform(5));
    for (const auto& unit : units) {
        layout.Width=std::max(layout.Width,unit.At.X+1);
        layout.Height=std::max(layout.Height,unit.At.Y+1);
    }
    layout.Tiles.resize(layout.Width*layout.Height);
    for (int y=0;y<layout.Height;++y) for (int x=0;x<layout.Width;++x) {
        // 初期編成用の通路と各人物の隣接マスは平坦にしておく。
        if (x<=2 || y==1 || std::any_of(units.begin(),units.end(),[&](const BattleUnit& unit) {
            return std::abs(unit.At.X-x)+std::abs(unit.At.Y-y)<=1;
        })) continue;
        auto& tile=layout.Tiles[y*layout.Width+x];
        const auto roll=random.Uniform(100);
        tile.Wall=roll<15;
        if (!tile.Wall) {
            tile.Elevation=roll<35?1:0;
            tile.Cover=roll>=35 && roll<55?20:0;
            tile.ObstacleHp=roll>=55 && roll<65?20:0;
            tile.Trap=roll>=65 && roll<75;
        }
    }
    return layout;
}
}

FieldFailure Battlefield::Validate(const TerrainLayout& layout,const std::vector<BattleUnit>& units,BattleGoal goal)
{
    try {
        const Battle battle(layout.Width,layout.Height,layout.Tiles,units,0,goal);
        if (battle.Snapshot().Outcome!=BattleOutcome::Ongoing) return FieldFailure::Invalid;
        const auto connected=battle.ConnectedTerrain(units.front().At,true);
        const auto walkable=std::count_if(layout.Tiles.begin(),layout.Tiles.end(),[](const Tile& tile) { return !tile.Wall && tile.ObstacleHp==0 && !tile.Trap; });
        if (connected.size()!=static_cast<std::size_t>(walkable)) return FieldFailure::Disconnected;
        for (const auto& unit : units)
            if (unit.Side==Faction::Player && unit.Hp>0 && battle.PotentialEnemyDamage(unit.Id)>=unit.Hp)
                return FieldFailure::Unsafe;
        return FieldFailure::None;
    } catch (const std::invalid_argument&) { return FieldFailure::Invalid; }
}

GenerationResult Battlefield::Generate(std::uint64_t seed,const std::vector<BattleUnit>& units,
    BattleGoal goal,int maxAttempts,const DraftFactory& draft)
{
    if (maxAttempts<0 || maxAttempts>8) throw std::invalid_argument("生成試行数は0～8で指定してください");
    // 座標・人数・能力値を乱数処理より先に共有ルールで検査する。
    const Battle inputCheck(12,12,std::vector<Tile>(144),units,seed,goal);
    Random random(seed);
    GenerationResult result;
    for (int attempt=0;attempt<maxAttempts;++attempt) {
        Random child(random.Next());
        result.Layout=draft?draft(child):MakeDraft(child,units);
        result.Attempts=attempt+1; result.RandomState=random.State();
        if (Validate(result.Layout,units,goal)==FieldFailure::None) return result;
    }
    result.Layout={12,12,std::vector<Tile>(144)};
    result.UsedFallback=true; result.RandomState=random.State();
    if (Validate(result.Layout,units,goal)!=FieldFailure::None)
        throw std::invalid_argument("固定戦場でも安全な戦闘を構成できません");
    return result;
}
}
