#pragma once
#include "Random.h"
#include "Combo.h"
#include "BattleBond.h"
#include "TimedBoost.h"
#include <vector>
#include <array>

namespace GirlsSpire {
enum class BattleRole { Standard, Dancer, Ninja, Archer, Gunner, Cavalry, Oni, CurseMonk, Fox, Spider, FallenPrincess, CastleLord };
enum class Facing { North, East, South, West };
enum class BattleGoal { Eliminate, Survive, Rescue, DefeatLord };
enum class BattleOutcome { Ongoing, Won, Lost };
enum class Skill { PowerStrike, FirstAid, Sweep, FarStrike, Heal };
enum class AbilityRuleSet { Legacy, Character, CharacterSkills, CharacterAbilities, CompleteCharacters };
enum class EnemyThinking { Legacy, Basic, Disrupt, Tactical };
inline bool UsesCharacterSkills(AbilityRuleSet rules){return rules==AbilityRuleSet::CharacterSkills || rules==AbilityRuleSet::CharacterAbilities || rules==AbilityRuleSet::CompleteCharacters;}
inline bool UsesCharacterAbilities(AbilityRuleSet rules){return rules==AbilityRuleSet::CharacterAbilities || rules==AbilityRuleSet::CompleteCharacters;}
inline constexpr int SkillCount=5;
struct Tile {
    bool Wall = false;
    int Elevation = 0;
    int Cover = 0;
    int ObstacleHp = 0;
    bool Trap = false;
    bool operator==(const Tile&) const = default;
};
struct BattleUnit {
    int Id = 0;
    Faction Side = Faction::Player;
    BattleRole Role = BattleRole::Standard;
    Position At{};
    int Hp = 30;
    int MaxHp = 30;
    int Attack = 12;
    int Defense = 2;
    int Speed = 10;
    int Accuracy = 90;
    int Evasion = 5;
    int Movement = 3;
    int Range = 1;
    Facing Direction = Facing::South;
    std::array<bool,SkillCount> Skills{};
    std::array<int,SkillCount> Cooldowns{};
    bool CreatesCover=false;
    bool AttackCursed=false;
    bool MovementWebbed=false;
    int SummonerId=-1;
    TimedBoost AttackBoost{},DefenseBoost{},AccuracyBoost{};
    bool operator==(const BattleUnit&) const = default;
};
struct BattleSnapshot {
    int Width = 8;
    int Height = 8;
    std::vector<Tile> Tiles;
    std::vector<BattleUnit> Units;
    std::vector<int> Order;
    int ActiveId = -1;
    int Round = 1;
    bool Moved = false;
    bool Acted = false;
    bool ChargeReady = false;
    BattleOutcome Outcome = BattleOutcome::Ongoing;
    std::uint64_t Revision = 0;
    int UsedSkill = -1;
    int Medicines = 0;
    BattleBond BondGains;
    ComboSnapshot Combos;
    std::optional<ComboResolution> LastCombo;
    AbilityRuleSet AbilityRules=AbilityRuleSet::Legacy;
    std::array<int,8> StartingBonds{};
    bool PersonalityAI=false;
    EnemyThinking EnemyAI=EnemyThinking::Legacy;
    bool EnemyCharacterSkills=false;
    bool operator==(const BattleSnapshot&) const = default;
};
struct PredictedHit {
    int TargetId;
    int Chance;
    int Damage;
    bool operator==(const PredictedHit&) const = default;
};
struct PredictedAid {
    int TargetId=-1,Healing=0,AttackBuff=0;
    bool Cleanse=false;
    bool operator==(const PredictedAid&) const=default;
};
struct AttackPreview {
    bool Legal = false;
    std::vector<PredictedHit> Hits;
    int TerrainDamage = 0;
    std::vector<PredictedAid> Aid;
    int SelfDefenseBuff=0;
    bool operator==(const AttackPreview&) const = default;
};
struct AttackHit {
    int TargetId;
    bool Hit;
    int Damage;
    bool operator==(const AttackHit&) const = default;
};
struct SkillPreview {
    bool Legal = false;
    AttackPreview Attack;
    int TargetId = -1;
    int Healing = 0;
    bool Support = false;
    int AttackBuff=0,DefenseBuff=0,AccuracyBuff=0;
    bool Cleanse=false;
    bool operator==(const SkillPreview&) const = default;
};
struct MovePreview {
    bool Legal = false;
    std::vector<Position> Path;
    int Damage = 0;
    bool KnockedOut = false;
    bool operator==(const MovePreview&) const = default;
};
struct BattleCheckpoint {
    BattleSnapshot State;
    std::uint64_t RandomState=0;
    BattleGoal Goal=BattleGoal::Eliminate;
    int SurvivalRounds=3;
    std::size_t TurnIndex=0;
    std::vector<AttackHit> LastHits;
    int EnemyHpPercent=100;
    bool operator==(const BattleCheckpoint&) const = default;
};
class Battle {
public:
    Battle(int width, int height, std::vector<Tile> tiles, std::vector<BattleUnit> units,
        std::uint64_t seed, BattleGoal goal = BattleGoal::Eliminate, int survivalRounds = 3, int medicines = 0, int enemyHpPercent = 100);
    const BattleSnapshot& Snapshot() const { return state_; }
    BattleCheckpoint Checkpoint() const;
    static std::optional<Battle> Restore(const BattleCheckpoint& checkpoint);
    std::uint64_t RandomState() const { return random_.State(); }
    const std::vector<AttackHit>& LastHits() const { return lastHits_; }
    bool HasLineOfSight(Position from, Position to) const;
    std::vector<Position> Reachable(int actorId) const;
    int MovementAllowance(int actorId) const;
    MovePreview PreviewMove(int actorId, Position to) const;
    AttackPreview PreviewAttack(int actorId, Position target, int radius = 0, bool normal = true) const;
    int NormalAttackRadius(int actorId) const;
    SkillPreview PreviewSkill(int actorId, Skill skill, Position target) const;
    bool UseSkill(std::uint64_t revision,int actorId,Skill skill,Position target);
    SkillPreview PreviewMedicine(int actorId,Position target) const;
    bool UseMedicine(std::uint64_t revision,int actorId,Position target);
    AttackPreview PreviewUltimate(int actorId,Position target) const;
    bool UseUltimate(std::uint64_t revision,int actorId,Position target);
    bool CanTeamUltimate(int actorId) const;
    AttackPreview PreviewTeamUltimate(int actorId,Position target) const;
    bool UseTeamUltimate(std::uint64_t revision,int actorId,Position target);
    bool Move(std::uint64_t revision, int actorId, Position to);
    bool Attack(std::uint64_t revision, int actorId, Position target, int radius = 0);
    bool EndTurn(std::uint64_t revision, int actorId, Facing facing);
    int PotentialEnemyDamage(int targetId) const;
    int PotentialHostileDamage(int targetId) const;
    std::vector<Position> ConnectedTerrain(Position from, bool avoidTraps=false) const;
    // 壁・障害物・段差を迂回した歩数。到達できなければ-1。ユニットの位置は考慮しない。
    int RouteDistance(Position from, Position to) const;
private:
    bool ProjectEnemySupport(int actorId,const Battle& timeline);
    AttackPreview PreviewCharacterUltimate(int actorId,Position target) const;
    bool UseCharacterUltimate(int actorId,const AttackPreview& preview);
    SkillPreview PreviewCharacterSkill(int actorId,Skill skill,Position target) const;
    bool UseCharacterSkill(std::uint64_t revision,int actorId,Skill skill,Position target,const SkillPreview& preview);
    explicit Battle(const BattleCheckpoint& checkpoint);
    bool ValidSetup(bool initial) const;
    bool ValidCheckpoint() const;
    void RecordCombo(int actorId,int attributes,bool succeeded,const std::vector<int>& enemies={},const std::vector<int>& knockedOut={});
    bool AttackWithSyntax(std::uint64_t revision,int actorId,Position target,int radius,int attributes,bool applyCurse=false,bool applyWeb=false,bool normal=false);
    bool Inside(Position at) const;
    bool TerrainStep(Position from, Position to) const;
    bool TraceLine(Position from, Position to, bool targetObstacle) const;
    bool Active(int id) const;
    const BattleUnit* Find(int id) const;
    BattleUnit* Find(int id);
    int Occupant(Position at) const;
    void RebuildOrder();
    void AdvanceTurn();
    void Evaluate();
    void GeneratePeriodicCover();
    void GeneratePeriodicSummons();
    BattleSnapshot state_;
    Random random_;
    BattleGoal goal_;
    int survivalRounds_;
    int enemyHpPercent_=100;
    std::size_t turnIndex_ = 0;
    std::vector<AttackHit> lastHits_;
    ComboLedger combos_;
};
}
