#include "AbilityCatalog.h"
#include <array>
namespace GirlsSpire {
namespace {
using T=AbilityTarget;using A=AbilityArea;
constexpr std::array<std::array<CharacterAbility,2>,9> Skills={{
    {{{"鍛冶の一撃",T::Enemy,A::Single,0,2,0,Ending,{.AttackBonus=8}},
      {"野戦の手当",T::LivingAlly,A::Single,3,2,0,Continuation,{.Healing=20}}}},
    {{{"烈火突き",T::Enemy,A::Single,0,2,0,Ending,{.AttackBonus=8}},
      {"退路断ち",T::Enemy,A::Single,0,3,0,Twist,{.MovementPenalty=2}}}},
    {{{"癒しの舞",T::LivingAlly,A::Single,3,2,0,Continuation,{.Healing=20}},
      {"勇気の舞",T::LivingAlly,A::Single,3,3,0,Continuation,{.AttackBuff=4}}}},
    {{{"霞斬り",T::Enemy,A::Single,0,1,0,Ending,{.AttackBonus=4}},
      {"影縫い",T::Enemy,A::Single,3,3,0,Twist,{.MovementPenalty=2}}}},
    {{{"破魔の札",T::Enemy,A::Single,3,2,0,Ending,{.AttackBonus=2}},
      {"封気の札",T::Enemy,A::Single,3,3,0,Twist,{.AttackPenalty=4}}}},
    {{{"貫き矢",T::Enemy,A::Single,5,2,0,Ending,{.AttackBonus=4}},
      {"狩人の眼",T::Self,A::Single,0,3,0,Continuation,{.AccuracyBuff=15}}}},
    {{{"装填射撃",T::Enemy,A::Single,5,3,0,Ending,{.AttackBonus=12}},
      {"制圧射撃",T::Enemy,A::Single,5,3,0,Twist,{.MovementPenalty=2}}}},
    {{{"返し斬り",T::Enemy,A::Single,0,2,0,Ending,{.AttackBonus=6}},
      {"護身の構え",T::Self,A::Single,0,2,0,Continuation,{.DefenseBuff=4}}}},
    {{{"鈴祓い",T::LivingAlly,A::Single,3,3,0,Continuation,{.Healing=12,.Cleanse=true}},
      {"導き鈴",T::LivingAlly,A::Single,3,3,0,Continuation,{.DefenseBuff=2,.AccuracyBuff=10}}}}
}};
constexpr std::array<CharacterAbility,9> Ultimates={{
    {"火打ちの誓い",T::Enemy,A::Single,0,0,50,Ending,{.AttackBonus=20}},
    {"朱槍一閃",T::Enemy,A::Single,0,0,50,Ending,{.AttackBonus=28}},
    {"命結びの舞",T::LivingAlly,A::SelfRadius,3,0,50,Ending,{.Healing=25}},
    {"月影一刃",T::Enemy,A::Single,0,0,50,Ending,{.AttackBonus=24,.IgnoreDefense=4}},
    {"五芒封陣",T::Enemy,A::TargetCross,3,0,50,Ending,{.AttackBonus=18,.AttackPenalty=4}},
    {"天翔一矢",T::Enemy,A::CardinalLine,5,0,50,Ending,{.AttackBonus=18}},
    {"紅蓮砲火",T::Enemy,A::TargetCross,5,0,50,Ending,{.AttackBonus=24}},
    {"四方断ち",T::Enemy,A::AdjacentFour,1,0,50,Ending,{.AttackBonus=20}},
    {"魂鎮め",T::LivingAlly,A::SelfRadius,3,0,50,Ending,{.Healing=20,.AttackBuff=4}}
}};
}
std::optional<CharacterAbility> FormalSkill(int actorId,Skill slot)
{
    const auto index=static_cast<int>(slot);
    if(actorId<0 || actorId>=static_cast<int>(Skills.size()) || index<0 || index>=2)return std::nullopt;
    return Skills[actorId][index];
}
std::optional<CharacterAbility> FormalUltimate(int actorId,int startingBond)
{
    if(actorId<0 || actorId>=static_cast<int>(Ultimates.size()) || startingBond<0 || startingBond>100)return std::nullopt;
    auto result=Ultimates[actorId];
    if(startingBond!=100)return result;
    // 開始時の固定絆だけで置換する。主人公自身に絆による変化はない。
    switch(actorId){
    case 1:result.Effects={.AttackBonus=36,.MovementPenalty=2};break;
    case 2:result.Effects={.Healing=35,.Cleanse=true};break;
    case 3:result.Effects={.AttackBonus=30,.IgnoreDefense=8};break;
    case 4:result.Effects={.AttackBonus=24,.AttackPenalty=4,.MovementPenalty=2};break;
    case 5:result.Effects={.AttackBonus=26};result.Range=6;break;
    case 6:result.Effects={.AttackBonus=32,.IgnoreDefense=4};break;
    case 7:result.Effects={.AttackBonus=28,.SelfDefenseBuff=4};break;
    case 8:result.Effects={.Healing=30,.AttackBuff=6};break;
    }
    return result;
}
}
