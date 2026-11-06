#pragma once
#include "Infrastructure/SavePayload.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include <sstream>

namespace {
struct CampaignProgress {
    int Days=0,Battles=0,MidBosses=0,FinalBosses=0,PlayerActions=0;
    std::uint64_t Generation=0;
};
struct CampaignTransfer {
    GirlsSpire::RunSaveState State;
    GirlsSpire::Profile Permanent;
    CampaignProgress Progress;
};
bool WriteCampaignTransfer(const FString& root,const CampaignTransfer& value)
{
    const auto run=GirlsSpire::EncodeRunPayload(value.State),profile=GirlsSpire::EncodeProfilePayload(value.Permanent);
    if(!run || !profile)return false;
    const auto write=[&](const TCHAR* name,const std::string& bytes){
        if(bytes.size()>MAX_int32)return false;
        TArray<uint8> buffer;buffer.Append(reinterpret_cast<const uint8*>(bytes.data()),static_cast<int32>(bytes.size()));
        return FFileHelper::SaveArrayToFile(buffer,*(root/name));
    };
    const auto& p=value.Progress;
    const auto text=FString::Printf(TEXT("%d %d %d %d %d %llu"),p.Days,p.Battles,p.MidBosses,p.FinalBosses,p.PlayerActions,static_cast<unsigned long long>(p.Generation));
    return write(TEXT("expected-run.bin"),*run) && write(TEXT("expected-profile.bin"),*profile) &&
        FFileHelper::SaveStringToFile(text,*(root/TEXT("progress.txt")));
}
std::optional<CampaignTransfer> ReadCampaignTransfer(const FString& root)
{
    TArray<uint8> runBytes,profileBytes;FString text;
    if(!FFileHelper::LoadFileToArray(runBytes,*(root/TEXT("expected-run.bin"))) ||
       !FFileHelper::LoadFileToArray(profileBytes,*(root/TEXT("expected-profile.bin"))) ||
       !FFileHelper::LoadFileToString(text,*(root/TEXT("progress.txt"))) || runBytes.IsEmpty() || profileBytes.IsEmpty())return {};
    const auto run=GirlsSpire::DecodeRunPayload(std::string_view(reinterpret_cast<const char*>(runBytes.GetData()),runBytes.Num()));
    const auto profile=GirlsSpire::DecodeProfilePayload(std::string_view(reinterpret_cast<const char*>(profileBytes.GetData()),profileBytes.Num()));
    CampaignProgress p;std::istringstream input(TCHAR_TO_UTF8(*text));
    input>>p.Days>>p.Battles>>p.MidBosses>>p.FinalBosses>>p.PlayerActions>>p.Generation;
    if(!input || !run || !profile || p.Generation==0 || p.Days<1 || p.Days>4095 ||
       p.Battles<1 || p.Battles>24 || p.MidBosses<0 || p.MidBosses>1 || p.FinalBosses!=0 || p.PlayerActions<1 || p.PlayerActions>4096)return {};
    input>>std::ws;if(!input.eof())return {};
    return CampaignTransfer{*run,*profile,p};
}
}
