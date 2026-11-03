#include "RecoverySelection.h"
#include "SavePayload.h"
#include <algorithm>
#include <limits>
namespace GirlsSpire {
RecoverySelection SelectRecoveryPair(const std::array<SlotBytes,4>& files)
{
    std::array<std::optional<SaveEnvelope>,4> decoded;
    std::uint64_t highest=0;
    for(std::size_t i=0;i<files.size();++i){
        if(files[i].Failed)return {SlotIssue::ReadFailed,{}};
        if(!files[i].Bytes)continue;
        const auto kind=i<2?SaveKind::Run:SaveKind::Profile;
        const auto value=DecodeSave(*files[i].Bytes,kind);
        if(value.Error==SaveError::UnsupportedVersion)return {SlotIssue::UnsupportedVersion,{}};
        decoded[i]=value.Value;
        if(value.Value){
            highest=std::max(highest,value.Value->Generation);
            if(InspectPayload(value.Value->Payload,kind)==SaveError::UnsupportedVersion)
                return {SlotIssue::UnsupportedVersion,{}};
        }
    }
    // 正常な候補が別にあっても、同世代の内容衝突は推測で解決しない。
    for(std::size_t i:{0u,2u})
        if(decoded[i] && decoded[i+1] && decoded[i]->Generation==decoded[i+1]->Generation && decoded[i]->Payload!=decoded[i+1]->Payload)
            return {};
    for(std::size_t i=0;i<decoded.size();++i){
        if(!decoded[i])continue;
        const auto& payload=decoded[i]->Payload;
        const bool valid=i<2?(payload.empty() || DecodeRunPayload(payload).has_value()):DecodeProfilePayload(payload).has_value();
        if(!valid)decoded[i].reset();
    }
    if(highest==std::numeric_limits<std::uint64_t>::max())return {SlotIssue::GenerationOverflow,{}};
    std::optional<RecoveryPair> pair;
    for(std::size_t r=0;r<2;++r)for(std::size_t p=2;p<4;++p)
        if(decoded[r] && decoded[p] && decoded[r]->Generation==decoded[p]->Generation &&
            (!pair || decoded[r]->Generation>pair->Run.Generation))
            pair=RecoveryPair{*decoded[r],*decoded[p],highest};
    return {pair?SlotIssue::None:SlotIssue::Corrupt,std::move(pair)};
}
}
