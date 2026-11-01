#include "Domain/CharacterCatalog.h"
#include "Application/Encounter.h"
#include <iostream>
using namespace GirlsSpire;
int TestHeroAbilities()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL HeroAbilities: "<<why<<'\n';}};
    check(CharacterSkillName(0,Skill::PowerStrike)=="鍛冶の一撃" && CharacterSkillName(0,Skill::FirstAid)=="野戦の手当" && CharacterUltimateName(0)=="火打ちの誓い","承認済み主人公技能/必殺の正式名");
    for(int id=1;id<=8;++id)check(CharacterSkillName(id,Skill::PowerStrike)=="強撃" && CharacterSkillName(id,Skill::FirstAid)=="応急手当" && CharacterUltimateName(id)=="開発用個人必殺","未接続の仲間を正式技能として表示しない");
    for(int id:{-1,9,100})check(CharacterSkillName(id,Skill::PowerStrike).empty() && CharacterUltimateName(id).empty(),"不正人物を拒否");
    for(int skill:{-1,SkillCount})check(CharacterSkillName(0,static_cast<Skill>(skill)).empty(),"不正技能を拒否");
    for(auto skill:{Skill::Sweep,Skill::FarStrike,Skill::Heal})check(!CharacterSkillName(0,skill).empty(),"報酬で追加した既存技能も表示できる");
    for(int role=0;role<4;++role) {
        RunSnapshot run;run.Phase=RunPhase::Event;run.Day=5;run.Role=static_cast<HeroRole>(role);run.Active={NodeKind::Battle};
        auto hero=Encounter::Create(run,42).Snapshot().Units.front();hero.At={1,1};hero.Hp=30;
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={2,1};enemy.Hp=enemy.MaxHp=200;enemy.Defense=2;
        Battle original(8,8,std::vector<Tile>(64),{hero,enemy},42);
        const auto attack=original.PreviewSkill(0,Skill::PowerStrike,enemy.At);
        const auto heal=original.PreviewSkill(0,Skill::FirstAid,hero.At);
        check(attack.Legal && attack.Attack.Hits[0].Damage==hero.Attack+8-enemy.Defense && heal.Legal && heal.Healing==20,"4武器で承認効果に一致");
        auto checkpoint=original.Checkpoint();checkpoint.State.Combos.Gauge=50;const auto charged=Battle::Restore(checkpoint);
        check(charged && charged->PreviewUltimate(0,enemy.At).Hits[0].Damage==hero.Attack+20-enemy.Defense,"個人必殺の承認効果に一致");
    }
    std::cout<<"HeroAbilities: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
