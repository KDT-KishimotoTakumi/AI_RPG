#pragma once
namespace GirlsSpire {
enum class TextSize { Standard, Large, Largest };
struct Preferences {
    TextSize Text=TextSize::Standard;
    int BgmVolume=100,SfxVolume=100;
    bool Subtitles=true,ReducedEffects=false,SuppressIntimacy=true;
    bool operator==(const Preferences&) const = default;
};
bool ValidPreferences(const Preferences& value);
int TextScalePercent(TextSize size);
}
