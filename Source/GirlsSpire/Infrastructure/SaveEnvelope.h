#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace GirlsSpire {
enum class SaveKind : std::uint32_t { Run = 1, Profile = 2 };
enum class SaveError { None, Corrupt, UnsupportedVersion, WrongKind };
struct SaveEnvelope {
    SaveKind Kind = SaveKind::Run;
    std::uint64_t Generation = 1;
    std::string Payload;
    bool operator==(const SaveEnvelope&) const = default;
};
struct SaveDecodeResult {
    SaveError Error = SaveError::Corrupt;
    std::optional<SaveEnvelope> Value;
};
inline constexpr std::size_t MaxSavePayload = 16 * 1024 * 1024;
std::optional<std::string> EncodeSave(const SaveEnvelope& value);
SaveDecodeResult DecodeSave(std::string_view bytes, SaveKind expected);
}
