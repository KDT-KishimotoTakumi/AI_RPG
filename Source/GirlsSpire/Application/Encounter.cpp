#include "Encounter.h"
#include "Domain/CharacterCatalog.h"
#include "Domain/AbilityCatalog.h"
#include <algorithm>
#include <set>
#include <stdexcept>
namespace GirlsSpire {
Battle Encounter::Create(const RunSnapshot& run, std::uint64_t seed, GenerationResult* details, bool forceGenerationFailure)
{
    return CreateConfigured(run,seed,nullptr,details,forceGenerationFailure);
}
Battle Encounter::CreateConfigured(const RunSnapshot& run,std::uint64_t seed,const Profile* profile,GenerationResult* details,bool forceGenerationFailure)
{
    const auto kind=run.Active.Kind;
    const bool boss=kind==NodeKind::MidBoss || kind==NodeKind::FinalBoss;
    if(run.Phase!=RunPhase::Event || run.Day<1 || run.Day>12 ||
        run.Level<Difficulty::Beginner || run.Level>Difficulty::Advanced ||
        run.Role<HeroRole::Sword || run.Role>HeroRole::Matchlock || run.Recruited.size()>4)
        throw std::invalid_argument("周回状態が無効です");
    if(kind!=NodeKind::Battle && kind!=NodeKind::Elite && kind!=NodeKind::Rescue && !boss)
        throw std::invalid_argument("戦闘ノードではありません");
    const std::set<int> recruited(run.Recruited.begin(),run.Recruited.end());
    if(recruited.size()!=run.Recruited.size()) throw std::invalid_argument("加入者が重複しています");
    for(int id:recruited) if(id<0 || id>=8) throw std::invalid_argument("加入者IDが無効です");
    if(kind==NodeKind::Rescue && (run.Active.Candidate<0 || run.Active.Candidate>=8 || recruited.contains(run.Active.Candidate)))
        throw std::invalid_argument("救出候補が無効です");
    std::vector<BattleUnit> units;
    auto hero=CreateCharacter(0,{1,1});
    const int ranges[]={1,2,3,5}; hero.Range=ranges[static_cast<int>(run.Role)];
    if(run.Role==HeroRole::Matchlock) { hero.Role=BattleRole::Gunner; hero.Movement=2; }
    units.push_back(hero);
    int slot=2;
    for(int id:run.Recruited) {
        units.push_back(CreateCharacter(id+1,{1,slot++}));
    }
    const int enemyCount=boss?1:1+(run.Day-1)/4;
    for(auto& unit:units) {
        ApplyGrowth(unit,run.Growth[unit.Id]);
        ApplyLevel(unit,run.Levels[unit.Id]);
        const auto accessory=run.Accessories[unit.Id];
        if(accessory!=Accessory::None && accessory!=Accessory::Power && accessory!=Accessory::Guard)
            throw std::invalid_argument("装飾品が無効です");
        if(accessory==Accessory::Power) unit.Attack+=3;
        if(accessory==Accessory::Guard) unit.Defense+=2;
        const auto& health=run.Health[unit.Id];
        unit.MaxHp=EffectiveMaxHp(unit.MaxHp,health.Wounds);
        if(health.Hp<-1 || health.Hp==0 || health.Hp>unit.MaxHp) throw std::invalid_argument("周回HPが無効です");
        unit.Hp=health.Hp==-1?unit.MaxHp:health.Hp;
    }
    if(kind==NodeKind::FinalBoss) {
        int guestSlot=0;
        for(int id:Run::Unrecruited(run)) {
            // 人物ごとに独立させ、経路生成・命中・戦闘判断の乱数位置を進めない。
            Random allegiance(seed ^ (UINT64_C(0x9e3779b97f4a7c15)*static_cast<std::uint64_t>(id+1)));
            auto guest=CreateCharacter(id+1,{8,2+guestSlot++});guest.Side=allegiance.Uniform(2)==0?Faction::Enemy:Faction::Neutral;
            if(guest.Side==Faction::Enemy)guest.MaxHp=guest.Hp=guest.MaxHp*(80+20*static_cast<int>(run.Level))/100;
            units.push_back(guest);
        }
    }
    for(int index=0;index<enemyCount;++index) {
        BattleUnit enemy; enemy.Id=100+index; enemy.Side=Faction::Enemy; enemy.At={4+index%2,1+index*2};
        const int baseHp=kind==NodeKind::MidBoss?160:kind==NodeKind::FinalBoss?240:kind==NodeKind::Elite?35:22;
        enemy.MaxHp=enemy.Hp=baseHp*(80+20*static_cast<int>(run.Level))/100;
        enemy.Attack=boss?18:10; enemy.Speed=10;
        if(kind==NodeKind::MidBoss){enemy.Role=BattleRole::FallenPrincess;enemy.Defense=4;enemy.Movement=2;enemy.Range=2;enemy.Speed=12;}
        if(kind==NodeKind::FinalBoss){enemy.Role=BattleRole::CastleLord;enemy.Attack=22;enemy.Defense=6;enemy.Movement=2;enemy.Range=3;enemy.Speed=15;}
        if(kind==NodeKind::Elite && index==0){enemy.CreatesCover=true;enemy.Defense=8;}
        else if(!boss) {
            const auto type=(seed%3+run.Day+index)%3;
            if(type==2 && run.Day>=5 && seed%2==0){enemy.Role=BattleRole::Ninja;enemy.Movement=4;}
            else if(type==2 && run.Day>=5 && seed%4==3){enemy.Role=BattleRole::Fox;enemy.Range=3;enemy.Attack=8;enemy.Skills[1]=true;}
            else if(type==1 && run.Day>=5 && seed%2==0){enemy.Role=BattleRole::Oni;enemy.Movement=2;enemy.Attack=16;enemy.Skills[0]=true;}
            else if(type==1 && run.Day>=5 && seed%4==1){enemy.Role=BattleRole::CurseMonk;enemy.Movement=2;enemy.Range=3;}
            else if(type==1){enemy.Role=BattleRole::Archer;enemy.Range=4;}
            else if(type==2){enemy.Role=BattleRole::Gunner;enemy.Range=5;enemy.Movement=2;}
            else if(run.Day>=5 && seed%4==1){enemy.Role=BattleRole::Spider;enemy.Movement=2;enemy.Range=3;}
            else if(run.Day>=5){enemy.Role=BattleRole::Cavalry;enemy.Movement=5;enemy.At.X=6;}
        }
        units.push_back(enemy);
    }
    if(kind==NodeKind::Rescue) {
        BattleUnit target; target.Id=200; target.Side=Faction::Neutral; target.At={6,6};
        units.push_back(target);
    }
    // HPを補充せず、瀕死の参加者がいる戦場では敵を初手の攻撃圏外へ離す。
    int initialThreat=0;
    int groupBuff=0;
    if(profile)for(const auto& unit:units)if(unit.Side==Faction::Enemy)for(int index=0;index<2;++index){
        const auto skill=FormalSkill(unit.Id,static_cast<Skill>(index));
        if(skill && unit.Skills[index] && skill->Target==AbilityTarget::LivingAlly)groupBuff=std::max(groupBuff,skill->Effects.AttackBuff);
    }
    for(const auto& unit:units)if(unit.Side==Faction::Enemy){
        int bonus=std::max(unit.Role==BattleRole::Cavalry?6:0,unit.Skills[0]?8:0);
        if(profile && unit.Id>=1 && unit.Id<=8){
            bonus=0;
            for(int index=0;index<2;++index){const auto skill=FormalSkill(unit.Id,static_cast<Skill>(index));if(skill && unit.Skills[index] && skill->Target==AbilityTarget::Enemy)bonus=std::max(bonus,skill->Effects.AttackBonus);}
        }
        initialThreat+=unit.Attack+bonus+groupBuff;
    }
    if(std::any_of(units.begin(),units.end(),[initialThreat](const BattleUnit& unit) {
        return unit.Side==Faction::Player && unit.Hp<=initialThreat;
    })) {
        for(auto& unit:units) if(unit.Side==Faction::Enemy){
            int range=unit.Range;
            if(profile)for(int index=0;index<2;++index){const auto skill=FormalSkill(unit.Id,static_cast<Skill>(index));if(skill && unit.Skills[index] && skill->Target==AbilityTarget::Enemy)range=std::max(range,skill->Range);}
            unit.At.X=std::min(11,std::max(7,range+unit.Movement+2));
        }
    }
    const auto goal=kind==NodeKind::Rescue?BattleGoal::Rescue:kind==NodeKind::FinalBoss?BattleGoal::DefeatLord:BattleGoal::Eliminate;
    const auto factory=forceGenerationFailure?
        Battlefield::DraftFactory{[](Random&) { return TerrainLayout{}; }}:Battlefield::DraftFactory{};
    auto field=Battlefield::Generate(seed,units,goal,8,factory,profile!=nullptr);
    if(details) *details=field;
    Battle battle(field.Layout.Width,field.Layout.Height,std::move(field.Layout.Tiles),std::move(units),seed,goal,3,run.Wallet.Medicines,80+20*static_cast<int>(run.Level));
    if(!profile)return battle;
    auto checkpoint=battle.Checkpoint();checkpoint.State.AbilityRules=AbilityRuleSet::CompleteCharacters;
    checkpoint.State.StartingBonds=profile->Bonds;checkpoint.State.PersonalityAI=true;
    checkpoint.State.EnemyAI=static_cast<EnemyThinking>(1+static_cast<int>(run.Level));checkpoint.State.EnemyCharacterSkills=true;
    auto restored=Battle::Restore(checkpoint);
    if(!restored)throw std::logic_error("生成した正式戦闘が無効です");
    return std::move(*restored);
}
}
