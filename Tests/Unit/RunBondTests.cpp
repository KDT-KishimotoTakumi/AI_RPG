#include "Application/Run.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace GirlsSpire;
int TestRunBond()
{
    int checks=0,failed=0,visits=0;std::array<bool,8> met{};
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL RunBond: "<<why<<'\n';}};
    for(int bad:{-1,101}) {
        Profile p;p.Bonds[7]=bad;bool rejected=false;
        try {Run run(0,Difficulty::Beginner,HeroRole::Sword,p);} catch(const std::invalid_argument&) {rejected=true;}
        check(rejected,"不正な持越しを拒否");
    }
    for(int seed=0;seed<100;++seed) {
        Profile p;p.Bonds.fill(seed%101);
        Run run(seed,Difficulty::Beginner,HeroRole::Sword,p);
        check(run.ProfileState()==p,"周回初期化で保持");
        for(int step=0;step<24 && run.Snapshot().Phase==RunPhase::Route;++step) {
            auto before=run.Snapshot();auto profile=run.ProfileState();auto random=run.RandomState();
            check(!run.Converse(before.Revision,1,ConversationChoice::Warm) && run.Snapshot()==before && run.ProfileState()==profile && run.RandomState()==random,"経路中拒否");
            int node=0;
            for(int i=0;i<static_cast<int>(before.Choices.size());++i) if(before.Choices[i].Kind==NodeKind::Social) node=i;
            run.ChooseNode(before.Revision,node);
            before=run.Snapshot();profile=run.ProfileState();random=run.RandomState();
            const bool social=before.Active.Kind==NodeKind::Social;
            for(int id=-1;id<=9;++id) {
                const bool member=id>0 && std::find(before.Recruited.begin(),before.Recruited.end(),id-1)!=before.Recruited.end();
                if(social && member) continue;
                check(!run.Converse(before.Revision,id,ConversationChoice::Warm) && run.Snapshot()==before && run.ProfileState()==profile && run.RandomState()==random,"対象・イベント不正で全状態不変");
            }
            if(social) {
                ++visits;const int id=before.Recruited.front()+1;met[id-1]=true;
                for(int rank:{0,1,24,25,49,50,74,75,95,99,100}) for(auto choice:{ConversationChoice::Warm,ConversationChoice::Neutral,ConversationChoice::Cold}) {
                    // 各枝は同じイベント状態から複製し、予告の非破壊性も確認する。
                    auto fork=run;auto expected=profile;ApplyConversation(expected,id,choice);
                    check(!fork.Converse(before.Revision+1,id,choice) && fork.Snapshot()==before && fork.ProfileState()==profile,"古い世代拒否");
                    check(!fork.Converse(before.Revision,id,static_cast<ConversationChoice>(rank+3)) && fork.ProfileState()==profile,"未知の選択拒否");
                    auto progression=run;progression.CompleteEvent(before.Revision);
                    check(fork.Converse(before.Revision,id,choice) && fork.ProfileState()==expected && fork.Snapshot()==progression.Snapshot() && fork.RandomState()==progression.RandomState(),"会話1回でイベント終了");
                    const auto after=fork.Snapshot();const auto afterRandom=fork.RandomState();
                    check(!fork.Converse(before.Revision,id,choice) && fork.Snapshot()==after && fork.ProfileState()==expected && fork.RandomState()==afterRandom,"二重会話拒否");
                }
                check(run.Snapshot()==before && run.ProfileState()==profile && run.RandomState()==random,"予告コピーは本体不変");
                check(run.Converse(before.Revision,id,static_cast<ConversationChoice>(seed%3)),"実会話");
                // 敗北枝でも恒久部分は残し、次周の財布などは初期化する。
                auto lost=run;if(lost.Snapshot().Phase==RunPhase::Route) {
                    lost.ChooseNode(lost.Snapshot().Revision,0);lost.CompleteEvent(lost.Snapshot().Revision,true);
                    check(lost.ProfileState()==run.ProfileState(),"敗北でも絆保持");
                    Run next(seed,Difficulty::Advanced,HeroRole::Matchlock,lost.ProfileState());
                    check(next.ProfileState()==run.ProfileState() && next.Snapshot().Wallet==Purse{} && next.Snapshot().Recruited.empty(),"次周は絆のみ持越し");
                }
            } else run.CompleteEvent(before.Revision);
        }
        const auto retained=run.ProfileState();
        check(run.Snapshot().Phase==RunPhase::Won,"12日完了");
        Run next(seed,Difficulty::Beginner,HeroRole::Sword,retained);
        check(next.ProfileState()==retained,"勝利後の次周保持");
    }
    check(visits>=100,"100シードで交流に到達");
    for(bool value:met) check(value,"8人全員の交流");
    std::cout<<"RunBond: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
