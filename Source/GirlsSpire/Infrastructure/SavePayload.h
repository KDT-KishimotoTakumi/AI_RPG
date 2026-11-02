#pragma once
#include "Application/Run.h"
#include "Application/CombatControl.h"
#include "SaveEnvelope.h"
#include "Domain/Preferences.h"
#include <string>
#include <string_view>

namespace GirlsSpire {
inline constexpr std::uint32_t RunPayloadSchema=14,ProfilePayloadSchema=4;
SaveError InspectPayload(std::string_view bytes,SaveKind expected);
struct SavedCombat {
    BattleCheckpoint Battle;
    CombatControlCheckpoint Control;
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
