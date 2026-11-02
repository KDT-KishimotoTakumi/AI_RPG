#include "AudioLevels.h"
namespace GirlsSpire {
std::optional<AudioLevels> ResolveAudioLevels(const Preferences& value)
{
    if(!ValidPreferences(value))return std::nullopt;
    return AudioLevels{value.BgmVolume/100.f,value.SfxVolume/100.f};
}
}
