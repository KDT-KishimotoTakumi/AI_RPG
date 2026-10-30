#include "Application/Run.h"
#include "Infrastructure/SavePayload.h"
#include <algorithm>
#include <iostream>
using namespace GirlsSpire;
int TestEpilogue()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL Epilogue: "<<why<<'\n';}};
    for(int seed=0;seed<20;++seed) {
        Run run(seed,Difficulty::Beginner,HeroRole::Sword);
        for(int step=0;step<24 && run.Snapshot().Phase==RunPhase::Route;++step) {
            check(run.EpilogueChoices().empty() && !run.ProfileForEpilogue(0),"勝利前は単独も選択不可");
            run.ChooseNode(run.Snapshot().Revision,0);run.CompleteEvent(run.Snapshot().Revision);
        }
        check(run.Snapshot().Phase==RunPhase::Won,"勝利状態を準備");
        for(int rank:{79,80,100})for(int mask=0;mask<256;++mask) {
            Profile profile;profile.Bonds.fill(rank);
            for(int i=0;i<8;++i)profile.SpecialConversations[i]=(mask&(1<<i))!=0;
            const auto won=Run::Restore(run.Checkpoint(),profile);
            if(!won){check(false,"勝利状態の復元");continue;}
            std::vector<int> expected{0};
            for(int id=1;id<=8;++id)
                if(rank>=80 && profile.SpecialConversations[id-1] && std::find(run.Snapshot().Recruited.begin(),run.Snapshot().Recruited.end(),id-1)!=run.Snapshot().Recruited.end())expected.push_back(id);
            check(won->EpilogueChoices()==expected,"全達成者と単独を候補に含める");
            for(int id=-1;id<=9;++id) {
                const auto recorded=won->ProfileForEpilogue(id);
                const bool valid=std::find(expected.begin(),expected.end(),id)!=expected.end();
                check(recorded.has_value()==valid,"不正/未達成の選択を拒否");
                if(recorded) {
                    auto changed=profile;changed.Epilogues[id]=true;
                    check(*recorded==changed,"選択した後日談だけを記録");
                }
                check(won->ProfileState()==profile && won->Checkpoint()==run.Checkpoint(),"選択予告は元状態不変");
            }
        }
    }
    Profile all;all.Bonds.fill(100);all.SpecialConversations.fill(true);all.Epilogues.fill(true);
    const auto bytes=EncodeProfilePayload(all);
    check(bytes && DecodeProfilePayload(*bytes)==all,"全後日談の保存往復");
    if(bytes)for(std::size_t offset=48;offset<bytes->size();++offset) {
        auto broken=*bytes;broken[offset]=2;check(!DecodeProfilePayload(broken),"不正な後日談boolを拒否");
    }
    for(int mask=0;mask<512;++mask) {
        auto profile=all;for(int i=0;i<9;++i)profile.Epilogues[i]=(mask&(1<<i))!=0;
        const auto encoded=EncodeProfilePayload(profile);check(encoded && DecodeProfilePayload(*encoded)==profile,"全記録パターンの保存往復");
    }
    std::string v2;const auto append=[&](std::uint32_t value){for(int i=0;i<4;++i)v2.push_back(static_cast<char>((value>>(8*i))&255));};
    append(2);append(2);for(int i=0;i<8;++i)append(80);
    for(int i=0;i<8;++i)v2.push_back(i%2?1:0);
    Profile expected;expected.Bonds.fill(80);for(int i=0;i<8;++i)expected.SpecialConversations[i]=i%2!=0;
    check(InspectPayload(v2,SaveKind::Profile)==SaveError::None && DecodeProfilePayload(v2)==expected,"v2の絆/専用交流を保持し後日談を未読で移行");
    std::cout<<"Epilogue: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
