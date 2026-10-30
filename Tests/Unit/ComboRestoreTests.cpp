#include "Domain/Combo.h"
#include <iostream>
#include <limits>
using namespace GirlsSpire;
int TestComboRestore()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL ComboRestore: "<<why<<'\n';}};
    const auto restore=[&](const ComboLedger& original) {
        ComboLedger result;
        result.Observe({999,20,99});
        check(result.Restore(original.Snapshot()),"検証済み履歴の復元");
        check(result.Snapshot()==original.Snapshot(),"上書き復元で全項目一致");
        return result;
    };
    ComboLedger original;
    restore(original);
    for(int round=1;round<=20;++round) {
        original.AdvanceRound(round);
        restore(original);
        for(int actor=0;actor<5;++actor) {
            const auto sequence=original.Snapshot().LastSequence+1;
            original.Observe({sequence,round,actor,Faction::Player,true,{},Opening|Continuation,{100,101}});
            auto resumed=restore(original);
            const ComboEvent enemy{sequence+1,round,100,Faction::Enemy,true};
            check(original.Observe(enemy)==resumed.Observe(enemy),"敵割込み結果一致");
            const std::vector<ComboCandidate> candidates={{1,{sequence-2,sequence}}};
            check(original.Resolve(sequence+1,candidates)==resumed.Resolve(sequence+1,candidates),"継続時の成立結果一致");
            check(original.Snapshot()==resumed.Snapshot(),"継続後も全状態一致");
            resumed=restore(original);
            check(!resumed.Resolve(original.Snapshot().ResolvedSequence,candidates),"成立済み番号再適用拒否");
            check(original.Spend(5)==resumed.Spend(5),"ゲージ消費一致");
            check(original.Snapshot()==resumed.Snapshot(),"消費後一致");
        }
    }
    ComboLedger target;target.Observe({1,1,0});const auto before=target.Snapshot();
    const auto reject=[&](ComboSnapshot bad) {
        check(!target.Restore(bad),"不正状態拒否");
        check(target.Snapshot()==before,"拒否時に既存状態維持");
    };
    for(int bad:{-1,0}) {auto s=before;s.Round=bad;reject(s);}
    for(int bad:{-1,101,std::numeric_limits<int>::max()}) {auto s=before;s.Gauge=bad;reject(s);}
    {auto s=before;s.ResolvedSequence=2;reject(s);}
    {auto s=before;s.Pending[0].Sequence=0;reject(s);}
    {auto s=before;s.Pending[0].Sequence=2;reject(s);}
    {auto s=before;s.Pending.push_back(s.Pending[0]);reject(s);}
    {auto s=before;s.LastSequence=3;s.Pending={{2,0},{1,1}};reject(s);}
    for(int bad:{-1,10000}) {auto s=before;s.Pending[0].ActorId=bad;reject(s);}
    for(int gauge:{0,100}) {
        ComboSnapshot boundary{std::numeric_limits<int>::max(),gauge,std::numeric_limits<std::uint64_t>::max(),0,{{1,9999},{2,9999}}};
        check(target.Restore(boundary) && target.Snapshot()==boundary,"有効境界と同一人物の複数履歴");
    }
    // 消費されなかった古い履歴は解決済み番号より前でも保持する。
    ComboLedger partial;
    for(int id=0;id<3;++id) partial.Observe({static_cast<std::uint64_t>(id+1),1,id});
    partial.Resolve(3,{{1,{2,3}}});
    auto resumed=restore(partial);
    check(resumed.Snapshot().Pending==std::vector<ComboAction>{{1,0}},"未消費の古い履歴を保持");
    check(!resumed.Resolve(3,{{2,{1,2}}}),"復元直後の二重解決拒否");
    resumed.AdvanceRound(2);partial.AdvanceRound(2);
    check(resumed.Snapshot()==partial.Snapshot(),"復元後の期限切れ一致");
    std::cout<<"ComboRestore: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
