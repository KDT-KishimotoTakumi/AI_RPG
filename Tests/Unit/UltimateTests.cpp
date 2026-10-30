#include "../Fixtures/UltimateBattle.h"
#include "AI/Intent.h"
#include <iostream>

int TestUltimate()
{
    using namespace GirlsSpire;
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* label){++checks;if(!ok){++failed;std::cerr<<"FAIL Ultimate: "<<label<<'\n';}};
    ComboLedger ledger;
    check(!ledger.Spend(50),"不足拒否");
    for(int i=0;i<10;++i) {
        const auto seq=static_cast<std::uint64_t>(i*2+1);
        ledger.Observe({seq,1,0});ledger.Observe({seq+1,1,1});ledger.Resolve(seq+1,{{2,{seq,seq+1}}});
    }
    const auto full=ledger.Snapshot();
    for(int amount:{-1,0,101}) check(!ledger.Spend(amount) && ledger.Snapshot()==full,"無効消費は不変");
    check(ledger.Spend(50) && ledger.Snapshot().Gauge==50,"100から50消費");
    check(ledger.Spend(50) && ledger.Snapshot().Gauge==0,"残り50を消費");
    check(!ledger.Spend(1) && ledger.Snapshot().Gauge==0,"負のゲージ不可");
    auto battle=Test::UltimateBattle();
    const auto before=battle.Snapshot();const auto random=battle.RandomState();
    const auto prediction=battle.PreviewUltimate(0,{6,1});
    check(prediction.Legal && prediction.Hits.size()==1 && prediction.Hits[0].Damage==30,"個人必殺の威力");
    check(battle.Snapshot()==before && battle.RandomState()==random,"予告でゲージ・履歴・乱数を変更しない");
    for(auto target:{Position{-1,1},Position{8,1},Position{0,0},Position{1,1},Position{2,1},Position{4,4}}) {
        check(!battle.PreviewUltimate(0,target).Legal,"無効対象の予告");
        check(!battle.UseUltimate(before.Revision,0,target) && battle.Snapshot()==before && battle.RandomState()==random,"無効対象不変");
    }
    check(!battle.UseUltimate(before.Revision-1,0,{6,1}) && battle.Snapshot()==before,"古い入力拒否");
    for(int actor:{-1,1,100,9999}) check(!battle.PreviewUltimate(actor,{6,1}).Legal,"本人の手番だけ");
    for(int gauge:{0,40,50,100}) {
        battle=Test::UltimateBattle(0,gauge);
        const auto state=battle.Snapshot();
        const bool legal=gauge>=50;
        check(battle.PreviewUltimate(0,{6,1}).Legal==legal,"ゲージ境界の予告");
        check(battle.UseUltimate(state.Revision,0,{6,1})==legal,"ゲージ境界の実行");
        check(battle.Snapshot().Combos.Gauge==(legal?gauge-50:gauge),"ゲージ消費量");
        check(battle.Snapshot().Acted==legal,"行動消費");
        const auto after=battle.Snapshot();
        check(!battle.UseUltimate(state.Revision,0,{6,1}) && battle.Snapshot()==after,"二重入力拒否");
        check(!battle.UseUltimate(after.Revision,0,{6,1}) && battle.Snapshot()==after,"行動済みまたは不足拒否");
    }
    int hits=0,misses=0;
    for(int seed=0;seed<100;++seed) {
        battle=Test::UltimateBattle(seed,50);auto repeat=battle;
        for(auto* b:{&battle,&repeat}) {
            b->EndTurn(b->Snapshot().Revision,0,Facing::South);
            check(b->UseUltimate(b->Snapshot().Revision,1,{6,1}),"仲間も手動必殺");
        }
        check(battle.Snapshot()==repeat.Snapshot() && battle.RandomState()==repeat.RandomState(),"100シード再現");
        if(battle.LastHits().empty()) {check(false,"命中結果を記録");continue;}
        const bool hit=battle.LastHits()[0].Hit;
        hit?++hits:++misses;
        check(battle.Snapshot().Combos.Gauge==(hit?10:0),"消費後の連携加算・外れても消費");
        check(battle.Snapshot().Combos.Pending.empty(),"成立分消費または外れで履歴中断");
        check(battle.Snapshot().Units[1].Attack==12 && battle.Snapshot().Units[1].Cooldowns==std::array<int,SkillCount>{0,0},"一時攻撃補正を残さない・通常技CDを変更しない");
    }
    check(hits>0 && misses>0,"成功と外れを両方検証");
    for(auto role:{BattleRole::Standard,BattleRole::Dancer,BattleRole::Ninja,BattleRole::Archer,BattleRole::Gunner}) {
        battle=Test::UltimateBattle(0,100,role);
        battle.UseUltimate(battle.Snapshot().Revision,0,{6,1});
        check(battle.PreviewMove(0,{1,2}).Legal==(role==BattleRole::Dancer || role==BattleRole::Ninja),"必殺後も役割別移動");
    }
    battle=Test::UltimateBattle();battle.EndTurn(battle.Snapshot().Revision,0,Facing::South);
    Random choice(0);auto forecast=IntentForecast::Build(battle,RankIntentActions(battle,{}),75);
    check(forecast.Execute(battle,choice) && battle.Snapshot().Combos.Gauge==100,"仲間AIは共有ゲージを消費しない");
    battle.EndTurn(battle.Snapshot().Revision,1,Facing::South);
    check(!battle.PreviewUltimate(100,{1,1}).Legal,"敵の手番でも共有ゲージを使用不可");
    for(int range:{5,8}) {
        battle=Test::UltimateBattle(1,100,BattleRole::Standard,10000,range);
        check(battle.Move(battle.Snapshot().Revision,0,range==5?Position{1,2}:Position{1,4}),"射程・射線検査用に移動");
        const auto state=battle.Snapshot();const auto rng=battle.RandomState();
        check(!battle.PreviewUltimate(0,{6,1}).Legal,"射程外または壁越しは予告不可");
        check(!battle.UseUltimate(state.Revision,0,{6,1}) && battle.Snapshot()==state && battle.RandomState()==rng,"射程・射線の拒否で消費しない");
    }
    battle=Test::UltimateBattle(1,100,BattleRole::Standard,130);
    battle.EndTurn(battle.Snapshot().Revision,0,Facing::South);
    check(battle.UseUltimate(battle.Snapshot().Revision,1,{6,1}),"最後の敵へ必殺");
    check(battle.Snapshot().Outcome==BattleOutcome::Won && battle.Snapshot().Combos.Gauge==60,"100から消費して撃破連携10を加算");
    const auto won=battle.Snapshot();
    check(!battle.UseUltimate(won.Revision,1,{6,1}) && battle.Snapshot()==won,"終了後の操作拒否");
    std::cout<<"Ultimate: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
