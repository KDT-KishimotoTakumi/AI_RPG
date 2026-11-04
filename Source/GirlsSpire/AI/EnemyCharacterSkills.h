#pragma once
#include "BasicTurn.h"
#include <optional>
namespace GirlsSpire {
struct CharacterAttack {
    BasicAction Action;
    AttackPreview Preview;
};
bool UsesCharacterEnemyAI(const Battle& battle);
std::optional<CharacterAttack> ChooseCharacterAttack(const Battle& battle,Position target);
std::optional<BasicAction> ChooseCharacterRecovery(const Battle& battle);
std::optional<BasicAction> ChooseCharacterPreparation(const Battle& battle);
}
