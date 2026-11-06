#include "Appearance.h"
#include "Bond.h"
#include <algorithm>
namespace GirlsSpire {
namespace {
bool Unlocked(const Profile& profile,int actorId)
{
    // 呼出元で人物範囲を検証する。難易度解放だけでは旧勝利を推測しない。
    return actorId==0?profile.VictoryRecorded || std::any_of(profile.Epilogues.begin(),profile.Epilogues.end(),[](bool value){return value;}):profile.SpecialConversations[actorId-1];
}
}
bool ValidAppearanceSnapshot(const AppearanceSelection& appearances)
{
    return std::all_of(appearances.begin(),appearances.end(),[](auto variant){return variant==AppearanceVariant::Standard || variant==AppearanceVariant::Alternate;});
}
bool ValidAppearanceSelection(const Profile& profile)
{
    for(int id=0;id<=8;++id){
        const auto variant=profile.SelectedAppearances[id];
        if(variant!=AppearanceVariant::Standard && (variant!=AppearanceVariant::Alternate || !Unlocked(profile,id)))return false;
    }
    return true;
}
bool CanChooseAppearance(const Profile& profile,int actorId,AppearanceVariant variant)
{
    return ValidProfile(profile) && actorId>=0 && actorId<=8 &&
        (variant==AppearanceVariant::Standard || (variant==AppearanceVariant::Alternate && Unlocked(profile,actorId)));
}
bool ChooseAppearance(Profile& profile,int actorId,AppearanceVariant variant)
{
    if(!CanChooseAppearance(profile,actorId,variant))return false;
    profile.SelectedAppearances[actorId]=variant;return true;
}
}
