#include "PreferencesStore.h"
#include "SavePayload.h"
namespace GirlsSpire {
PreferencesLoad PreferencesStore::Load() const
{
    const auto read=files_.ReadPreferences();
    if(read.Failed)return {PreferencesIssue::ReadFailed,{}};
    if(!read.Bytes)return {};
    const auto value=DecodePreferencesPayload(*read.Bytes);
    return value?PreferencesLoad{PreferencesIssue::None,*value}:PreferencesLoad{PreferencesIssue::Corrupt,{}};
}
PreferencesIssue PreferencesStore::Save(const Preferences& value)
{
    const auto bytes=EncodePreferencesPayload(value);if(!bytes)return PreferencesIssue::Invalid;
    const auto loaded=Load();
    if(loaded.Issue!=PreferencesIssue::None && loaded.Issue!=PreferencesIssue::Empty)return loaded.Issue;
    return files_.ReplacePreferences(*bytes)?PreferencesIssue::None:PreferencesIssue::WriteFailed;
}
PreferencesIssue PreferencesStore::RecoverDefaults()
{
    const auto loaded=Load();
    if(loaded.Issue==PreferencesIssue::ReadFailed)return loaded.Issue;
    if(loaded.Issue!=PreferencesIssue::Corrupt)return PreferencesIssue::Invalid;
    if(!files_.PreservePreferences())return PreferencesIssue::PreserveFailed;
    return files_.ReplacePreferences(*EncodePreferencesPayload({}))?PreferencesIssue::None:PreferencesIssue::WriteFailed;
}
}
