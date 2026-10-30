#include "../Fixtures/TeamUltimateBattle.h"
#include <iostream>
int TestTeamUltimate()
{
    using namespace GirlsSpire;
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* name){++checks;if(!ok){++failed;std::cerr<<"FAIL TeamUltimate: "<<name<<'\n';}};
    auto b=Test::TeamUltimateBattle();
    const auto before=b.Snapshot();const auto rng=b.RandomState();
    auto p=b.PreviewTeamUltimate(0,{6,1});
    check(b.CanTeamUltimate(0),"距離4を含む5人生存");
    check(p.Legal && p.Hits.size()==2 && p.Hits[0].Damage==50 && p.Hits[1].TargetId==101,"上下左右範囲・攻撃力40加算");
    check(b.Snapshot()==before && b.RandomState()==rng,"予告不変");
    for(auto at:{Position{-1,1},Position{8,1},Position{0,0},Position{1,1},Position{5,1},Position{7,0}})
        check(!b.UseTeamUltimate(before.Revision,0,at) && b.Snapshot()==before && b.RandomState()==rng,"無効中心は消費なし");
    check(!b.UseTeamUltimate(before.Revision-1,0,{6,1}) && b.Snapshot()==before,"古い入力拒否");
    for(int id:{-1,1,100,9999}) check(!b.CanTeamUltimate(id),"本人の手番のみ");
    for(int variant=1;variant<=4;++variant) {
        b=Test::TeamUltimateBattle(1,100,variant);
        const auto s=b.Snapshot();const auto r=b.RandomState();
        check(!b.CanTeamUltimate(s.ActiveId),"距離5・戦闘不能・主人公なし・4人拒否");
        check(!b.UseTeamUltimate(s.Revision,s.ActiveId,{6,1}) && b.Snapshot()==s && b.RandomState()==r,"条件不足不変");
    }
    b=Test::TeamUltimateBattle(1,90);
    check(!b.CanTeamUltimate(0),"ゲージ不足");
    int allHit=0,partial=0;
    for(int seed=0;seed<100;++seed) {
        b=Test::TeamUltimateBattle(seed);b.EndTurn(b.Snapshot().Revision,0,Facing::South);
        auto copy=b;
        for(auto* battle:{&b,&copy}) check(battle->UseTeamUltimate(battle->Snapshot().Revision,1,{6,1}),"仲間の全員必殺");
        check(b.Snapshot()==copy.Snapshot() && b.RandomState()==copy.RandomState(),"100シード再現");
        if(b.LastHits().size()!=2){check(false,"2対象の結果");continue;}
        const bool success=b.LastHits()[0].Hit && b.LastHits()[1].Hit;
        if(success) ++allHit;else ++partial;
        if((success && allHit==1) || (!success && partial==1)) std::cout<<"TeamUltimate fixture seed "<<seed<<": "<<(success?"all hit":"miss included")<<'\n';
        check(b.Snapshot().Combos.Gauge==(success?10:0),"100消費後2人連携・一部外れで加算なし");
        check(b.Snapshot().Combos.Pending.empty(),"履歴消費または中断");
        check(b.Snapshot().Units[1].Attack==12 && b.Snapshot().Units.back().Hp==30,"補正復元・中立無傷");
        const auto after=b.Snapshot();
        check(!b.UseTeamUltimate(after.Revision,1,{6,1}) && b.Snapshot()==after,"行動済み連打拒否");
        b.EndTurn(b.Snapshot().Revision,1,Facing::South);
        check(b.Snapshot().ActiveId==2 && !b.Snapshot().Acted,"他の仲間の手番を消費しない");
    }
    check(allHit>0 && partial>0,"命中と一部外れを検証");
    b=Test::TeamUltimateBattle();b.UseTeamUltimate(b.Snapshot().Revision,0,{6,1});
    check(b.Snapshot().Combos.Gauge==0,"5人参加だけで連携加算しない");
    for(auto role:{BattleRole::Standard,BattleRole::Dancer,BattleRole::Ninja,BattleRole::Archer,BattleRole::Gunner}) {
        b=Test::TeamUltimateBattle(1,100,0,role);
        b.UseTeamUltimate(b.Snapshot().Revision,0,{6,1});
        check(b.PreviewMove(0,{0,1}).Legal==(role==BattleRole::Dancer || role==BattleRole::Ninja),"役割別の発動後移動");
    }
    for(int variant:{0,5}) {
        b=Test::TeamUltimateBattle(1,100,variant);
        if(variant==0) check(b.Move(b.Snapshot().Revision,0,{1,4}),"射線検査の移動");
        const auto s=b.Snapshot();const auto r=b.RandomState();
        check(b.CanTeamUltimate(0) && !b.PreviewTeamUltimate(0,{6,1}).Legal,"配置成立でも射線・射程不足");
        check(!b.UseTeamUltimate(s.Revision,0,{6,1}) && b.Snapshot()==s && b.RandomState()==r,"射線・射程不足不変");
    }
    b=Test::TeamUltimateBattle();b.Attack(b.Snapshot().Revision,0,{6,1});
    check(b.Snapshot().Combos.Gauge==100 && !b.CanTeamUltimate(0),"ゲージ満タンでも行動済み不可");
    while(b.Snapshot().ActiveId!=100) b.EndTurn(b.Snapshot().Revision,b.Snapshot().ActiveId,Facing::South);
    check(!b.CanTeamUltimate(100),"敵の本人手番でも不可");
    std::cout<<"TeamUltimate: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
