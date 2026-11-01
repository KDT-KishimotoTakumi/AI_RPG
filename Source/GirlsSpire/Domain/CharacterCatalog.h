#pragma once
#include "Battle.h"
#include <string_view>
namespace GirlsSpire {
struct CharacterDefinition {
    int Id;
    std::string_view Name,Portrait,RoleName;
    BattleRole Role;
    int MaxHp,Attack,Speed,Accuracy,Range;
};
const std::array<CharacterDefinition,9>& CharacterCatalog();
const CharacterDefinition* FindCharacter(int id);
BattleUnit CreateCharacter(int id,Position at);
std::string_view CharacterSkillName(int actorId,Skill skill);
std::string_view CommonSkillName(Skill skill);
std::string_view CharacterUltimateName(int actorId);
}
