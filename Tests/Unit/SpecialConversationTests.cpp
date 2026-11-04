#include "Application/Run.h"
#include "Infrastructure/SavePayload.h"
#include <algorithm>
#include <iostream>
using namespace GirlsSpire;
int TestSpecialConversation()
{
    int checks=0,failed=0,events=0;std::array<bool,8> met{};
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL SpecialConversation: "<<why<<'\n';}};
    for(int seed=0;seed<100;++seed) {
        Profile profile;profile.Bonds.fill(seed%2==0?80:79);
        Run run(seed,Difficulty::Beginner,HeroRole::Sword,profile);
        for(int step=0;step<24 && run.Snapshot().Phase==RunPhase::Route;++step) {
            check(!run.CanSpecialConverse(1) && !run.SpecialConverse(run.Snapshot().Revision,1),"経路画面では不可");
            int node=0;
            for(int i=0;i<static_cast<int>(run.Snapshot().Choices.size());++i)
                if(run.Snapshot().Choices[i].Kind==NodeKind::Social)node=i;
            run.ChooseNode(run.Snapshot().Revision,node);
            const auto before=run.Checkpoint();const auto permanent=run.ProfileState();
            for(int id=-1;id<=9;++id) {
                const bool member=id>=1 && id<=8 && std::find(before.State.Recruited.begin(),before.State.Recruited.end(),id-1)!=before.State.Recruited.end();
                const bool eligible=before.State.Active.Kind==NodeKind::Social && member && permanent.Bonds[id-1]>=80;
                check(run.CanSpecialConverse(id)==eligible,"加入と絆80の境界");
                auto preview=run;
                check(!preview.SpecialConverse(before.State.Revision+1,id) && preview.Checkpoint()==before && preview.ProfileState()==permanent,"古い世代は全状態不変");
                const bool accepted=preview.SpecialConverse(before.State.Revision,id);
                check(accepted==eligible,"確定の合法性と一致");
                if(eligible && accepted) {
                    ++events;met[id-1]=true;
                    auto expected=permanent;expected.SpecialConversations[id-1]=true;
                    auto elapsed=run;elapsed.CompleteEvent(before.State.Revision);
                    check(preview.ProfileState()==expected && preview.Checkpoint()==elapsed.Checkpoint(),"人物の完了だけ記録しイベント1回を消費");
                    const auto after=preview.Checkpoint();
                    check(!preview.SpecialConverse(before.State.Revision,id) && preview.Checkpoint()==after,"確定の二重実行拒否");
                    Run next(seed,Difficulty::Advanced,HeroRole::Matchlock,preview.ProfileState());
                    check(next.ProfileState()==expected,"次周も完了を保持");
                    auto revisited=Run::Restore(before,expected);
                    check(revisited && !revisited->CanSpecialConverse(id) && !revisited->SpecialConverse(before.State.Revision,id),"完了済みを再確定しない");
                } else check(preview.Checkpoint()==before && preview.ProfileState()==permanent,"拒否で進行も恒久も不変");
                check(run.Checkpoint()==before && run.ProfileState()==permanent,"予告は元状態を変更しない");
            }
            run.CompleteEvent(run.Snapshot().Revision);
        }
    }
    check(events>100,"多数の交流イベントで確定");for(bool value:met)check(value,"8人全員の専用交流を検証");
    for(int mask=0;mask<256;++mask) {
        Profile profile;for(int i=0;i<8;++i){profile.Bonds[i]=(mask+i)%101;profile.SpecialConversations[i]=(mask&(1<<i))!=0;}
        const auto bytes=EncodeProfilePayload(profile);
        check(bytes && DecodeProfilePayload(*bytes)==profile,"全完了パターンを保存往復");
    }
    std::string legacy;const auto append=[&](std::uint32_t value){for(int i=0;i<4;++i)legacy.push_back(static_cast<char>((value>>(8*i))&255));};
    append(2);append(1);Profile expected;expected.HighestDifficulty=2;
    for(int i=0;i<8;++i){expected.Bonds[i]=i*13;append(expected.Bonds[i]);}
    check(InspectPayload(legacy,SaveKind::Profile)==SaveError::None && DecodeProfilePayload(legacy)==expected,"v1の絆を維持し完了は未完として移行");
    check(!DecodeProfilePayload(legacy+"x"),"v1の余剰を受理しない");
    std::cout<<"SpecialConversation: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
