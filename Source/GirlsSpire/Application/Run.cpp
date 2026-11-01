#include "Run.h"
#include <algorithm>
#include <stdexcept>

namespace GirlsSpire {
Run::Run(std::uint64_t seed, Difficulty difficulty, HeroRole role,Profile profile)
    : random_(seed), profile_(profile), state_{}
{
    if(!ValidProfile(profile_)) throw std::invalid_argument("絆データが無効です");
    if (difficulty < Difficulty::Beginner || difficulty > Difficulty::Advanced ||
        role < HeroRole::Sword || role > HeroRole::Matchlock) {
        throw std::invalid_argument("難易度または役割が無効です");
    }
    state_.Level = difficulty;
    state_.Role = role;
    BeginDay();
}

bool Run::ChooseNode(std::uint64_t revision, int index)
{
    if (state_.Phase != RunPhase::Route || state_.PendingRescue>=0 || revision != state_.Revision ||
        index < 0 || static_cast<std::size_t>(index) >= state_.Choices.size()) return false;
    state_.Active = state_.Choices[static_cast<std::size_t>(index)];
    if(state_.Active.Kind==NodeKind::Shop) state_.Stock={};
    state_.Choices.clear();
    state_.Phase = RunPhase::Event;
    ++state_.Revision;
    return true;
}

bool Run::CompleteEvent(std::uint64_t revision, bool partyDefeated)
{
    if (state_.Phase != RunPhase::Event || revision != state_.Revision) return false;
    ++state_.Revision;
    if (partyDefeated) {
        state_.Growth={};
        state_.Health={};
        state_.Levels={};
        state_.Wallet={0,0};state_.Accessories={};
        state_.Phase = RunPhase::Lost;
        return true;
    }
    if (state_.Active.Kind == NodeKind::Rescue) {
        state_.Recruited.push_back(state_.Active.Candidate);
        state_.Levels[state_.Active.Candidate+1]={state_.Levels[0].Level,0};
    }
    if (state_.Active.Kind == NodeKind::FinalBoss) {
        state_.Phase = RunPhase::Won;
        return true;
    }
    state_.Phase = RunPhase::Route;
    ++state_.EventIndex;
    if (state_.EventIndex == state_.EventsToday) {
        ++state_.Day;
        BeginDay();
    } else {
        GenerateChoices();
    }
    return true;
}

std::vector<int> Run::Unrecruited() const
{
    return Unrecruited(state_);
}
bool Run::DiscoverCombo(int recipeId)
{
    return GirlsSpire::DiscoverCombo(profile_,recipeId);
}
std::vector<int> Run::Unrecruited(const RunSnapshot& state)
{
    std::vector<int> result;
    for (int candidate = 0; candidate < 8; ++candidate) {
        if (std::find(state.Recruited.begin(), state.Recruited.end(), candidate) == state.Recruited.end())
            result.push_back(candidate);
    }
    return result;
}

void Run::BeginDay()
{
    state_.EventIndex = 0;
    state_.EventsToday = 1 + static_cast<int>(random_.Uniform(2));
    GenerateChoices();
}

void Run::GenerateChoices()
{
    state_.Choices.clear();
    // 必須ノードを経路自体に組み込み、UIや呼出元から飛ばせないようにする。
    if ((state_.Day == 6 || state_.Day == 12) && state_.EventIndex == state_.EventsToday - 1) {
        state_.Choices.push_back({state_.Day == 6 ? NodeKind::MidBoss : NodeKind::FinalBoss});
    } else if (state_.Day <= 4 && state_.EventIndex == 0) {
        auto candidates = Unrecruited();
        for (int i = 0; i < 3; ++i) {
            const auto index = random_.Uniform(static_cast<std::uint32_t>(candidates.size()));
            state_.Choices.push_back({NodeKind::Rescue, candidates[index]});
            candidates.erase(candidates.begin() + index);
        }
    } else {
        std::vector<NodeKind> kinds{NodeKind::Battle, NodeKind::Elite, NodeKind::Social,
            NodeKind::Shop, NodeKind::Rest, NodeKind::Anomaly};
        for (int i = 0; i < 3; ++i) {
            const auto index = random_.Uniform(static_cast<std::uint32_t>(kinds.size()));
            state_.Choices.push_back({kinds[index]});
            kinds.erase(kinds.begin() + index);
        }
    }
}
}
