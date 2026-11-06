#pragma once
#include <array>
namespace GirlsSpire {
struct Profile;
enum class AppearanceVariant { Standard, Alternate };
using AppearanceSelection=std::array<AppearanceVariant,9>;
bool ValidAppearanceSnapshot(const AppearanceSelection& appearances);
bool ValidAppearanceSelection(const Profile& profile);
bool CanChooseAppearance(const Profile& profile,int actorId,AppearanceVariant variant);
bool ChooseAppearance(Profile& profile,int actorId,AppearanceVariant variant);
}
