#include "Domain/AbilityCatalog.h"
#include "../Fixtures/AbilityContextCases.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
using namespace GirlsSpire;
int TestCharacterUltimates()
{
    int checks=0,failed=0;const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL CharacterUltimates: "<<why<<'\n';}};
    for(int id=0;id<9;++id)for(int rank:{0,100}){
        const auto ability=*FormalUltimate(id,rank);
        BattleUnit actor;actor.Id=id;actor.At={3,3};actor.Hp=60;actor.MaxHp=100;actor.Attack=20;actor.Speed=30;actor.Accuracy=200;actor.Range=3;
        auto ally=actor;ally.Id=id?0:1;ally.At={3,4};ally.Speed=20;ally.Hp=30;ally.AttackCursed=true;ally.MovementWebbed=true;
        auto a=actor;a.Id=100;a.Side=Faction::Enemy;a.At={4,3};a.Speed=10;a.Defense=10;a.Hp=a.MaxHp=500;
        auto b=a;b.Id=101;b.At={5,3};auto c=a;c.Id=102;c.At={4,4};
        Battle base(8,8,std::vector<Tile>(64),{actor,ally,a,b,c},42);auto context=base.Checkpoint();context.State.AbilityRules=AbilityRuleSet::CharacterAbilities;context.State.StartingBonds.fill(rank);context.State.Combos.Gauge=50;
        for(auto& unit:context.State.Units)if(unit.Side==Faction::Enemy)unit.DefenseBoost={4,false};
        auto battle=Battle::Restore(context);check(battle.has_value(),"正式個人必殺の識別を復元");if(!battle)continue;
        const auto target=ability.Area==AbilityArea::SelfRadius || ability.Area==AbilityArea::AdjacentFour?actor.At:a.At;
        const auto before=battle->Checkpoint();const auto preview=battle->PreviewUltimate(id,target);
        check(preview.Legal && battle->Checkpoint()==before,"9必殺/全絆強化を状態非消費で予告");
        const bool support=id==2 || id==8;
        const int count=id==4 || id==6?3:id==5?2:support?0:1;
        check(static_cast<int>(preview.Hits.size())==count && preview.Aid.size()==(support?2u:0u),"単体/十字/直線/隣接/範囲回復の対象数");
        for(const auto& hit:preview.Hits)check(hit.Damage==20+ability.Effects.AttackBonus-std::max(0,14-ability.Effects.IgnoreDefense) && hit.Chance==95,"防御強化を含む防御無視/威力/命中");
        check(!battle->UseUltimate(before.State.Revision+1,id,target) && battle->Checkpoint()==before,"不正revisionで無変更");
        check(battle->UseUltimate(before.State.Revision,id,target) && battle->Snapshot().Combos.Gauge==0 && battle->Snapshot().Acted,"ゲージ50と行動1回を消費");
        Random random(before.RandomState);for(std::size_t i=0;i<preview.Hits.size();++i)random.Uniform(100);
        check(battle->RandomState()==random.State(),"敵ごとに1命中判定、回復/強化はRNG非消費");
        if(support){
            const auto& self=battle->Snapshot().Units[0];const auto& friendUnit=battle->Snapshot().Units[1];
            check(self.Hp==60+ability.Effects.Healing && friendUnit.Hp==30+ability.Effects.Healing,"自分を含む味方の回復");
            check(self.AttackBoost==TimedBoost{ability.Effects.AttackBuff,ability.Effects.AttackBuff>0} && friendUnit.AttackBoost==TimedBoost{ability.Effects.AttackBuff,false},"自己/他者の強化と期限");
            check(friendUnit.AttackCursed!=ability.Effects.Cleanse && friendUnit.MovementWebbed!=ability.Effects.Cleanse,"絆100澪の低下解除");
        }else{
            for(const auto& hit:battle->LastHits()){
                const auto& unit=*std::find_if(battle->Snapshot().Units.begin(),battle->Snapshot().Units.end(),[&](const BattleUnit& u){return u.Id==hit.TargetId;});
                check(unit.AttackCursed==(hit.Hit && ability.Effects.AttackPenalty>0) && unit.MovementWebbed==(hit.Hit && ability.Effects.MovementPenalty>0),"弱体は命中時のみ");
            }
            check(battle->Snapshot().Units[0].DefenseBoost==TimedBoost{ability.Effects.SelfDefenseBuff,ability.Effects.SelfDefenseBuff>0},"絆100千景は自身防御を付与");
        }
        if(!battle->Snapshot().Combos.Pending.empty())check(battle->Snapshot().Combos.Pending.back().Attributes==Ending,"回復型も結");
        check(!battle->UseUltimate(battle->Snapshot().Revision,id,target),"行動後の再使用を拒否");
        auto low=context;low.State.Combos.Gauge=49;const auto shortage=Battle::Restore(low);check(shortage && !shortage->PreviewUltimate(id,target).Legal,"49ゲージで不可");
        check(!Battle::Restore(context)->PreviewUltimate(id,{-1,0}).Legal,"盤外を拒否");
    }
    const auto path=std::filesystem::path(u8"" __FILE__).parent_path().parent_path()/"Fixtures"/"ultimate-v14.bin";std::ifstream input(path,std::ios::binary);const std::string bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
    auto expected=TestData::AbilityContextCase(true);auto checkpoint=expected.Combat->Battle;checkpoint.State.AbilityRules=AbilityRuleSet::CharacterSkills;checkpoint.State.Units[0].Hp-=20;auto old=Battle::Restore(checkpoint);old->UseSkill(old->Snapshot().Revision,0,Skill::FirstAid,checkpoint.State.Units[0].At);expected.Combat->Battle=old->Checkpoint();
    check(bytes.size()==3438 && static_cast<unsigned char>(bytes[4])==14 && TestData::RosterPayloadHash(bytes)==UINT64_C(13246135108891492838) && DecodeRunPayload(bytes)==expected,"固定v14の旧通常技能/構文を維持");
    {
        BattleUnit archer;archer.Id=5;archer.At={1,1};archer.Speed=30;archer.Attack=20;
        auto hero=archer;hero.Id=0;hero.At={1,2};hero.Speed=20;
        auto enemy=archer;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={7,1};enemy.Speed=10;enemy.Hp=enemy.MaxHp=200;
        Battle arena(8,8,std::vector<Tile>(64),{archer,hero,enemy},42);auto c=arena.Checkpoint();c.State.AbilityRules=AbilityRuleSet::CharacterAbilities;c.State.Combos.Gauge=50;
        check(!Battle::Restore(c)->PreviewUltimate(5,{2,1}).Legal,"絆100未満の矢は距離5まで");
        c.State.StartingBonds[4]=100;auto strong=Battle::Restore(c);
        check(strong && strong->PreviewUltimate(5,{2,1}).Hits.size()==1,"絆100の矢は距離6に到達");
        check(!strong->PreviewUltimate(5,{2,2}).Legal && !strong->PreviewUltimate(5,{1,1}).Legal,"直線は斜め/方向なしを拒否");
        c.State.Tiles[1*8+4].Wall=true;check(!Battle::Restore(c)->PreviewUltimate(5,{2,1}).Legal,"矢は壁で停止");
        c.State.Tiles[1*8+4].Wall=false;c.State.Tiles[1*8+4].ObstacleHp=20;check(!Battle::Restore(c)->PreviewUltimate(5,{2,1}).Legal,"矢は障害物で停止");
        c.State.Tiles[1*8+4]={};c.State.Units[0].Id=2;c.State.Units[0].Hp=c.State.Units[0].MaxHp;c.State.Units[1].Hp=c.State.Units[1].MaxHp;c.State.Order[0]=2;c.State.ActiveId=2;c.State.StartingBonds.fill(100);
        auto full=Battle::Restore(c);check(full && !full->PreviewUltimate(2,{1,1}).Legal,"全員満タン/解除なしの回復は不可");
        c.State.Units[1].AttackCursed=true;auto cleanse=Battle::Restore(c);check(cleanse && cleanse->PreviewUltimate(2,{1,1}).Legal && !cleanse->PreviewUltimate(2,{1,2}).Legal,"絆100は解除だけでも可、中心は自身");
    }
    check(RunPayloadSchema>=15,"新必殺識別はRun v15");
    std::cout<<"CharacterUltimates: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
