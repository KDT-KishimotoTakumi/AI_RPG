#include "Domain/Battle.h"
#include <algorithm>
#include <iostream>
int TestComboBattle()
{
    using namespace GirlsSpire;
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* label){++checks;if(!ok){++failed;std::cerr<<"FAIL ComboBattle: "<<label<<'\n';}};
    std::vector<BattleUnit> units;
    for(int i=0;i<5;++i) {BattleUnit u;u.Id=i;u.At={1+i,1};u.Speed=50-i;u.Range=8;u.Skills={true,true};units.push_back(u);}
    BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={6,1};enemy.Hp=enemy.MaxHp=1000;enemy.Attack=0;enemy.Movement=0;units.push_back(enemy);
    const auto make=[&](int seed=42){return Battle(8,8,std::vector<Tile>(64),units,seed);};
    ComboSnapshot history;history.LastSequence=7;
    history.Pending={{1,0,Opening,{}},{2,4,Continuation,{}},{7,1,Ending,{100}}};
    auto found=FindComboCandidates(history,units);
    check(found.size()==1 && found[0].RecipeId==2 && found[0].Actions==std::vector<std::uint64_t>{1,7},"不要な味方行動を飛ばした隣接2人");
    units[1].At={3,1};check(FindComboCandidates(history,units).empty(),"現在配置で隣接していなければ不成立");units[1].At={2,1};
    history.Pending[2].ActorId=0;check(FindComboCandidates(history,units).empty(),"人物重複不可");history.Pending[2].ActorId=1;
    units[0].Hp=0;check(FindComboCandidates(history,units).empty(),"倒れた参加者不可");units[0].Hp=30;
    units[0].Side=Faction::Enemy;check(FindComboCandidates(history,units).empty(),"味方以外は参加不可");units[0].Side=Faction::Player;
    history.Pending[2].EnemyTargets.clear();check(FindComboCandidates(history,units).empty(),"最後が敵への攻撃でなければ不可");
    history.Pending={{1,0,Opening,{}},{2,1,Twist,{100}},{7,2,Continuation|Ending,{100}}};
    found=FindComboCandidates(history,units);check(found.size()==1 && found[0].RecipeId==3,"同一敵への3人連携");
    history.Pending[1].EnemyTargets={101};check(FindComboCandidates(history,units).empty(),"異なる敵では3人不成立");
    history.Pending[1].EnemyTargets={100};history.LastSequence=8;check(FindComboCandidates(history,units).empty(),"直前の行動を含まない履歴だけでは発動しない");
    history.LastSequence=5;history.Pending={{1,0,Opening,{}},{2,1,Continuation,{}},{3,2,Continuation,{}},{4,3,Twist,{100}},{5,4,Ending,{100}}};
    found=FindComboCandidates(history,units);
    check(std::any_of(found.begin(),found.end(),[](const auto& c){return c.RecipeId==5;}),"縦横合計4マス境界で5人成立");
    units[0].At={0,1};found=FindComboCandidates(history,units);
    check(std::none_of(found.begin(),found.end(),[](const auto& c){return c.RecipeId==5;}),"距離5では5人不成立");units[0].At={1,1};
    history.Pending[0].ActorId=99;check(FindComboCandidates(history,units).empty(),"存在しない参加者不可");
    check(FindComboCandidates({},units).empty(),"空履歴は不成立");
    history.Pending={{1,0,Opening,{}},{5,99,Ending,{100}}};
    check(FindComboCandidates(history,units).empty(),"最後の人物が不在なら不成立");
    history.Pending={{1,0,Opening,{}},{2,1,Opening,{}},{3,0,Twist,{100}},{5,2,Ending,{100}}};
    found=FindComboCandidates(history,units);
    check(std::any_of(found.begin(),found.end(),[](const auto& c){return c.RecipeId==3 && c.Actions==std::vector<std::uint64_t>{2,3,5};}),"先頭で人物重複する場合も別の開始候補を探索");
    for(int seed=0;seed<100;++seed) {
        auto battle=make(seed),repeat=make(seed);
        for(auto* b:{&battle,&repeat}) {
            b->EndTurn(b->Snapshot().Revision,0,Facing::South);
            b->Attack(b->Snapshot().Revision,1,enemy.At);
        }
        const bool hit=battle.LastHits()[0].Hit;
        check(battle.Snapshot().Combos.Gauge==(hit?10:0),"命中時だけ加算");
        check(battle.Snapshot()==repeat.Snapshot() && battle.RandomState()==repeat.RandomState(),"100シード再現");
        const auto before=battle.Snapshot();
        check(!battle.Attack(0,1,enemy.At) && battle.Snapshot()==before,"無効入力で連携状態不変");
        if(hit) check(battle.Snapshot().LastCombo && battle.Snapshot().LastCombo->Participants==std::vector<int>{0,1},"成立結果に参加者を保持");
        else check(battle.Snapshot().Combos.Pending.empty(),"外れで未成立履歴を中断");
    }
    auto battle=make();const auto before=battle.Snapshot();
    battle.PreviewAttack(0,enemy.At);battle.PreviewMove(0,{0,1});battle.PreviewSkill(0,Skill::PowerStrike,enemy.At);
    check(battle.Snapshot()==before,"予告では連携履歴を変更しない");
    battle.EndTurn(0,0,Facing::South);
    check(battle.Snapshot().Combos.Pending.size()==1 && battle.Snapshot().Combos.Pending[0].Attributes==Opening,"待機を起として記録");
    battle.Move(battle.Snapshot().Revision,1,{2,2});
    check(battle.Snapshot().Combos.Pending.size()==2 && battle.Snapshot().Combos.Pending.back().Attributes==Opening,"移動を起として記録");
    const auto original=units;
    for(int count:{3,5}) {
        units=original;
        const Position positions[]={{1,4},{2,3},{2,5},{5,4},{4,4}};
        for(int i=0;i<5;++i) units[i].At=count==5?positions[i]:Position{i,0};
        units[5].At={6,4};
        if(count==3) {units[0].At={0,0};units[1].At={3,1};units[2].At={5,1};}
        units[1].Hp=units[2].Hp=10;
        battle=make(0);battle.EndTurn(0,0,Facing::South);
        for(int actor=1;actor<count;++actor) {
            const auto revision=battle.Snapshot().Revision;
            if(count==5 && actor<3) {
                check(battle.UseSkill(revision,actor,Skill::FirstAid,units[actor].At),"回復で承を記録");
                check(battle.Snapshot().Combos.Pending.back().Attributes==(Continuation|Twist),"回復タグ");
            } else if(actor==count-2) {
                check(battle.UseSkill(revision,actor,Skill::PowerStrike,units[5].At),"強撃で転を記録");
                check(battle.Snapshot().Combos.Pending.back().Attributes==(Twist|Ending),"強撃タグ");
            } else check(battle.Attack(revision,actor,units[5].At),"最後の通常攻撃");
            if(actor+1<count) {
                check(battle.Snapshot().Combos.Gauge==0,"途中で短い連携が発動しない配置");
                battle.EndTurn(battle.Snapshot().Revision,actor,Facing::South);
            }
        }
        check(battle.Snapshot().Combos.Gauge==(count==3?20:40),"実際の3人・5人連携");
        check(battle.Snapshot().LastCombo && battle.Snapshot().LastCombo->Participants.size()==static_cast<std::size_t>(count),"最大人数を優先");
    }
    units=original;
    units[5].Speed=49;units[1].Speed=48;
    battle=make(0);battle.EndTurn(0,0,Facing::South);
    check(battle.Snapshot().ActiveId==100,"敵が途中で行動する順序");
    battle.EndTurn(1,100,Facing::South);
    battle.Attack(2,1,units[5].At);
    check(battle.Snapshot().Combos.Gauge==10,"敵の待機を挟んでも成立");
    units[5].Range=8;units[5].Attack=100;
    battle=make(0);battle.EndTurn(0,0,Facing::South);
    battle.Attack(1,100,units[0].At);
    check(battle.Snapshot().Combos.Pending.empty(),"敵が参加者を倒すと履歴中断");
    units=original;
    units[5].Hp=1;units.erase(units.begin()+2,units.begin()+5);
    battle=make();battle.EndTurn(0,0,Facing::South);battle.Attack(battle.Snapshot().Revision,1,enemy.At);
    check(battle.Snapshot().Outcome==BattleOutcome::Won && battle.Snapshot().Combos.Gauge==10 && battle.Snapshot().LastCombo,"最後の敵の撃破でも連携成立");
    units[2].Hp=1000;battle=make();battle.EndTurn(0,0,Facing::South);battle.EndTurn(1,1,Facing::South);battle.EndTurn(2,100,Facing::South);
    check(battle.Snapshot().Round==2 && battle.Snapshot().Combos.Pending.empty(),"ラウンド開始で履歴消去");
    std::cout<<"ComboBattle: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
