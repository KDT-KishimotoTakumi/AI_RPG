#include "Preferences.h"
namespace GirlsSpire {
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
