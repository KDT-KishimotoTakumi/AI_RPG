#include "SaveSlot.h"
#include <limits>
namespace GirlsSpire {
bool SlotFiles::Preserve(SaveKind) {return false;}
bool SaveSlot::HasPending() const {return pending_.has_value();}
SlotLoad SaveSlot::Load() const
{
    if(pending_) return {SlotIssue::Pending,{}};
    if(kind_!=SaveKind::Run && kind_!=SaveKind::Profile) return {SlotIssue::Invalid,{}};
    const auto main=files_.Read(kind_,false),backup=files_.Read(kind_,true);
    if(main.Failed || backup.Failed) return {SlotIssue::ReadFailed,{}};
    if(!main.Bytes && !backup.Bytes) return {};
    const auto a=main.Bytes?DecodeSave(*main.Bytes,kind_):SaveDecodeResult{};
    const auto b=backup.Bytes?DecodeSave(*backup.Bytes,kind_):SaveDecodeResult{};
    if(a.Error==SaveError::UnsupportedVersion || b.Error==SaveError::UnsupportedVersion)
        return {SlotIssue::UnsupportedVersion,{}};
    if(a.Value && b.Value && a.Value->Generation==b.Value->Generation && a.Value->Payload!=b.Value->Payload)
        return {SlotIssue::Corrupt,{}};
    if(b.Value && (!a.Value || b.Value->Generation>a.Value->Generation)) return {SlotIssue::None,b.Value,true};
    if(a.Value) return {SlotIssue::None,a.Value,false};
    return {SlotIssue::Corrupt,{}};
}
SlotIssue SaveSlot::Prepare(std::string payload)
{
    const auto loaded=Load();
    if(loaded.Issue!=SlotIssue::None && loaded.Issue!=SlotIssue::Empty) return loaded.Issue;
    const auto generation=loaded.Value?loaded.Value->Generation:0;
    if(generation==std::numeric_limits<std::uint64_t>::max()) return SlotIssue::GenerationOverflow;
    const auto bytes=EncodeSave({kind_,generation+1,std::move(payload)});
    if(!bytes) return SlotIssue::Invalid;
    pending_=*bytes;return SlotIssue::None;
}
SlotIssue SaveSlot::Retry()
{
    if(!pending_) return SlotIssue::NoPending;
    if(!files_.Replace(kind_,true,*pending_) || !files_.Replace(kind_,false,*pending_)) return SlotIssue::WriteFailed;
    pending_.reset();return SlotIssue::None;
}
}
