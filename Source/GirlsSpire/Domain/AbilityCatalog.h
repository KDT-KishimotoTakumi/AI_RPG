#pragma once
#include "Battle.h"
#include <optional>
#include <string_view>
namespace GirlsSpire {
enum class AbilityTarget { Enemy, LivingAlly, Self };
enum class AbilityArea { Single, TargetCross, SelfRadius, CardinalLine, AdjacentFour };
struct AbilityEffects {
    int AttackBonus=0,Healing=0,IgnoreDefense=0;
    int AttackPenalty=0,MovementPenalty=0;
    int AttackBuff=0,DefenseBuff=0,AccuracyBuff=0;
    bool Cleanse=false;
    // 敵への範囲攻撃と併用する使用者自身の強化。
    int SelfDefenseBuff=0;
    bool operator==(const AbilityEffects&) const=default;
};
struct CharacterAbility {
    std::string_view Name;
    AbilityTarget Target=AbilityTarget::Enemy;
    AbilityArea Area=AbilityArea::Single;
    // 0は通常攻撃射程を継承。Self指定は射程に関わらず自身を選択する。
    int Range=0,Cooldown=0,GaugeCost=0;
    ComboTag Syntax=Ending;
    AbilityEffects Effects;
    bool operator==(const CharacterAbility&) const=default;
};
// 定義だけを提供する。旧ルールの技能/表示はこの表へ自動変換しない。
std::optional<CharacterAbility> FormalSkill(int actorId,Skill slot);
std::optional<CharacterAbility> FormalUltimate(int actorId,int startingBond);
}
