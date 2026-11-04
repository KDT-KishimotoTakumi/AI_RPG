#include "Battle.h"
#include "Rules.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <queue>
#include <set>
#include <stdexcept>
#include <utility>

namespace GirlsSpire {
bool Battle::TerrainStep(Position from, Position to) const
{
    if (!Inside(to)) return false;
    const auto& destination=state_.Tiles[to.Y*state_.Width+to.X];
    const auto& source=state_.Tiles[from.Y*state_.Width+from.X];
    return !destination.Wall && destination.ObstacleHp==0 && std::abs(destination.Elevation-source.Elevation)<=1;
}

std::vector<Position> Battle::ConnectedTerrain(Position from,bool avoidTraps) const
{
    if (!Inside(from) || state_.Tiles[from.Y*state_.Width+from.X].Wall ||
        state_.Tiles[from.Y*state_.Width+from.X].ObstacleHp>0 ||
        (avoidTraps && state_.Tiles[from.Y*state_.Width+from.X].Trap)) return {};
    std::vector<Position> result{from};
    std::vector<bool> visited(state_.Tiles.size(),false);
    visited[from.Y*state_.Width+from.X]=true;
    const Position directions[]={{0,-1},{1,0},{0,1},{-1,0}};
    for (std::size_t index=0;index<result.size();++index) {
        const auto current=result[index];
        for (const auto step : directions) {
            const Position next{current.X+step.X,current.Y+step.Y};
            if (!TerrainStep(current,next)) continue;
            const auto tileIndex=next.Y*state_.Width+next.X;
            if(avoidTraps && state_.Tiles[tileIndex].Trap) continue;
            if (visited[tileIndex]) continue;
            visited[tileIndex]=true; result.push_back(next);
        }
    }
    return result;
}
int Battle::PotentialEnemyDamage(int targetId) const
{
    const auto target=Find(targetId);
    return target && target->Side==Faction::Player?PotentialHostileDamage(targetId):0;
}
int Battle::PotentialHostileDamage(int targetId) const
{
    const auto target = Find(targetId);
    if (!target || target->Hp <= 0 || target->Side == Faction::Neutral ||
        state_.Outcome != BattleOutcome::Ongoing) return 0;
    int total = 0;
    for (const auto& enemy : state_.Units) {
        if (enemy.Side == target->Side || enemy.Side==Faction::Neutral || enemy.Hp <= 0) continue;
        // コピー内で次の手番を仮定。移動・射線・攻撃を通常操作と同じ判定に通す。
        auto projection = *this;
        // 敵が動くまでに受け手の手番が終わるなら、防御強化の期限を先に進める。
        // 現在の敵から直ちに受ける攻撃や、自己使用手番を除外する強化は残す。
        for(std::size_t step=0;step<state_.Order.size();++step){
            const int id=state_.Order[(turnIndex_+step)%state_.Order.size()];
            if(id==enemy.Id)break;
            if(id==targetId)EndTimedBoostTurn(projection.Find(targetId)->DefenseBoost);
        }
        projection.state_.ActiveId = enemy.Id;
        projection.state_.Moved = false; projection.state_.Acted = false;
        projection.state_.ChargeReady=false;projection.state_.UsedSkill=-1;
        projection.turnIndex_=static_cast<std::size_t>(std::find(state_.Order.begin(),state_.Order.end(),enemy.Id)-state_.Order.begin());
        const auto damage=[&](const Battle& candidate) {
            const auto received=[&](const AttackPreview& preview) {
                for(const auto& hit:preview.Hits)if(hit.TargetId==targetId)return hit.Damage;
                return 0;
            };
            int maximum=received(candidate.PreviewAttack(enemy.Id,target->At));
            // 範囲中心が別の味方でも被害を受ける場合がある。実際の敵AIと同じ中心候補を照合する。
            if(candidate.NormalAttackRadius(enemy.Id)>0)for(const auto& center:candidate.Snapshot().Units) {
                if(center.Hp<=0 || center.Side==enemy.Side || (enemy.Side==Faction::Player && center.Side==Faction::Neutral))continue;
                maximum=std::max(maximum,received(candidate.PreviewAttack(enemy.Id,center.At)));
            }
            const int count=UsesCharacterSkills(state_.AbilityRules)?SkillCount:1;
            for(int index=0;index<count;++index){
                const auto skill=candidate.PreviewSkill(enemy.Id,static_cast<Skill>(index),target->At);
                if(skill.Legal)maximum=std::max(maximum,received(skill.Attack));
            }
            return maximum;
        };
        const auto afterMoves=[&](const Battle& initial){
            int maximum=damage(initial);
            // 現在地で弱い遠隔技が使えても、接近後の強い技を上限から除外しない。
            for(const auto at:initial.Reachable(enemy.Id)){
                auto candidate=initial;
                if(candidate.Move(candidate.Snapshot().Revision,enemy.Id,at))maximum=std::max(maximum,damage(candidate));
            }
            return maximum;
        };
        int maximum=afterMoves(projection);
        if(state_.EnemyCharacterSkills){
            auto supported=projection;
            // 回復でボスの範囲化が解除される場合にも、支援しない場合の上限を失わない。
            if(supported.ProjectEnemySupport(enemy.Id,*this))maximum=std::max(maximum,afterMoves(supported));
        }
        total+=maximum;
    }
    return total;
}
namespace {
bool Between(int value, int low, int high) { return value >= low && value <= high; }
int Distance(Position a, Position b) { return std::abs(a.X - b.X) + std::abs(a.Y - b.Y); }
}

Battle::Battle(int width, int height, std::vector<Tile> tiles, std::vector<BattleUnit> units,
    std::uint64_t seed, BattleGoal goal, int rounds, int medicines, int enemyHpPercent)
    : random_(seed), goal_(goal), survivalRounds_(rounds), enemyHpPercent_(enemyHpPercent)
{
    state_.Width = width; state_.Height = height;
    state_.Medicines=medicines;
    state_.Tiles = std::move(tiles); state_.Units = std::move(units);
    for(auto& unit:state_.Units){unit.Cooldowns={};unit.AttackBoost={};unit.DefenseBoost={};unit.AccuracyBoost={};}
    if(!ValidSetup(true)) throw std::invalid_argument("戦場・人物・目的の設定が無効です");
    RebuildOrder();
    Evaluate();
}

bool Battle::Inside(Position at) const
{
    return Between(at.X,0,state_.Width - 1) && Between(at.Y,0,state_.Height - 1);
}

const BattleUnit* Battle::Find(int id) const
{
    const auto found = std::find_if(state_.Units.begin(),state_.Units.end(),
        [id](const BattleUnit& unit) { return unit.Id == id; });
    return found == state_.Units.end() ? nullptr : &*found;
}

BattleUnit* Battle::Find(int id) { return const_cast<BattleUnit*>(std::as_const(*this).Find(id)); }

bool Battle::Active(int id) const
{
    return state_.Outcome == BattleOutcome::Ongoing && state_.ActiveId == id;
}

int Battle::Occupant(Position at) const
{
    for (const auto& unit : state_.Units) if (unit.Hp > 0 && unit.At == at) return unit.Id;
    return -1;
}

void Battle::RebuildOrder()
{
    state_.Order.clear();
    for (const auto& unit : state_.Units)
        if (unit.Hp > 0 && unit.Side != Faction::Neutral) state_.Order.push_back(unit.Id);
    std::sort(state_.Order.begin(),state_.Order.end(),[this](int left, int right) {
        const auto& a = *Find(left); const auto& b = *Find(right);
        if (a.Speed != b.Speed) return a.Speed > b.Speed;
        if (a.Side != b.Side) return a.Side == Faction::Player;
        return a.Id < b.Id;
    });
    turnIndex_ = 0;
    state_.ActiveId = state_.Order.empty() ? -1 : state_.Order.front();
}

void Battle::Evaluate()
{
    bool playerAlive = false, enemyAlive = false;
    for (const auto& unit : state_.Units) {
        if (unit.Hp <= 0) continue;
        if (unit.Side == Faction::Player) playerAlive = true;
        if (unit.Side == Faction::Enemy) enemyAlive = true;
    }
    if (!playerAlive) state_.Outcome = BattleOutcome::Lost;
    else if (goal_ == BattleGoal::Survive) {
        if (state_.Round > survivalRounds_) state_.Outcome = BattleOutcome::Won;
    } else if (goal_ == BattleGoal::DefeatLord) {
        const bool lordAlive=std::any_of(state_.Units.begin(),state_.Units.end(),[](const BattleUnit& unit) {
            return unit.Role==BattleRole::CastleLord && unit.Hp>0;
        });
        if(!lordAlive)state_.Outcome=BattleOutcome::Won;
    } else if (!enemyAlive) state_.Outcome = BattleOutcome::Won;
    if (state_.Outcome != BattleOutcome::Ongoing) state_.ActiveId = -1;
}

bool Battle::HasLineOfSight(Position from, Position to) const
{
    return TraceLine(from,to,false);
}

bool Battle::TraceLine(Position from, Position to, bool targetObstacle) const
{
    if (!Inside(from) || !Inside(to)) return false;
    const int maxHeight = std::max(state_.Tiles[from.Y * state_.Width + from.X].Elevation,
        state_.Tiles[to.Y * state_.Width + to.X].Elevation);
    const auto open = [&](Position at) {
        const auto& tile = state_.Tiles[at.Y * state_.Width + at.X];
        // 攻撃対象の障害物だけは終点として許可。手前の遮断は通常射線と同じ。
        return !tile.Wall && (tile.ObstacleHp==0 || (targetObstacle && at==to)) && tile.Elevation <= maxHeight;
    };
    if (!open(from) || !open(to)) return false;
    const int nx = std::abs(to.X - from.X), ny = std::abs(to.Y - from.Y);
    const int sx = to.X > from.X ? 1 : -1, sy = to.Y > from.Y ? 1 : -1;
    int ix = 0, iy = 0;
    auto cell = from;
    while (ix < nx || iy < ny) {
        const int decision = (1 + 2 * ix) * ny - (1 + 2 * iy) * nx;
        if (decision == 0) {
            // 線がマスの角を通る場合、両隣も検査して角越しの射撃を防ぐ。
            if (!open({cell.X + sx,cell.Y}) || !open({cell.X,cell.Y + sy})) return false;
            cell.X += sx; cell.Y += sy; ++ix; ++iy;
        } else if (decision < 0) { cell.X += sx; ++ix; }
        else { cell.Y += sy; ++iy; }
        if (!open(cell)) return false;
    }
    return true;
}

std::vector<Position> Battle::Reachable(int actorId) const
{
    std::vector<Position> reachable;
    if (!Active(actorId) || state_.Moved) return reachable;
    const auto& actor = *Find(actorId);
    if (state_.Acted && actor.Role != BattleRole::Dancer && actor.Role != BattleRole::Ninja) return reachable;
    const int allowance=MovementAllowance(actorId);
    std::vector<int> distance(state_.Tiles.size(),-1);
    std::queue<Position> frontier;
    frontier.push(actor.At);
    distance[actor.At.Y * state_.Width + actor.At.X] = 0;
    const Position directions[] = {{0,-1},{1,0},{0,1},{-1,0}};
    while (!frontier.empty()) {
        const auto from = frontier.front(); frontier.pop();
        const auto fromIndex = from.Y * state_.Width + from.X;
        if (distance[fromIndex] == allowance) continue;
        for (auto step : directions) {
            const Position to{from.X + step.X,from.Y + step.Y};
            if (!Inside(to)) continue;
            const auto index = to.Y * state_.Width + to.X;
            if (distance[index] >= 0 || !TerrainStep(from,to)) continue;
            const int occupant = Occupant(to);
            if (occupant >= 0 && Find(occupant)->Side != actor.Side) continue;
            distance[index] = distance[fromIndex] + 1;
            frontier.push(to);
            if (occupant < 0) reachable.push_back(to);
        }
    }
    return reachable;
}

int Battle::NormalAttackRadius(int actorId) const
{
    const auto actor=Find(actorId);
    return actor && actor->Hp>0 && actor->Role==BattleRole::FallenPrincess && actor->Hp*2<=actor->MaxHp?1:0;
}
AttackPreview Battle::PreviewAttack(int actorId, Position target, int radius, bool normal) const
{
    AttackPreview preview;
    if (!Active(actorId) || state_.Acted || !Between(radius,0,3) || !Inside(target)) return preview;
    if(normal)radius=std::max(radius,NormalAttackRadius(actorId));
    const auto& actor = *Find(actorId);
    const auto& targetTerrain=state_.Tiles[target.Y*state_.Width+target.X];
    const int power=std::max(0,actor.Attack+actor.AttackBoost.Amount-(actor.AttackCursed?4:0))+(state_.ChargeReady?6:0);
    if (Distance(actor.At,target) > actor.Range || !TraceLine(actor.At,target,targetTerrain.ObstacleHp>0)) return preview;
    if (targetTerrain.ObstacleHp>0) {
        if (radius!=0) return preview;
        preview.Legal=true;
        preview.TerrainDamage=std::min(power,targetTerrain.ObstacleHp);
        return preview;
    }
    for (const auto& victim : state_.Units) {
        if (victim.Hp <= 0 || victim.Side == actor.Side ||
            (actor.Side == Faction::Player && victim.Side == Faction::Neutral) ||
            Distance(victim.At,target) > radius || !HasLineOfSight(target,victim.At)) continue;
        const auto& sourceTile = state_.Tiles[actor.At.Y * state_.Width + actor.At.X];
        const auto& targetTile = state_.Tiles[victim.At.Y * state_.Width + victim.At.X];
        const int chance = ClampHitChance(actor.Accuracy + actor.AccuracyBoost.Amount - victim.Evasion - targetTile.Cover +
            10 * (sourceTile.Elevation - targetTile.Elevation));
        preview.Hits.push_back({victim.Id,chance,std::max(1,power - victim.Defense - victim.DefenseBoost.Amount)});
    }
    // 入力配列の並びが変わっても乱数の消費対象を同じにする。
    std::sort(preview.Hits.begin(),preview.Hits.end(),[](const PredictedHit& a, const PredictedHit& b) {
        return a.TargetId < b.TargetId;
    });
    preview.Legal = !preview.Hits.empty();
    return preview;
}

bool Battle::Attack(std::uint64_t revision, int actorId, Position target, int radius)
{
    const auto actor=Find(actorId);
    return AttackWithSyntax(revision,actorId,target,radius,Continuation|Ending,actor && actor->Role==BattleRole::CurseMonk,actor && actor->Role==BattleRole::Spider,true);
}

bool Battle::AttackWithSyntax(std::uint64_t revision,int actorId,Position target,int radius,int attributes,bool applyCurse,bool applyWeb,bool normal)
{
    if (revision != state_.Revision) return false;
    const auto preview = PreviewAttack(actorId,target,radius,normal);
    if (!preview.Legal) return false;
    lastHits_.clear();
    std::vector<int> enemies,knockedOut;
    bool succeeded=true;
    auto& terrain=state_.Tiles[target.Y*state_.Width+target.X];
    if (terrain.ObstacleHp>0) {
        terrain.ObstacleHp-=preview.TerrainDamage;
        if (terrain.ObstacleHp==0) terrain=Tile{};
    }
    for (const auto& hit : preview.Hits) {
        auto& victim = *Find(hit.TargetId);
        const bool success = random_.Uniform(100) < static_cast<std::uint32_t>(hit.Chance);
        const int damage = success ? std::min(victim.Hp,hit.Damage) : 0;
        victim.Hp -= damage;
        if(success && applyCurse)victim.AttackCursed=true;
        if(success && applyWeb)victim.MovementWebbed=true;
        succeeded=succeeded && success;
        if(success && victim.Side==Faction::Enemy) enemies.push_back(victim.Id);
        if(victim.Hp==0) knockedOut.push_back(victim.Id);
        lastHits_.push_back({hit.TargetId,success,damage});
    }
    state_.Acted = true;
    state_.ChargeReady=false;
    ++state_.Revision;
    RecordCombo(actorId,attributes,succeeded,enemies,knockedOut);
    Evaluate();
    return true;
}

bool Battle::EndTurn(std::uint64_t revision, int actorId, Facing facing)
{
    if (revision != state_.Revision || !Active(actorId) || !Between(static_cast<int>(facing),0,3)) return false;
    Find(actorId)->Direction = facing;
    ++state_.Revision;
    // 攻撃後の終了を追加の「起」にしない。回復/アイテム後の待機は従来どおり。
    if(!state_.Acted || lastHits_.empty())RecordCombo(actorId,Opening,true);
    AdvanceTurn();
    Evaluate();
    GeneratePeriodicCover();
    GeneratePeriodicSummons();
    return true;
}

void Battle::AdvanceTurn()
{
    auto& outgoing=*Find(state_.ActiveId);
    outgoing.AttackCursed=false;
    outgoing.MovementWebbed=false;
    EndTimedBoostTurn(outgoing.AttackBoost);
    EndTimedBoostTurn(outgoing.DefenseBoost);
    EndTimedBoostTurn(outgoing.AccuracyBoost);
    for(int index=0;index<SkillCount;++index) {
        if(index!=state_.UsedSkill && outgoing.Cooldowns[index]>0) --outgoing.Cooldowns[index];
    }
    state_.UsedSkill=-1;
    state_.Moved = false; state_.Acted = false;
    state_.ChargeReady=false;
    do {
        ++turnIndex_;
        if (turnIndex_ == state_.Order.size()) { ++state_.Round; RebuildOrder(); }
    } while (Find(state_.Order[turnIndex_])->Hp <= 0);
    state_.ActiveId = state_.Order[turnIndex_];
    combos_.AdvanceRound(state_.Round);
    state_.Combos=combos_.Snapshot();
}
}
