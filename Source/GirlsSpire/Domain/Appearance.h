#pragma once
namespace GirlsSpire {
struct Profile;
enum class AppearanceVariant { Standard, Alternate };
bool ValidAppearanceSelection(const Profile& profile);
bool CanChooseAppearance(const Profile& profile,int actorId,AppearanceVariant variant);
bool ChooseAppearance(Profile& profile,int actorId,AppearanceVariant variant);
}
