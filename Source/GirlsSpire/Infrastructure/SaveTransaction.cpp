#include "SaveTransaction.h"
#include <algorithm>
#include <limits>
namespace GirlsSpire {
bool JournalFiles::PreserveJournal() {return false;}
SlotIssue SaveTransaction::PrepareJournalRepair()
{
    std::optional<Commit> repair;
    const auto issue=BuildJournalRepair(repair);
    if(issue!=SlotIssue::None) return issue;
    if(!journal_.PreserveJournal()) return SlotIssue::WriteFailed;
    pending_=std::move(repair);return SlotIssue::None;
}
SlotIssue SaveTransaction::BuildJournalRepair(std::optional<Commit>& repair)
{
    if(pending_) return SlotIssue::Pending;
    const auto journal=journal_.ReadJournal();
    if(journal.Failed) return SlotIssue::ReadFailed;
    if(!journal.Bytes) return SlotIssue::Invalid;
    const auto frame=DecodeSave(*journal.Bytes,SaveKind::Run);
    if(frame.Error==SaveError::UnsupportedVersion) return SlotIssue::UnsupportedVersion;
    if(frame.Value) {
        if(frame.Value->Payload.empty()) return SlotIssue::Invalid;
        SlotIssue issue=SlotIssue::Corrupt;
        if(DecodeJournal(*journal.Bytes,issue)) return SlotIssue::Invalid;
        if(issue==SlotIssue::UnsupportedVersion) return issue;
    }
    const auto run=SaveSlot(slots_,SaveKind::Run).Load();
    const auto profile=SaveSlot(slots_,SaveKind::Profile).Load();
    for(const auto issue : {run.Issue,profile.Issue})
        if(issue!=SlotIssue::None) return issue==SlotIssue::Empty?SlotIssue::Corrupt:issue;
    if(run.Value->Generation!=profile.Value->Generation) return SlotIssue::Corrupt;
    for(const auto* value : {&*run.Value,&*profile.Value}) {
        const bool valid=value->Kind==SaveKind::Run?
            (value->Payload.empty() || DecodeRunPayload(value->Payload).has_value()):
            DecodeProfilePayload(value->Payload).has_value();
        if(!valid) return InspectPayload(value->Payload,value->Kind)==SaveError::UnsupportedVersion?
            SlotIssue::UnsupportedVersion:SlotIssue::Corrupt;
    }
    auto generation=run.Value->Generation;
    if(frame.Value) generation=std::max(generation,frame.Value->Generation);
    if(generation==std::numeric_limits<std::uint64_t>::max()) return SlotIssue::GenerationOverflow;
    ++generation;
    // 同じ確定世代の両側だけを使用する。世代不一致は推測で補わない。
    const auto commit=MakeCommit(*EncodeSave({SaveKind::Run,generation,run.Value->Payload}),
        *EncodeSave({SaveKind::Profile,generation,profile.Value->Payload}),generation);
    if(!commit) return SlotIssue::Invalid;
    repair=*commit;return SlotIssue::None;
}
SlotIssue SaveTransaction::PrepareReset(SaveKind kind)
{
    if(kind!=SaveKind::Run && kind!=SaveKind::Profile) return SlotIssue::Invalid;
    const auto loaded=Load();
    if(loaded.Issue!=SlotIssue::None) return loaded.Issue;
    const auto target=kind==SaveKind::Run?loaded.RunIssue:loaded.ProfileIssue;
    const auto other=kind==SaveKind::Run?loaded.ProfileIssue:loaded.RunIssue;
    if(target==SlotIssue::UnsupportedVersion || other==SlotIssue::UnsupportedVersion) return SlotIssue::UnsupportedVersion;
    if(target!=SlotIssue::Corrupt) return SlotIssue::Invalid;
    if(other!=SlotIssue::None && other!=SlotIssue::Empty && other!=SlotIssue::Corrupt) return other;
    auto generation=loaded.Generation;
    for(const auto side : {SaveKind::Run,SaveKind::Profile}) for(bool backup : {false,true}) {
        const auto bytes=slots_.Read(side,backup);
        if(bytes.Failed) return SlotIssue::ReadFailed;
        if(bytes.Bytes) if(const auto decoded=DecodeSave(*bytes.Bytes,side);decoded.Value)
            generation=std::max(generation,decoded.Value->Generation);
    }
    const auto journal=journal_.ReadJournal();
    if(journal.Failed) return SlotIssue::ReadFailed;
    if(journal.Bytes) if(const auto decoded=DecodeSave(*journal.Bytes,SaveKind::Run);decoded.Value)
        generation=std::max(generation,decoded.Value->Generation);
    if(generation==std::numeric_limits<std::uint64_t>::max()) return SlotIssue::GenerationOverflow;
    ++generation;
    std::string runBytes,profileBytes;
    if(kind==SaveKind::Run || other!=SlotIssue::Corrupt) {
        const auto payload=kind==SaveKind::Run || !loaded.Run?std::string{}:*EncodeRunPayload(*loaded.Run);
        runBytes=*EncodeSave({SaveKind::Run,generation,payload});
    }
    if(kind==SaveKind::Profile || other!=SlotIssue::Corrupt) {
        const auto payload=*EncodeProfilePayload(kind==SaveKind::Profile?Profile{}:loaded.Permanent);
        profileBytes=*EncodeSave({SaveKind::Profile,generation,payload});
    }
    const auto commit=MakeCommit(std::move(runBytes),std::move(profileBytes),generation);
    if(!commit) return SlotIssue::Invalid;
    if(!slots_.Preserve(kind)) return SlotIssue::WriteFailed;
    pending_=*commit;return SlotIssue::None;
}
SlotIssue SaveTransaction::PrepareResume()
{
    const auto loaded=Load();
    if(loaded.Issue!=SlotIssue::None) return loaded.Issue;
    for(const auto issue : {loaded.RunIssue,loaded.ProfileIssue})
        if(issue!=SlotIssue::None && issue!=SlotIssue::Empty) return issue;
    if(!loaded.Run || !loaded.Run->ResumeAvailable) return SlotIssue::Invalid;
    auto consumed=*loaded.Run;consumed.ResumeAvailable=false;
    return Prepare(consumed,loaded.Permanent);
}
bool SaveTransaction::HasPending() const {return pending_.has_value();}
std::optional<SaveTransaction::Commit> SaveTransaction::DecodeJournal(std::string_view bytes,SlotIssue& issue)
{
    issue=SlotIssue::Corrupt;
    const auto decoded=DecodeSave(bytes,SaveKind::Run);
    if(!decoded.Value || decoded.Value->Payload.size()<4) return std::nullopt;
    const auto& payload=decoded.Value->Payload;
    std::uint32_t length=0;
    for(int i=0;i<4;++i) length|=static_cast<std::uint32_t>(static_cast<unsigned char>(payload[i]))<<(8*i);
    if(length>payload.size()-4) return std::nullopt;
    const auto runBytes=payload.substr(4,length),profileBytes=payload.substr(4+length);
    if(runBytes.empty() && profileBytes.empty()) return std::nullopt;
    if(!runBytes.empty()) {
        const auto run=DecodeSave(runBytes,SaveKind::Run);
        if(run.Error==SaveError::UnsupportedVersion) issue=SlotIssue::UnsupportedVersion;
        if(!run.Value || run.Value->Generation!=decoded.Value->Generation) return std::nullopt;
        if(!run.Value->Payload.empty() && !DecodeRunPayload(run.Value->Payload)) {
            if(InspectPayload(run.Value->Payload,SaveKind::Run)==SaveError::UnsupportedVersion) issue=SlotIssue::UnsupportedVersion;
            return std::nullopt;
        }
    }
    if(!profileBytes.empty()) {
        const auto profile=DecodeSave(profileBytes,SaveKind::Profile);
        if(profile.Error==SaveError::UnsupportedVersion) issue=SlotIssue::UnsupportedVersion;
        if(!profile.Value || profile.Value->Generation!=decoded.Value->Generation) return std::nullopt;
        if(!DecodeProfilePayload(profile.Value->Payload)) {
            if(InspectPayload(profile.Value->Payload,SaveKind::Profile)==SaveError::UnsupportedVersion) issue=SlotIssue::UnsupportedVersion;
            return std::nullopt;
        }
    }
    issue=SlotIssue::None;return Commit{runBytes,profileBytes,std::string(bytes),decoded.Value->Generation};
}
TransactionLoad SaveTransaction::Load()
{
    TransactionLoad result;
    if(pending_) {result.Issue=SlotIssue::Pending;return result;}
    const auto journal=journal_.ReadJournal();
    if(journal.Failed) {result.Issue=SlotIssue::ReadFailed;return result;}
    if(journal.Bytes) {
        const auto decoded=DecodeSave(*journal.Bytes,SaveKind::Run);
        if(!decoded.Value) {
            result.Issue=decoded.Error==SaveError::UnsupportedVersion?SlotIssue::UnsupportedVersion:SlotIssue::Corrupt;
            if(result.Issue==SlotIssue::Corrupt) {
                std::optional<Commit> repair;
                result.JournalRepairable=BuildJournalRepair(repair)==SlotIssue::None;
            }
            return result;
        }
        if(!decoded.Value->Payload.empty()) {
            SlotIssue issue=SlotIssue::Corrupt;
            pending_=DecodeJournal(*journal.Bytes,issue);
            if(!pending_) {
                result.Issue=issue;
                if(issue==SlotIssue::Corrupt) {
                    std::optional<Commit> repair;
                    result.JournalRepairable=BuildJournalRepair(repair)==SlotIssue::None;
                }
                return result;
            }
            result.Issue=Retry();
            if(result.Issue!=SlotIssue::None) return result;
            result.Recovered=true;
        }
    }
    SaveSlot runSlot(slots_,SaveKind::Run),profileSlot(slots_,SaveKind::Profile);
    const auto run=runSlot.Load(),profile=profileSlot.Load();
    result.RunIssue=run.Issue;result.ProfileIssue=profile.Issue;
    if(run.Value && profile.Issue==SlotIssue::Empty) result.ProfileIssue=SlotIssue::Corrupt;
    if(profile.Value && run.Issue==SlotIssue::Empty) result.RunIssue=SlotIssue::Corrupt;
    result.Recovered=result.Recovered || run.Recovered || profile.Recovered;
    if(run.Value) {
        result.Generation=run.Value->Generation;
        if(!run.Value->Payload.empty()) {
            result.Run=DecodeRunPayload(run.Value->Payload);
            if(!result.Run) result.RunIssue=InspectPayload(run.Value->Payload,SaveKind::Run)==SaveError::UnsupportedVersion?SlotIssue::UnsupportedVersion:SlotIssue::Corrupt;
        }
    }
    if(profile.Value) {
        result.Generation=std::max(result.Generation,profile.Value->Generation);
        const auto permanent=DecodeProfilePayload(profile.Value->Payload);
        if(permanent) result.Permanent=*permanent;
        else result.ProfileIssue=InspectPayload(profile.Value->Payload,SaveKind::Profile)==SaveError::UnsupportedVersion?SlotIssue::UnsupportedVersion:SlotIssue::Corrupt;
    }
    // ジャーナル再生後も世代が一致しなければ、組み合わせてゲームへ渡さない。
    if(run.Value && profile.Value && result.RunIssue==SlotIssue::None && result.ProfileIssue==SlotIssue::None &&
        run.Value->Generation!=profile.Value->Generation) result.Issue=SlotIssue::Corrupt;
    return result;
}
SlotIssue SaveTransaction::Prepare(const std::optional<RunSaveState>& run,const Profile& profile)
{
    const auto loaded=Load();
    if(loaded.Issue!=SlotIssue::None) return loaded.Issue;
    for(const auto issue : {loaded.RunIssue,loaded.ProfileIssue})
        if(issue!=SlotIssue::None && issue!=SlotIssue::Empty) return issue;
    if(loaded.Generation==std::numeric_limits<std::uint64_t>::max()) return SlotIssue::GenerationOverflow;
    const auto runPayload=run?EncodeRunPayload(*run):std::optional<std::string>{std::string{}};
    const auto profilePayload=EncodeProfilePayload(profile);
    if(!runPayload || !profilePayload) return SlotIssue::Invalid;
    const auto generation=loaded.Generation+1;
    const auto runBytes=EncodeSave({SaveKind::Run,generation,*runPayload});
    const auto profileBytes=EncodeSave({SaveKind::Profile,generation,*profilePayload});
    if(!runBytes || !profileBytes) return SlotIssue::Invalid;
    pending_=MakeCommit(*runBytes,*profileBytes,generation);
    return pending_?SlotIssue::None:SlotIssue::Invalid;
}
std::optional<SaveTransaction::Commit> SaveTransaction::MakeCommit(std::string runBytes,std::string profileBytes,std::uint64_t generation)
{
    std::string payload;
    const auto length=static_cast<std::uint32_t>(runBytes.size());
    for(int i=0;i<4;++i) payload.push_back(static_cast<char>((length>>(8*i))&255));
    payload+=runBytes;payload+=profileBytes;
    const auto journal=EncodeSave({SaveKind::Run,generation,std::move(payload)});
    if(!journal) return std::nullopt;
    return Commit{std::move(runBytes),std::move(profileBytes),*journal,generation};
}
SlotIssue SaveTransaction::Retry()
{
    if(!pending_) return SlotIssue::NoPending;
    const auto& commit=*pending_;
    const auto currentJournal=journal_.ReadJournal();
    if(currentJournal.Failed) return SlotIssue::ReadFailed;
    if(currentJournal.Bytes) {
        const auto current=DecodeSave(*currentJournal.Bytes,SaveKind::Run);
        if(current.Error==SaveError::UnsupportedVersion) return SlotIssue::UnsupportedVersion;
        if(current.Value && (current.Value->Generation>commit.Generation ||
            (current.Value->Generation==commit.Generation && !current.Value->Payload.empty() && *currentJournal.Bytes!=commit.JournalBytes)))
            return SlotIssue::Invalid;
    }
    // 再起動した別の所有者が先へ進んだ場合、古い固定内容を上書きしない。
    for(const auto kind : {SaveKind::Run,SaveKind::Profile}) for(bool backup : {false,true}) {
        const auto& expected=kind==SaveKind::Run?commit.RunBytes:commit.ProfileBytes;
        if(expected.empty()) continue;
        const auto bytes=slots_.Read(kind,backup);
        if(bytes.Failed) return SlotIssue::ReadFailed;
        if(!bytes.Bytes) continue;
        const auto current=DecodeSave(*bytes.Bytes,kind);
        if(current.Error==SaveError::UnsupportedVersion) return SlotIssue::UnsupportedVersion;
        if(current.Value && (current.Value->Generation>commit.Generation ||
            (current.Value->Generation==commit.Generation && *bytes.Bytes!=expected))) return SlotIssue::Invalid;
    }
    if(!journal_.ReplaceJournal(commit.JournalBytes) ||
        (!commit.ProfileBytes.empty() && (!slots_.Replace(SaveKind::Profile,true,commit.ProfileBytes) ||
            !slots_.Replace(SaveKind::Profile,false,commit.ProfileBytes))) ||
        (!commit.RunBytes.empty() && (!slots_.Replace(SaveKind::Run,true,commit.RunBytes) ||
            !slots_.Replace(SaveKind::Run,false,commit.RunBytes)))) return SlotIssue::WriteFailed;
    const auto completed=EncodeSave({SaveKind::Run,commit.Generation,{}});
    if(!journal_.ReplaceJournal(*completed)) return SlotIssue::WriteFailed;
    pending_.reset();return SlotIssue::None;
}
}
