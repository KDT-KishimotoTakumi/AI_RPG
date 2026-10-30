#include "Domain/Battle.h"
#include "AI/Intent.h"
#include <iostream>
int TestSkills()
{
    using namespace GirlsSpire;
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* label) {++checks;if(!ok){++failed;std::cerr<<"FAIL Skills: "<<label<<'\n';}};
    BattleUnit hero; hero.At={1,1}; hero.Speed=30; hero.Skills={true,true}; hero.Hp=10;
    BattleUnit enemy; enemy.Id=100; enemy.Side=Faction::Enemy; enemy.At={2,1}; enemy.Hp=enemy.MaxHp=100;
    std::vector<Tile> tiles(64);
    const auto make=[&](int seed=42) {return Battle(8,8,tiles,{hero,enemy},seed);};
    auto battle=make(); const auto before=battle.Snapshot(); const auto rng=battle.RandomState();
    const auto aid=battle.PreviewSkill(0,Skill::FirstAid,{1,1});
    check(aid.Legal && aid.Healing==20 && aid.TargetId==0,"自分への回復予告");
    check(aid==battle.PreviewSkill(0,Skill::FirstAid,{1,1}) && battle.Snapshot()==before && battle.RandomState()==rng,"予告は状態乱数不変");
    check(!battle.UseSkill(99,0,Skill::FirstAid,{1,1}) && battle.Snapshot()==before,"古い入力は拒否");
    check(battle.UseSkill(0,0,Skill::FirstAid,{1,1}),"回復を実行");
    check(battle.Snapshot().Units[0].Hp==30 && battle.Snapshot().Acted && battle.Snapshot().Units[0].Cooldowns[1]==2,"回復と行動・クールダウン消費");
    check(battle.RandomState()==rng,"回復は乱数不消費");
    check(!battle.UseSkill(battle.Snapshot().Revision,0,Skill::PowerStrike,{2,1}),"スキル使用後の二度目行動拒否");
    battle.EndTurn(battle.Snapshot().Revision,0,Facing::South);
    check(battle.Snapshot().Units[0].Cooldowns[1]==2,"使用手番終了では減らさない");
    battle.EndTurn(battle.Snapshot().Revision,100,Facing::South);
    check(battle.Snapshot().Units[0].Cooldowns[1]==2,"他者の手番で減らさない");
    for(int remaining : {2,1}) {
        check(battle.Snapshot().Units[0].Cooldowns[1]==remaining,"自分の2手番は残数あり");
        const auto snapshot=battle.Snapshot();
        check(!battle.UseSkill(snapshot.Revision,0,Skill::FirstAid,{1,1}) && battle.Snapshot()==snapshot,"再使用不可で状態不変");
        battle.EndTurn(battle.Snapshot().Revision,0,Facing::South); battle.EndTurn(battle.Snapshot().Revision,100,Facing::South);
    }
    check(battle.Snapshot().Units[0].Cooldowns[1]==0,"3回目の自分手番で再使用可能");
    bool hit=false,miss=false;
    for(int seed=0;seed<100;++seed) {
        auto first=make(seed),second=make(seed);
        const auto normal=first.PreviewAttack(0,{2,1}); const auto strike=first.PreviewSkill(0,Skill::PowerStrike,{2,1});
        check(strike.Legal && strike.Attack.Hits.size()==1,"強撃予告");
        if(!strike.Attack.Hits.empty()) check(strike.Attack.Hits[0].Chance==normal.Hits[0].Chance && strike.Attack.Hits[0].Damage==18,"命中共通・攻撃力+8を防御前に加算");
        check(first.UseSkill(0,0,Skill::PowerStrike,{2,1}) && second.UseSkill(0,0,Skill::PowerStrike,{2,1}),"強撃実行");
        check(first.Snapshot()==second.Snapshot() && first.RandomState()==second.RandomState(),"同じシードの再現");
        if(!first.LastHits().empty()) { hit=hit||first.LastHits()[0].Hit;miss=miss||!first.LastHits()[0].Hit; }
        check(first.Snapshot().Units[0].Attack==hero.Attack && first.Snapshot().Units[0].Cooldowns[0]==2,"外れても消費・攻撃値は恒久変更しない");
    }
    check(hit && miss,"命中と外れの両方を検証");
    hero.Cooldowns={2,2}; check(make().Snapshot().Units[0].Cooldowns==std::array<int,SkillCount>{0,0},"新しい戦闘はクールダウンをリセット"); hero.Cooldowns={};
    hero.Skills={false,false}; auto absent=make();
    check(!absent.PreviewSkill(0,Skill::FirstAid,{1,1}).Legal && !absent.UseSkill(0,0,Skill::PowerStrike,{2,1}),"不所持技を拒否"); hero.Skills={true,true};
    battle=make();
    for(const Skill skill:{static_cast<Skill>(-1),static_cast<Skill>(SkillCount)}) check(!battle.UseSkill(0,0,skill,{1,1}),"不正スキルID拒否");
    check(!battle.PreviewSkill(-1,Skill::FirstAid,{1,1}).Legal && !battle.PreviewSkill(100,Skill::FirstAid,{2,1}).Legal,"不正人物・手番外拒否");
    check(!battle.PreviewSkill(0,Skill::FirstAid,{-1,0}).Legal && !battle.PreviewSkill(0,Skill::FirstAid,{0,0}).Legal && !battle.PreviewSkill(0,Skill::FirstAid,{2,1}).Legal,"盤外・空マス・敵への回復拒否");
    hero.Hp=29; check(make().PreviewSkill(0,Skill::FirstAid,{1,1}).Healing==1,"回復上限");
    hero.Hp=30; check(!make().PreviewSkill(0,Skill::FirstAid,{1,1}).Legal,"満タン拒否"); hero.Hp=10;
    BattleUnit ally=hero; ally.Id=1; ally.At={1,3}; ally.Speed=10;
    auto friends=Battle(8,8,tiles,{hero,ally,enemy},42);
    check(friends.PreviewSkill(0,Skill::FirstAid,ally.At).Legal && friends.UseSkill(0,0,Skill::FirstAid,ally.At) && friends.Snapshot().Units[1].Hp==30,"味方への回復");
    ally.At={1,5}; check(!Battle(8,8,tiles,{hero,ally,enemy},42).PreviewSkill(0,Skill::FirstAid,ally.At).Legal,"射程外回復拒否");
    ally.At={1,3}; tiles[17].Wall=true;
    check(!Battle(8,8,tiles,{hero,ally,enemy},42).PreviewSkill(0,Skill::FirstAid,ally.At).Legal,"壁越し回復拒否"); tiles[17].Wall=false;
    ally.Hp=0; check(!Battle(8,8,tiles,{hero,ally,enemy},42).PreviewSkill(0,Skill::FirstAid,ally.At).Legal,"戦闘不能を蘇生しない");
    hero.Hp=0; check(!make().PreviewSkill(0,Skill::FirstAid,hero.At).Legal,"終了した戦闘は拒否"); hero.Hp=10;
    enemy.At={7,7}; tiles[10].ObstacleHp=20;
    auto terrain=make(); const auto terrainRng=terrain.RandomState();
    check(terrain.PreviewSkill(0,Skill::PowerStrike,{2,1}).Attack.TerrainDamage==20 && terrain.UseSkill(0,0,Skill::PowerStrike,{2,1}) && terrain.Snapshot().Tiles[10].ObstacleHp==0,"強撃も障害物を破壊");
    check(terrain.RandomState()==terrainRng,"障害物は確定命中で乱数不消費"); tiles[10]={};
    for(const auto role:{BattleRole::Dancer,BattleRole::Ninja,BattleRole::Archer,BattleRole::Gunner}) {
        hero.Role=role; auto movement=make(); movement.UseSkill(0,0,Skill::FirstAid,hero.At);
        check(movement.Move(movement.Snapshot().Revision,0,{2,1})==(role==BattleRole::Dancer||role==BattleRole::Ninja),"技後の移動も役割規則どおり");
    }
    hero.Role=BattleRole::Standard; hero.Hp=10; enemy.At={2,1};
    battle=make();
    const IntentOrder support{TacticalIntent::Support,{}, {}};
    auto ranked=RankIntentActions(battle,support);
    check(!ranked.empty() && ranked.front().Kind==static_cast<BasicActionKind>(3),"支援は攻撃より回復技を優先");
    auto forecast=IntentForecast::Build(battle,ranked,75); Random choice(42);
    check(forecast.Execute(battle,choice) && battle.Snapshot().Units[0].Hp==30,"支援予告から回復を実行");
    battle=make(); ranked=RankIntentActions(battle,{TacticalIntent::Assault,{}, {}});
    check(!ranked.empty() && ranked.front().Kind==static_cast<BasicActionKind>(3),"攻勢は威力の高い強撃を選ぶ");
    hero.Skills={false,false}; battle=make(); ranked=RankIntentActions(battle,support);
    check(ranked.front().Kind==BasicActionKind::Attack,"回復技がなければ通常攻撃へ退避");
    enemy.Skills={true,false};
    check(make().PotentialEnemyDamage(0)==18,"敵の強撃も被害上限に含める");
    hero.Skills={true,true}; enemy.Skills={}; battle=make();
    battle.UseSkill(0,0,Skill::PowerStrike,enemy.At);
    battle.EndTurn(battle.Snapshot().Revision,0,Facing::South);
    battle.EndTurn(battle.Snapshot().Revision,100,Facing::South);
    check(!battle.PreviewSkill(0,Skill::PowerStrike,enemy.At).Legal,"有効な敵でも残数2は再使用不可");
    battle.UseSkill(battle.Snapshot().Revision,0,Skill::FirstAid,hero.At);
    battle.EndTurn(battle.Snapshot().Revision,0,Facing::South);
    check(battle.Snapshot().Units[0].Cooldowns==std::array<int,SkillCount>{1,2},"別の技を使っても以前の技だけ減る");
    battle.EndTurn(battle.Snapshot().Revision,100,Facing::South);
    battle.EndTurn(battle.Snapshot().Revision,0,Facing::South);
    battle.EndTurn(battle.Snapshot().Revision,100,Facing::South);
    check(battle.PreviewSkill(0,Skill::PowerStrike,enemy.At).Legal,"3手番目に強撃が再使用可能");
    check(battle.UseSkill(battle.Snapshot().Revision,0,Skill::PowerStrike,enemy.At),"再使用を実行できる");
    std::cout<<"Skills: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
