#pragma once
#include "Application/Run.h"
#include "Application/CombatControl.h"
#include "SaveEnvelope.h"
#include "Domain/Preferences.h"
#include <string>
#include <string_view>

namespace GirlsSpire {
// 対応上限。標準外見だけの周回は同一バイトのv19で保存する。
inline constexpr std::uint32_t RunPayloadSchema=20,ProfilePayloadSchema=7;
SaveError InspectPayload(std::string_view bytes,SaveKind expected);
struct SavedCombat {
    BattleCheckpoint Battle;
    CombatControlCheckpoint Control;
    AppearanceSelection Appearances{};
    bool operator==(const SavedCombat&) const = default;
};
struct RunSaveState {
    RunCheckpoint Run;
    std::optional<SavedCombat> Combat;
    bool ResumeAvailable=true;
    bool operator==(const RunSaveState&) const = default;
};
std::optional<std::string> EncodeRunPayload(const RunSaveState& state);
std::optional<RunSaveState> DecodeRunPayload(std::string_view bytes);
std::optional<std::string> EncodeProfilePayload(const Profile& profile);
std::optional<Profile> DecodeProfilePayload(std::string_view bytes);
std::optional<std::string> EncodePreferencesPayload(const Preferences& preferences);
std::optional<Preferences> DecodePreferencesPayload(std::string_view bytes);
}
