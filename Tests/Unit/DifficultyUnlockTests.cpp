#include "Application/Run.h"
#include "Infrastructure/SavePayload.h"
#include <iostream>
using namespace GirlsSpire;
int TestDifficultyUnlock()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL DifficultyUnlock: "<<why<<'\n';}};
    const auto rank=[](const Profile& p){return p.HighestDifficulty;};
    check(ProfilePayloadSchema>=5,"解放を持つ恒久保存はv5以降");
    check(rank(Profile{})==0,"新規データは初級だけ");
    for(int version=1;version<=4;++version){
        std::string old;const auto put=[&](int n){for(int i=0;i<4;++i)old.push_back(static_cast<char>((n>>(8*i))&255));};
        put(2);put(version);for(int i=0;i<8;++i)put(0);
        if(version>=2)old.append(8,'\0');if(version>=3)old.append(9,'\0');if(version>=4)put(0);
        const auto migrated=DecodeProfilePayload(old);
        check(InspectPayload(old,SaveKind::Profile)==SaveError::None && migrated && rank(*migrated)==2,"旧v1～4は全解放で移行");
    }
    for(int difficulty=0;difficulty<3;++difficulty)for(bool defeated:{false,true}){
        Run run(42,static_cast<Difficulty>(difficulty),HeroRole::Sword);
        while(run.Snapshot().Phase==RunPhase::Route){
            run.ChooseNode(run.Snapshot().Revision,0);
            const bool final=run.Snapshot().Active.Kind==NodeKind::FinalBoss;
            run.CompleteEvent(run.Snapshot().Revision,final && defeated);
        }
        check(rank(run.ProfileState())==(defeated?0:std::min(2,difficulty+1)),"最終ボス勝利だけ次の段階を恒久解放");
        const auto saved=EncodeProfilePayload(run.ProfileState());
        check(saved && DecodeProfilePayload(*saved)==run.ProfileState(),"解放を保存再読込で維持");
    }
    for(int highest=-1;highest<=3;++highest){
        Profile p;p.HighestDifficulty=highest;
        check(ValidProfile(p)==(highest>=0 && highest<=2),"恒久解放の範囲検査");
        for(int d=-1;d<=3;++d)check(CanStartDifficulty(p,d)==(highest>=0 && highest<=2 && d>=0 && d<=highest),"出発の解放条件を境界で検証");
        check(EncodeProfilePayload(p).has_value()==ValidProfile(p),"不正な解放値は保存拒否");
    }
    Profile unlocked;unlocked.HighestDifficulty=2;Run retained(42,Difficulty::Beginner,HeroRole::Sword,unlocked);
    retained.ChooseNode(retained.Snapshot().Revision,0);retained.CompleteEvent(retained.Snapshot().Revision,true);
    check(retained.ProfileState()==unlocked,"敗北しても既存の解放を維持");
    auto resume=Run::Restore(retained.Checkpoint(),unlocked);
    check(resume && resume->ProfileState()==unlocked,"敗北保存の復元でも維持");
    Run direct(42,Difficulty::Advanced,HeroRole::Sword);
    check(Run::Restore(direct.Checkpoint(),Profile{}).has_value(),"未解放の旧周回も再開可能・直接シミュレーション維持");
    for(int highest=0;highest<3;++highest)for(int d=0;d<3;++d){
        Profile prior;prior.HighestDifficulty=highest;prior.Bonds.fill(42);prior.DiscoveredCombos={1,2};
        Run run(42,static_cast<Difficulty>(d),HeroRole::Sword,prior);
        while(run.Snapshot().Phase==RunPhase::Route){
            run.ChooseNode(run.Snapshot().Revision,0);
            if(run.Snapshot().Active.Kind==NodeKind::FinalBoss)break;
            run.CompleteEvent(run.Snapshot().Revision);
        }
        const auto revision=run.Snapshot().Revision;const auto rng=run.RandomState();
        check(!run.CompleteBattle(revision+1,BattleOutcome::Won) && run.ProfileState()==prior,"古い勝利入力では解放しない");
        check(run.CompleteBattle(revision,BattleOutcome::Won),"実戦結果経路で最終ボス勝利");
        prior.HighestDifficulty=std::max(highest,std::min(2,d+1));prior.VictoryRecorded=true;
        check(run.ProfileState()==prior && run.Snapshot().Phase==RunPhase::Reward,"報酬前に解放し既存の記録は保持");
        check(run.RandomState()==rng,"解放処理は乱数を消費しない");
    }
    Profile oldSource;oldSource.HighestDifficulty=1;oldSource.Bonds.fill(42);oldSource.SpecialConversations[3]=true;oldSource.Epilogues[5]=true;oldSource.DiscoveredCombos={1,2,201};
    if(auto v4=EncodeProfilePayload(oldSource)){
        // v7の外見37バイト＋v6の空の敵一覧4＋v5の難易度4を除く。
        v4->resize(v4->size()-45);(*v4)[4]=4;auto expected=oldSource;expected.HighestDifficulty=2;
        check(DecodeProfilePayload(*v4)==expected,"v4の絆/交流/後日談/発見連携を全て維持して全解放");
        for(std::size_t size=0;size<v4->size();++size)check(!DecodeProfilePayload(v4->substr(0,size)),"切断したv4は受理しない");
        check(!DecodeProfilePayload(*v4+"x"),"余剰のあるv4は受理しない");
    }
    std::cout<<"DifficultyUnlock: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
