#include "Domain/Battle.h"
#include "AI/Intent.h"
#include <iostream>
int TestAddedSkills()
{
    using namespace GirlsSpire;
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* label){++checks;if(!ok){++failed;std::cerr<<"FAIL AddedSkill: "<<label<<'\n';}};
    BattleUnit hero;hero.At={1,1};hero.Speed=30;hero.Range=3;hero.Hp=10;hero.MaxHp=100;hero.Skills.fill(true);
    BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={4,1};enemy.Hp=enemy.MaxHp=1000;
    auto adjacent=enemy;adjacent.Id=101;adjacent.At={4,2};
    auto ally=hero;ally.Id=1;ally.At={3,1};ally.Speed=20;
    std::vector<Tile> tiles(64);
    const auto make=[&](int seed=0){return Battle(8,8,tiles,{hero,ally,enemy,adjacent},seed);};
    for(auto skill:{Skill::Sweep,Skill::FarStrike,Skill::Heal}) {
        auto b=make();const auto state=b.Snapshot();const auto rng=b.RandomState();
        const auto at=skill==Skill::Heal?hero.At:enemy.At;
        const auto p=b.PreviewSkill(0,skill,at);
        check(p.Legal,"追加技予告");
        check(b.Snapshot()==state && b.RandomState()==rng,"予告不変");
        check(!b.UseSkill(99,0,skill,at) && b.Snapshot()==state,"古い入力拒否");
        check(b.UseSkill(0,0,skill,at),"追加技実行");
        const int tags=skill==Skill::Heal?Continuation|Twist:skill==Skill::FarStrike?Continuation|Ending:Twist|Ending;
        const auto& history=b.Snapshot().Combos.Pending;
        check(!history.empty() && history.back().Attributes==tags,"追加技の構文属性");
        const int cd=skill==Skill::FarStrike?2:3;
        check(b.Snapshot().Acted && b.Snapshot().Units[0].Cooldowns[static_cast<int>(skill)]==cd,"行動とCD消費");
        check(b.Snapshot().Units[0].Attack==12 && b.Snapshot().Units[0].Range==3,"一時補正を戻す");
        check(b.Snapshot().Units[1].Hp==10,"味方に攻撃しない");
        for(int remaining=cd;remaining>=0;--remaining) {
            b.EndTurn(b.Snapshot().Revision,0,Facing::South);
            for(int id:{1,100,101}) b.EndTurn(b.Snapshot().Revision,id,Facing::South);
            check(b.Snapshot().Units[0].Cooldowns[static_cast<int>(skill)]==remaining,"自分の手番でだけCD減少");
            check(b.PreviewSkill(0,skill,at).Legal==(remaining==0),"再使用境界");
        }
        hero.Skills[static_cast<int>(skill)]=false;
        check(!make().PreviewSkill(0,skill,at).Legal,"不所持拒否");hero.Skills.fill(true);
    }
    auto b=make();
    check(b.PreviewSkill(0,Skill::Sweep,enemy.At).Attack.Hits.size()==2,"上下左右の敵2体");
    check(b.PreviewSkill(0,Skill::Heal,hero.At).Healing==40,"最大40回復");
    hero.Hp=99;check(make().PreviewSkill(0,Skill::Heal,hero.At).Healing==1,"回復上限");
    hero.Hp=100;check(!make().PreviewSkill(0,Skill::Heal,hero.At).Legal,"満タン不可");hero.Hp=10;
    ally.At={4,1};enemy.At={5,1};
    check(make().PreviewSkill(0,Skill::Heal,ally.At).Legal,"回復射程3境界");
    tiles[10].Wall=true;check(!make().PreviewSkill(0,Skill::Heal,ally.At).Legal,"回復も壁に遮られる");tiles[10]={};
    ally.At={5,2};check(!make().PreviewSkill(0,Skill::Heal,ally.At).Legal,"回復射程外");
    ally.At={3,1};ally.Hp=0;check(!make().PreviewSkill(0,Skill::Heal,ally.At).Legal,"戦闘不能者は回復不可");ally.Hp=10;enemy.At={4,1};
    enemy.At={6,1};check(make().PreviewSkill(0,Skill::FarStrike,enemy.At).Legal,"射程プラス2");
    enemy.At={7,1};check(!make().PreviewSkill(0,Skill::FarStrike,enemy.At).Legal,"射程外");enemy.At={4,1};
    tiles[2+8].Wall=true;
    check(!make().PreviewSkill(0,Skill::FarStrike,enemy.At).Legal && !make().PreviewSkill(0,Skill::Sweep,enemy.At).Legal,"壁越し攻撃不可");tiles[10]={};
    for(auto skill:{Skill::Sweep,Skill::FarStrike,Skill::Heal}) {
        b=make();const auto s=b.Snapshot();const auto r=b.RandomState();
        for(auto at:{Position{-1,0},Position{0,0}}) check(!b.UseSkill(0,0,skill,at) && b.Snapshot()==s && b.RandomState()==r,"無効対象は不変");
    }
    bool hit=false,miss=false;
    for(int seed=0;seed<100;++seed) {
        b=make(seed);auto copy=b;
        check(b.UseSkill(0,0,Skill::Sweep,enemy.At) && copy.UseSkill(0,0,Skill::Sweep,enemy.At),"100シードで範囲技実行");
        check(b.Snapshot()==copy.Snapshot() && b.RandomState()==copy.RandomState(),"同一シード再現");
        for(auto h:b.LastHits()){hit|=h.Hit;miss|=!h.Hit;check(h.Damage==(h.Hit?10:0),"通常攻撃力と防御");}
    }
    check(hit && miss,"命中と外れ");
    b=make();auto actions=RankIntentActions(b,{TacticalIntent::Support,{},{}});
    check(!actions.empty() && actions.front().Ability==Skill::Heal,"支援で追加回復を使用");
    std::cout<<"AddedSkills: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
