#pragma once
#include "Appearance.h"
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
    // 0=初級、1=中級、2=上級。解放は連続し、周回終了でも失わない。
    int HighestDifficulty=0;
    std::vector<int> DiscoveredEnemies;
    // 初勝利を後日談選択前に確定する。旧記録の後日談も勝利の根拠になる。
    bool VictoryRecorded=false;
    std::array<AppearanceVariant,9> SelectedAppearances{};
    bool operator==(const Profile&) const = default;
};
bool ValidProfile(const Profile& profile);
bool CanStartDifficulty(const Profile& profile,int difficulty);
bool ApplyConversation(Profile& profile,int actorId,ConversationChoice choice);
bool DiscoverCombo(Profile& profile,int recipeId);
}
