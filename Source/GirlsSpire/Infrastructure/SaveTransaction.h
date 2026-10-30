#pragma once
#include "SaveSlot.h"
#include "SavePayload.h"

namespace GirlsSpire {
class JournalFiles {
public:
    virtual ~JournalFiles()=default;
    virtual SlotBytes ReadJournal()=0;
    virtual bool ReplaceJournal(std::string_view bytes)=0;
    virtual bool PreserveJournal();
};
struct TransactionLoad {
    SlotIssue Issue=SlotIssue::None;
    SlotIssue RunIssue=SlotIssue::Empty;
    SlotIssue ProfileIssue=SlotIssue::Empty;
    std::optional<RunSaveState> Run;
    Profile Permanent;
    bool Recovered=false;
    bool JournalRepairable=false;
    std::uint64_t Generation=0;
};
class SaveTransaction {
public:
    SaveTransaction(SlotFiles& slots,JournalFiles& journal) : slots_(slots),journal_(journal) {}
    TransactionLoad Load();
    SlotIssue Prepare(const std::optional<RunSaveState>& run,const Profile& profile);
    SlotIssue PrepareResume();
    SlotIssue PrepareReset(SaveKind kind);
    SlotIssue PrepareJournalRepair();
    SlotIssue Retry();
    bool HasPending() const;
private:
    struct Commit {
        std::string RunBytes,ProfileBytes,JournalBytes;
        std::uint64_t Generation=0;
    };
    static std::optional<Commit> DecodeJournal(std::string_view bytes,SlotIssue& issue);
    static std::optional<Commit> MakeCommit(std::string runBytes,std::string profileBytes,std::uint64_t generation);
    SlotIssue BuildJournalRepair(std::optional<Commit>& repair);
    SlotFiles& slots_;
    JournalFiles& journal_;
    std::optional<Commit> pending_;
};
}
