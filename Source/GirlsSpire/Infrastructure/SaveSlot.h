#pragma once
#include "SaveEnvelope.h"

namespace GirlsSpire {
struct SlotBytes {
    bool Failed=false;
    std::optional<std::string> Bytes;
};
class SlotFiles {
public:
    virtual ~SlotFiles()=default;
    virtual SlotBytes Read(SaveKind kind,bool backup)=0;
    virtual bool Replace(SaveKind kind,bool backup,std::string_view bytes)=0;
    virtual bool Preserve(SaveKind kind);
};
enum class SlotIssue {None,Empty,Corrupt,UnsupportedVersion,ReadFailed,Pending,NoPending,Invalid,GenerationOverflow,WriteFailed};
struct SlotLoad {
    SlotIssue Issue=SlotIssue::Empty;
    std::optional<SaveEnvelope> Value;
    bool Recovered=false;
};
class SaveSlot {
public:
    SaveSlot(SlotFiles& files,SaveKind kind) : files_(files),kind_(kind) {}
    SlotLoad Load() const;
    SlotIssue Prepare(std::string payload);
    SlotIssue Retry();
    bool HasPending() const;
private:
    SlotFiles& files_;
    SaveKind kind_;
    std::optional<std::string> pending_;
};
}
