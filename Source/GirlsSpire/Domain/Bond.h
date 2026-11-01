#pragma once
#include <array>
#include <vector>
namespace GirlsSpire {
enum class ConversationChoice { Warm, Neutral, Cold };
struct Profile {
    // 仲間の人物ID1～8を添字0～7へ対応させ、主人公の欄を持たない。
    std::array<int,8> Bonds{};
    std::array<bool,8> SpecialConversations{};
    // 0は単独、1～8は人物IDに対応する後日談。
    std::array<bool,9> Epilogues{};
    std::vector<int> DiscoveredCombos;
    bool operator==(const Profile&) const = default;
};
bool ValidProfile(const Profile& profile);
bool ApplyConversation(Profile& profile,int actorId,ConversationChoice choice);
bool DiscoverCombo(Profile& profile,int recipeId);
}
