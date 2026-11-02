#include "Preferences.h"
#include <algorithm>
#include <cstdint>
#include <limits>
namespace GirlsSpire {
int ScaledFontSize(TextSize size,int base)
{
    const auto percent=TextScalePercent(size);if(base<=0 || percent==0)return 0;
    const auto scaled=(static_cast<std::int64_t>(base)*percent+50)/100;
    return static_cast<int>(std::min<std::int64_t>(scaled,std::numeric_limits<int>::max()));
}
int TextScalePercent(TextSize size)
{
    switch(size) {
    case TextSize::Standard:return 100;
    case TextSize::Large:return 125;
    case TextSize::Largest:return 150;
    default:return 0;
    }
}
bool ValidPreferences(const Preferences& value)
{
    return TextScalePercent(value.Text)!=0 && value.BgmVolume>=0 && value.BgmVolume<=100 &&
        value.SfxVolume>=0 && value.SfxVolume<=100;
}
}
