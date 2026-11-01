#include "CharacterCatalog.h"
#include "AbilityCatalog.h"
#include <stdexcept>
namespace GirlsSpire {
std::string_view CommonSkillName(Skill skill)
{
    const int index=static_cast<int>(skill);
    if(index<0 || index>=SkillCount)return {};
    static constexpr std::array<std::string_view,SkillCount> common={"強撃","応急手当","薙ぎ払い","遠当て","癒しの術"};
    return common[index];
}
std::string_view CharacterSkillName(int actorId,Skill skill)
{
    if(!FindCharacter(actorId))return {};
    if(actorId==0)if(const auto ability=FormalSkill(actorId,skill))return ability->Name;
    return CommonSkillName(skill);
}
std::string_view CharacterUltimateName(int actorId)
{
    if(!FindCharacter(actorId))return {};
    return actorId==0?FormalUltimate(actorId,0)->Name:"開発用個人必殺";
}
const std::array<CharacterDefinition,9>& CharacterCatalog()
{
    static constexpr std::array<CharacterDefinition,9> characters={{
        {0,"黒鉄 蓮","hero-ren","主人公",BattleRole::Standard,80,18,20,95,1},
        {1,"真田 茜","akane","近接",BattleRole::Standard,40,12,15,90,1},
        {2,"白峰 澪","mio","舞姫",BattleRole::Dancer,40,12,15,90,1},
        {3,"朧","oboro","忍",BattleRole::Ninja,40,12,15,90,1},
        {4,"土御門 紫苑","shion","近接",BattleRole::Standard,40,12,15,90,1},
        {5,"鷹司 楓","kaede","弓使い",BattleRole::Archer,40,12,15,90,4},
        {6,"雑賀 火乃香","honoka","鉄砲使い",BattleRole::Gunner,40,12,15,90,5},
        {7,"柳生 千景","chikage","近接",BattleRole::Standard,40,12,15,90,1},
        {8,"鈴鹿 小夜","sayo","舞姫",BattleRole::Dancer,40,12,15,90,1}
    }};
    return characters;
}
const CharacterDefinition* FindCharacter(int id)
{
    return id>=0 && id<static_cast<int>(CharacterCatalog().size())?&CharacterCatalog()[id]:nullptr;
}
BattleUnit CreateCharacter(int id,Position at)
{
    const auto definition=FindCharacter(id);
    if(!definition)throw std::invalid_argument("人物IDが無効です");
    BattleUnit unit;unit.Id=id;unit.At=at;unit.Role=definition->Role;
    unit.MaxHp=unit.Hp=definition->MaxHp;unit.Attack=definition->Attack;
    unit.Speed=definition->Speed;unit.Accuracy=definition->Accuracy;unit.Range=definition->Range;
    unit.Skills={true,true};return unit;
}
}
