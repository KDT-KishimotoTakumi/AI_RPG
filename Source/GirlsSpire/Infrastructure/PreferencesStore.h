#pragma once
#include "Domain/Preferences.h"
#include "SaveSlot.h"
namespace GirlsSpire {
class PreferencesFiles {
public:
    virtual ~PreferencesFiles()=default;
    virtual SlotBytes ReadPreferences()=0;
    virtual bool ReplacePreferences(std::string_view bytes)=0;
    virtual bool PreservePreferences()=0;
};
enum class PreferencesIssue {None,Empty,Corrupt,ReadFailed,WriteFailed,Invalid,PreserveFailed};
struct PreferencesLoad {PreferencesIssue Issue=PreferencesIssue::Empty;Preferences Value;};
class PreferencesStore {
public:
    explicit PreferencesStore(PreferencesFiles& files):files_(files){}
    PreferencesLoad Load() const;
    PreferencesIssue Save(const Preferences& value);
    PreferencesIssue RecoverDefaults();
private:
    PreferencesFiles& files_;
};
}
