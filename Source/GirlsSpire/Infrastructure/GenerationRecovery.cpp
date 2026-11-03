#include "SaveTransaction.h"
#include <algorithm>
#include <limits>
namespace GirlsSpire {
namespace {
bool SameRecovery(const GenerationRecovery& a,const GenerationRecovery& b)
{
    return a.Originals==b.Originals && a.Journal==b.Journal && a.Candidate.Run==b.Candidate.Run &&
        a.Candidate.Profile==b.Candidate.Profile && a.Candidate.HighestGeneration==b.Candidate.HighestGeneration;
}
}
GenerationRecoveryPreview SaveTransaction::PreviewGenerationRecovery()
{
    if(pending_)return {SlotIssue::Pending,{}};
    const auto journal=journal_.ReadJournal();
    if(journal.Failed)return {SlotIssue::ReadFailed,{}};
    std::uint64_t journalGeneration=0;
    if(journal.Bytes){
        const auto frame=DecodeSave(*journal.Bytes,SaveKind::Run);
        if(frame.Error==SaveError::UnsupportedVersion)return {SlotIssue::UnsupportedVersion,{}};
        if(frame.Value){
            journalGeneration=frame.Value->Generation;
            if(!frame.Value->Payload.empty()){
                SlotIssue issue=SlotIssue::Corrupt;
                if(DecodeJournal(*journal.Bytes,issue))return {};
                if(issue==SlotIssue::UnsupportedVersion)return {issue,{}};
            }
        }
    }
    std::array<SlotBytes,4> files;
    for(std::size_t i=0;i<files.size();++i)files[i]=slots_.Read(i<2?SaveKind::Run:SaveKind::Profile,(i%2)!=0);
    auto selected=SelectRecoveryPair(files);
    if(!selected.Pair)return {selected.Issue,{}};
    std::array<std::uint64_t,2> latest{};
    for(std::size_t i=0;i<files.size();++i)if(files[i].Bytes){
        const auto value=DecodeSave(*files[i].Bytes,i<2?SaveKind::Run:SaveKind::Profile);
        if(value.Value)latest[i/2]=std::max(latest[i/2],value.Value->Generation);
    }
    // 同世代の修復と有効な取引の再生は、既存経路に委ねる。
    if(latest[0]==latest[1])return {};
    selected.Pair->HighestGeneration=std::max(selected.Pair->HighestGeneration,journalGeneration);
    if(selected.Pair->HighestGeneration==std::numeric_limits<std::uint64_t>::max())return {SlotIssue::GenerationOverflow,{}};
    GenerationRecovery recovery{*selected.Pair,{},journal.Bytes};
    for(std::size_t i=0;i<files.size();++i)recovery.Originals[i]=std::move(files[i].Bytes);
    return {SlotIssue::None,std::move(recovery)};
}
SlotIssue SaveTransaction::PrepareGenerationRecovery(const GenerationRecovery& confirmed)
{
    const auto current=PreviewGenerationRecovery();
    if(!current.Value)return current.Issue;
    if(!SameRecovery(*current.Value,confirmed))return SlotIssue::Invalid;
    const auto generation=confirmed.Candidate.HighestGeneration+1;
    const auto commit=MakeCommit(*EncodeSave({SaveKind::Run,generation,confirmed.Candidate.Run.Payload}),
        *EncodeSave({SaveKind::Profile,generation,confirmed.Candidate.Profile.Payload}),generation);
    if(!commit)return SlotIssue::Invalid;
    if(!slots_.Preserve(SaveKind::Run) || !slots_.Preserve(SaveKind::Profile) || !journal_.PreserveJournal())return SlotIssue::WriteFailed;
    // 確認後だけでなく、原本の保全中に外部変更された場合も停止する。
    const auto preserved=PreviewGenerationRecovery();
    if(!preserved.Value || !SameRecovery(*preserved.Value,confirmed))return SlotIssue::Invalid;
    pending_=*commit;return SlotIssue::None;
}
}
