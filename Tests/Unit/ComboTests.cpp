#include "Domain/Combo.h"
#include <algorithm>
#include <iostream>

int TestCombo()
{
    using namespace GirlsSpire;
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* message) { ++checks;if(!ok){++failed;std::cerr<<"FAIL Combo: "<<message<<'\n';} };
    const auto fill=[&](int count) {
        ComboLedger ledger;
        for(int actor=0;actor<count;++actor) check(ledger.Observe({static_cast<std::uint64_t>(actor+1),1,actor}),"味方行動を記録");
        return ledger;
    };
    ComboLedger ledger;
    check(ledger.Snapshot()==ComboSnapshot{},"初期状態");
    check(!ledger.Resolve(0,{{1,{1,2}}}),"空履歴では不成立");
    ledger=fill(1);
    check(ledger.Observe({2,1,100,Faction::Enemy,false}),"敵の外れを観測");
    check(ledger.Snapshot().Pending==std::vector<ComboAction>{{1,0}},"敵の割込みで継続");
    check(ledger.Observe({3,1,200,Faction::Neutral}),"中立の行動を観測");
    check(ledger.Observe({4,1,1}),"割込み後の味方行動");
    auto resolution=ledger.Resolve(4,{{1,{1,4}}});
    check(resolution && *resolution==ComboResolution{1,{0,1},10},"割込みを挟んだ2人連携");
    check(ledger.Snapshot().Gauge==10 && ledger.Snapshot().Pending.empty(),"加算と履歴消費");
    const auto resolved=ledger.Snapshot();
    check(!ledger.Resolve(4,{{1,{1,4}}}) && ledger.Snapshot()==resolved,"二重加算拒否");

    for(int count:{2,3,5}) {
        ledger=fill(count);
        ComboCandidate candidate{count,{}};
        for(int i=1;i<=count;++i) candidate.Actions.push_back(i);
        resolution=ledger.Resolve(count,{candidate});
        const int gain=count==2?10:count==3?20:40;
        check(resolution && resolution->GaugeGain==gain && ledger.Snapshot().Gauge==gain,"人数別ゲージ");
    }
    ledger=fill(5);
    const std::vector<ComboCandidate> candidates={{30,{1,2}},{20,{1,2,3}},{10,{1,2,3,4,5}}};
    std::vector<int> indices={0,1,2};
    do {
        auto copy=ledger;
        std::vector<ComboCandidate> reordered;
        for(int i:indices) reordered.push_back(candidates[i]);
        resolution=copy.Resolve(5,reordered);
        check(resolution && resolution->RecipeId==10 && copy.Snapshot().Gauge==40,"入力順に関わらず最多人数1つ");
    } while(std::next_permutation(indices.begin(),indices.end()));
    ledger=fill(4);
    resolution=ledger.Resolve(4,{{9,{3,4}},{8,{2,4}},{8,{1,3}}});
    check(resolution && resolution->RecipeId==8 && resolution->Participants==std::vector<int>{0,2},"同人数はIDと行動列で安定選択");
    check(ledger.Snapshot().Pending==std::vector<ComboAction>{{2,1},{4,3}},"未使用履歴を維持");
    check(!ledger.Resolve(4,{{9,{2,4}}}),"同イベントで別連携も二重発動しない");
    ledger.Observe({5,1,100,Faction::Enemy});
    check(!ledger.Resolve(5,{{8,{1,3}}}),"消費した履歴の再利用拒否");

    for(const auto& invalid:std::vector<ComboCandidate>{{0,{1,2}},{-1,{1,2}},{1,{}},{1,{1}},{1,{1,2,3,4}},
        {1,{1,2,3,4,5,6}},{1,{2,1}},{1,{1,1}},{1,{1,99}}}) {
        ledger=fill(5);const auto before=ledger.Snapshot();
        check(!ledger.Resolve(5,{invalid}) && ledger.Snapshot()==before,"不正候補を状態不変で拒否");
    }
    ledger=fill(1);ledger.Observe({2,1,0});const auto duplicate=ledger.Snapshot();
    check(!ledger.Resolve(2,{{1,{1,2}}}) && ledger.Snapshot()==duplicate,"同一人物の複数行動では不成立");
    ledger=fill(2);const auto before=ledger.Snapshot();
    check(!ledger.Resolve(1,{{1,{1,2}}}) && ledger.Snapshot()==before,"古い世代で解決しない");
    check(!ledger.Resolve(3,{{1,{1,2}}}) && ledger.Snapshot()==before,"未来世代で解決しない");
    for(const auto& event:std::vector<ComboEvent>{{0,1,0},{2,1,0},{1,1,0},{3,0,0},{3,1,-1},{3,1,10000},
        {3,1,0,static_cast<Faction>(-1)},{3,1,0,static_cast<Faction>(3)},
        {3,1,0,Faction::Enemy,true,{-1}},{3,1,0,Faction::Enemy,true,{10000}},{3,1,0,Faction::Enemy,true,{1,1}}}) {
        check(!ledger.Observe(event) && ledger.Snapshot()==before,"不正イベントは状態不変");
    }
    ledger.Observe({3,2,2});
    check(ledger.Snapshot().Round==2 && ledger.Snapshot().Pending==std::vector<ComboAction>{{3,2}},"次ラウンドに履歴を持ち越さない");
    const auto nextRound=ledger.Snapshot();
    check(!ledger.Observe({4,1,3}) && ledger.Snapshot()==nextRound,"過去ラウンド拒否");
    ledger=fill(2);ledger.Observe({3,1,2,Faction::Player,false});
    check(ledger.Snapshot().Pending.empty(),"味方の外れで中断");
    ledger=fill(2);ledger.Observe({3,1,100,Faction::Enemy,true,{9}});
    check(ledger.Snapshot().Pending.size()==2,"非参加者の戦闘不能では継続");
    ledger.Observe({4,1,100,Faction::Enemy,true,{0}});
    check(ledger.Snapshot().Pending.empty(),"参加者の戦闘不能で中断");
    ledger=fill(1);ledger.Observe({2,1,1,Faction::Player,true,{1}});
    check(ledger.Snapshot().Pending.empty(),"実行中に倒れた人物の行動も未成立にしない");
    ledger=fill(2);ledger.Resolve(2,{{1,{1,2}}});ledger.Observe({3,2,0,Faction::Player,false,{1}});
    check(ledger.Snapshot().Gauge==10 && ledger.Snapshot().Pending.empty(),"中断しても成立済みゲージを維持");
    ledger=ComboLedger{};
    for(int round=1;round<=12;++round) {
        const auto seq=static_cast<std::uint64_t>(round*2);
        ledger.Observe({seq-1,round,0});ledger.Observe({seq,round,1});
        resolution=ledger.Resolve(seq,{{1,{seq-1,seq}}});
        check(resolution && resolution->GaugeGain==(round<=10?10:0) && ledger.Snapshot().Gauge==std::min(100,round*10),"ゲージ上限と実加算量");
    }
    std::cout<<"Combo: "<<checks<<" checks, "<<failed<<" failed\n";
    return failed;
}
