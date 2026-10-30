#pragma once
#include "Domain/Random.h"
#include "Domain/Growth.h"
#include "Domain/Health.h"
#include "Domain/Level.h"
#include "Domain/Shop.h"
#include "Domain/Bond.h"
#include <cstdint>
#include <optional>
#include <vector>

namespace GirlsSpire {
enum class Difficulty { Beginner, Intermediate, Advanced };
enum class HeroRole { Sword, Naginata, CommandFan, Matchlock };
enum class NodeKind { Battle, Elite, Rescue, Social, Shop, Rest, Anomaly, MidBoss, FinalBoss };
enum class RunPhase { Route, Event, Won, Lost, Reward };
struct RouteNode {
    NodeKind Kind;
    int Candidate = -1;
    bool operator==(const RouteNode&) const = default;
};
struct RunSnapshot {
    int Day = 1;
    int EventIndex = 0;
    int EventsToday = 1;
    Difficulty Level=Difficulty::Beginner;
    HeroRole Role=HeroRole::Sword;
    RunPhase Phase = RunPhase::Route;
    std::uint64_t Revision = 0;
    std::vector<int> Recruited;
    std::vector<RouteNode> Choices;
    RouteNode Active{NodeKind::Battle};
    std::array<GrowthProgress,9> Growth{};
    std::array<HealthProgress,9> Health{};
    std::array<LevelProgress,9> Levels{};
    Purse Wallet;
    ShopStock Stock;
    std::array<Accessory,9> Accessories{};
    int RewardRecipient=-1;
    std::vector<Reward> Rewards;
    int PendingRescue=-1;
    bool operator==(const RunSnapshot&) const = default;
};
struct RunCheckpoint {
    RunSnapshot State;
    std::uint64_t RandomState=0;
    RunPhase AfterReward=RunPhase::Route;
    bool operator==(const RunCheckpoint&) const = default;
};
class Run {
public:
    Run(std::uint64_t seed, Difficulty difficulty, HeroRole role,Profile profile={});
    RunCheckpoint Checkpoint() const;
    static std::optional<Run> Restore(const RunCheckpoint& checkpoint,const Profile& profile);
    const Profile& ProfileState() const { return profile_; }
    bool Converse(std::uint64_t revision,int actorId,ConversationChoice choice);
    bool CanSpecialConverse(int actorId) const;
    bool SpecialConverse(std::uint64_t revision,int actorId);
    bool ConverseRescue(std::uint64_t revision,ConversationChoice choice);
    std::vector<int> EpilogueChoices() const;
    std::optional<Profile> ProfileForEpilogue(int actorId) const;
    const RunSnapshot& Snapshot() const { return state_; }
    std::uint64_t RandomState() const { return random_.State(); }
    bool ChooseNode(std::uint64_t revision, int index);
    bool CompleteEvent(std::uint64_t revision, bool partyDefeated = false);
    // 結果のみの経路検証用。実戦のHP・経験値・銭はSnapshot版で反映する。
    bool CompleteBattle(std::uint64_t revision,BattleOutcome outcome);
    bool CompleteBattle(std::uint64_t revision,const BattleSnapshot& battle);
    bool Rest(std::uint64_t revision,int patient=-1);
    bool Purchase(std::uint64_t revision,ShopOffer offer,int actorId);
    int MaxHp(int actorId) const;
    int CurrentHp(int actorId) const;
    bool ChooseRecipient(std::uint64_t revision,int actorId);
    bool ClaimReward(std::uint64_t revision,int index);
    std::vector<int> Unrecruited() const;
    static std::vector<int> Unrecruited(const RunSnapshot& state);
private:
    bool HasMember(int actorId) const;
    int BaseMaxHp(int actorId) const;
    void BeginDay();
    void GenerateChoices();
    Random random_;
    Profile profile_;
    RunSnapshot state_;
    RunPhase afterReward_=RunPhase::Route;
};
}
