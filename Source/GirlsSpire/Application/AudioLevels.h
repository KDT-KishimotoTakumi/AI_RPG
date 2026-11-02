#pragma once
#include "Domain/Preferences.h"
#include <optional>
namespace GirlsSpire {
struct AudioLevels {float Bgm=1,Sfx=1;bool operator==(const AudioLevels&) const=default;};
std::optional<AudioLevels> ResolveAudioLevels(const Preferences& preferences);
}
