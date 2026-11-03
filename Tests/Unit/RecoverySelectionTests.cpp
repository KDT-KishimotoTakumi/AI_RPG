#include "Infrastructure/RecoverySelection.h"
#include "Infrastructure/SavePayload.h"
#include <iostream>
#include <algorithm>
#include <limits>

int TestRecoverySelection()
{
    using namespace GirlsSpire;
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL RecoverySelection: "<<why<<'\n';}};
    Run run(42,Difficulty::Beginner,HeroRole::Sword);const RunSaveState state{run.Checkpoint(),{}};
    Profile profile;profile.Bonds.fill(80);profile.SpecialConversations.fill(true);profile.Epilogues.fill(true);
    const auto rp=*EncodeRunPayload(state),pp=*EncodeProfilePayload(profile);
    const auto slot=[&](int index,std::uint64_t generation,std::string payload){return SlotBytes{false,EncodeSave({index<2?SaveKind::Run:SaveKind::Profile,generation,std::move(payload)})};};
    const auto runAt=[&](int generation){Run r(100+generation,Difficulty::Beginner,HeroRole::Sword,profile);return *EncodeRunPayload(RunSaveState{r.Checkpoint(),{}});};
    const auto profileAt=[&](int generation){auto p=profile;p.Bonds.fill(generation*10);return *EncodeProfilePayload(p);};
    for(int a=1;a<=4;++a)for(int b=1;b<=4;++b)for(int c=1;c<=4;++c)for(int d=1;d<=4;++d){
        const std::array<SlotBytes,4> files={slot(0,a,runAt(a)),slot(1,b,runAt(b)),slot(2,c,profileAt(c)),slot(3,d,profileAt(d))};
        const auto selected=SelectRecoveryPair(files);
        int common=0;for(int r:{a,b})for(int p:{c,d})if(r==p && r>common)common=r;
        check(selected.Issue==(common?SlotIssue::None:SlotIssue::Corrupt) && bool(selected.Pair)==(common!=0),"全256世代組合せの成立判定");
        if(selected.Pair){
            check(selected.Pair->Run.Generation==static_cast<std::uint64_t>(common) && selected.Pair->Profile.Generation==static_cast<std::uint64_t>(common),"最新の共通世代だけを使用");
            check(selected.Pair->Run.Payload==runAt(common) && selected.Pair->Profile.Payload==profileAt(common),"周回・恒久の内容を維持");
            check(selected.Pair->HighestGeneration==static_cast<std::uint64_t>(std::max({a,b,c,d})),"新取引用に片側の最新世代も保持");
        }
    }
    const std::array<SlotBytes,4> base={slot(0,3,rp),slot(1,2,rp),slot(2,2,pp),slot(3,1,pp)};
    for(int i=0;i<4;++i){
        auto files=base;files[i].Failed=true;auto result=SelectRecoveryPair(files);
        check(result.Issue==SlotIssue::ReadFailed && !result.Pair,"読込失敗は全位置で停止");
        files=base;auto future=*(i<2?EncodeRunPayload(state):EncodeProfilePayload(profile));future[4]=99;files[i]=slot(i,i<2?3:2,future);
        result=SelectRecoveryPair(files);check(result.Issue==SlotIssue::UnsupportedVersion && !result.Pair,"内部未来版は候補外でも上書き不可");
        files=base;auto envelope=*files[i].Bytes;envelope[4]=99;
        std::uint64_t checksum=14695981039346656037ULL;
        for(std::size_t n=0;n<envelope.size()-8;++n){checksum^=static_cast<unsigned char>(envelope[n]);checksum*=1099511628211ULL;}
        for(int n=0;n<8;++n)envelope[envelope.size()-8+n]=static_cast<char>((checksum>>(n*8))&255);
        files[i].Bytes=envelope;
        result=SelectRecoveryPair(files);check(result.Issue==SlotIssue::UnsupportedVersion && !result.Pair,"包絡未来版は全位置で停止");
    }
    for(int side=0;side<2;++side){
        auto files=base;const int first=side*2;
        files[first]=slot(first,2,side?pp:rp);files[first+1]=slot(first+1,2,"different");
        const auto result=SelectRecoveryPair(files);check(result.Issue==SlotIssue::Corrupt && !result.Pair,"同種・同世代の内容衝突は停止");
    }
    auto files=base;files[0].Bytes="broken";files[3].Bytes.reset();
    auto result=SelectRecoveryPair(files);check(result.Issue==SlotIssue::None && result.Pair && result.Pair->Run.Generation==2,"壊れた片側と不在を除き共通世代を選ぶ");
    files=base;files[0]=slot(0,4,"bad payload");
    result=SelectRecoveryPair(files);check(result.Pair && result.Pair->Run.Generation==2 && result.Pair->HighestGeneration==4,"不正内部値は使わず包絡の最新世代を保持");
    files=base;files[1]=slot(1,2,"");result=SelectRecoveryPair(files);check(result.Pair && result.Pair->Run.Payload.empty(),"完了済みの空周回も正常候補");
    files=base;files[2]=slot(2,2,"");result=SelectRecoveryPair(files);check(!result.Pair,"空恒久は復旧候補にしない");
    files=base;files[0]=slot(0,std::numeric_limits<std::uint64_t>::max(),rp);
    result=SelectRecoveryPair(files);check(result.Issue==SlotIssue::GenerationOverflow && !result.Pair,"世代上限は共通世代が低くても停止");
    result=SelectRecoveryPair({});check(result.Issue==SlotIssue::Corrupt && !result.Pair,"全不在から自動初期化しない");
    check(base[0].Bytes==slot(0,3,rp).Bytes,"候補選択は入力を変更しない");
    std::cout<<"RecoverySelection: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
