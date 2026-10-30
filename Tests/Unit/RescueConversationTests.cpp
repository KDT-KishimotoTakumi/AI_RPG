#include "Application/Encounter.h"
#include "Infrastructure/SavePayload.h"
#include <iostream>
using namespace GirlsSpire;
int TestRescueConversation()
{
    int checks=0,failed=0;std::array<bool,8> met{};
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL RescueConversation: "<<why<<'\n';}};
    for(int seed=0;seed<100;++seed)for(int rank:{0,1,5,95,100}) {
        Profile profile;profile.Bonds.fill(rank);Run run(seed,Difficulty::Beginner,HeroRole::Sword,profile);
        check(!run.ConverseRescue(0,ConversationChoice::Warm),"救出前の会話を拒否");
        run.ChooseNode(0,0);const auto entry=run.Checkpoint();const int candidate=entry.State.Active.Candidate;met[candidate]=true;
        auto battle=Encounter::Create(entry.State,run.RandomState()).Snapshot();battle.Outcome=BattleOutcome::Won;
        check(run.CompleteBattle(entry.State.Revision,battle),"実戦の救出成功を反映");
        check(run.Snapshot().PendingRescue==candidate,"救出した人物への返答を一度要求");
        for(int stage=0;stage<2;++stage)for(auto choice:{ConversationChoice::Warm,ConversationChoice::Neutral,ConversationChoice::Cold}) {
            auto fork=run;if(stage==1){fork.ChooseRecipient(fork.Snapshot().Revision,0);fork.ClaimReward(fork.Snapshot().Revision,0);}
            const auto before=fork.Checkpoint();const auto random=fork.RandomState();
            auto blocked=fork;check(!blocked.ChooseNode(before.State.Revision,0) && blocked.Checkpoint()==before,"未返答で次の経路へ進まない");
            check(!fork.ConverseRescue(before.State.Revision+1,choice) && fork.Checkpoint()==before,"古い確定で進行不変");
            check(!fork.ConverseRescue(before.State.Revision,static_cast<ConversationChoice>(3)) && fork.Checkpoint()==before && fork.ProfileState()==profile,"不正返答で全状態不変");
            check(fork.ConverseRescue(before.State.Revision,choice),"救出後の返答を確定");
            auto expected=before;expected.State.PendingRescue=-1;++expected.State.Revision;
            auto permanent=profile;ApplyConversation(permanent,candidate+1,choice);
            check(fork.Checkpoint()==expected && fork.ProfileState()==permanent && fork.RandomState()==random,"絆だけ反映し追加時間・乱数消費なし");
            check(!fork.ConverseRescue(fork.Snapshot().Revision,choice) && fork.Checkpoint()==expected && fork.ProfileState()==permanent,"同じ救出で二度加算しない");
            const auto encoded=EncodeRunPayload({before,{}});
            check(encoded && DecodeRunPayload(*encoded)==RunSaveState{before,{}},"未返答の状態を保存往復");
        }
        auto wrong=run.Checkpoint();wrong.State.PendingRescue=(candidate+1)%8;
        check(!Run::Restore(wrong,profile),"異なる救出対象を復元しない");
        for(int invalid:{-2,8}) {
            auto invalidTarget=run.Checkpoint();invalidTarget.State.PendingRescue=invalid;
            check(!Run::Restore(invalidTarget,profile),"範囲外の未返答人物を拒否");
        }
        auto unjoined=run.Checkpoint();unjoined.State.PendingRescue=(candidate+1)%8;unjoined.State.Active.Candidate=unjoined.State.PendingRescue;
        check(!Run::Restore(unjoined,profile),"加入していない救出対象への返答を拒否");
        auto wrongEnd=run.Checkpoint();wrongEnd.AfterReward=RunPhase::Won;wrongEnd.State.Choices.clear();
        check(!Run::Restore(wrongEnd,profile),"救出返答待ちを最終勝利へ直結する破損を拒否");
        auto lost=Run::Restore(entry,profile);battle.Outcome=BattleOutcome::Lost;
        for(auto& unit:battle.Units)if(unit.Side==Faction::Player)unit.Hp=0;
        check(lost && lost->CompleteBattle(entry.State.Revision,battle) && lost->Snapshot().PendingRescue==-1 && !lost->ConverseRescue(lost->Snapshot().Revision,ConversationChoice::Warm),"敗北では救出返答を発生させない");
    }
    for(bool found:met)check(found,"8人全員の救出返答を検証");
    Run fresh(42,Difficulty::Beginner,HeroRole::Sword);const RunSaveState saved{fresh.Checkpoint(),{}};
    const auto encoded=EncodeRunPayload(saved);check(RunPayloadSchema>=3,"周回形式v3以降の未返答保存を維持");
    if(encoded) {
        auto legacy=*encoded;
        // 末尾は乱数8・報酬後段階4・戦闘有無1・再開可否1。直前の未返答人物4を除く。
        if constexpr(RunPayloadSchema>=3)legacy.erase(legacy.size()-18,4);
        legacy[4]=2;
        check(InspectPayload(legacy,SaveKind::Run)==SaveError::None && DecodeRunPayload(legacy)==saved,"v2は未返答なしとして状態を維持");
    }
    std::cout<<"RescueConversation: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
