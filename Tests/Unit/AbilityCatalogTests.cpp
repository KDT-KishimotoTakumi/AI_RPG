#include "Domain/AbilityCatalog.h"
#include <array>
#include <iostream>
#include <limits>
using namespace GirlsSpire;
int TestAbilityCatalog()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL AbilityCatalog: "<<why<<'\n';}};
    using T=AbilityTarget;using A=AbilityArea;
    const std::array<std::array<CharacterAbility,2>,9> skills={{
        {{{"鍛冶の一撃",T::Enemy,A::Single,0,2,0,Ending,{8}}, {"野戦の手当",T::LivingAlly,A::Single,3,2,0,Continuation,{0,20}}}},
        {{{"烈火突き",T::Enemy,A::Single,0,2,0,Ending,{8}}, {"退路断ち",T::Enemy,A::Single,0,3,0,Twist,{0,0,0,0,2}}}},
        {{{"癒しの舞",T::LivingAlly,A::Single,3,2,0,Continuation,{0,20}}, {"勇気の舞",T::LivingAlly,A::Single,3,3,0,Continuation,{0,0,0,0,0,4}}}},
        {{{"霞斬り",T::Enemy,A::Single,0,1,0,Ending,{4}}, {"影縫い",T::Enemy,A::Single,3,3,0,Twist,{0,0,0,0,2}}}},
        {{{"破魔の札",T::Enemy,A::Single,3,2,0,Ending,{2}}, {"封気の札",T::Enemy,A::Single,3,3,0,Twist,{0,0,0,4}}}},
        {{{"貫き矢",T::Enemy,A::Single,5,2,0,Ending,{4}}, {"狩人の眼",T::Self,A::Single,0,3,0,Continuation,{0,0,0,0,0,0,0,15}}}},
        {{{"装填射撃",T::Enemy,A::Single,5,3,0,Ending,{12}}, {"制圧射撃",T::Enemy,A::Single,5,3,0,Twist,{0,0,0,0,2}}}},
        {{{"返し斬り",T::Enemy,A::Single,0,2,0,Ending,{6}}, {"護身の構え",T::Self,A::Single,0,2,0,Continuation,{0,0,0,0,0,0,4}}}},
        {{{"鈴祓い",T::LivingAlly,A::Single,3,3,0,Continuation,{0,12,0,0,0,0,0,0,true}}, {"導き鈴",T::LivingAlly,A::Single,3,3,0,Continuation,{0,0,0,0,0,0,2,10}}}}
    }};
    const std::array<CharacterAbility,9> ultimates={{
        {"火打ちの誓い",T::Enemy,A::Single,0,0,50,Ending,{20}},
        {"朱槍一閃",T::Enemy,A::Single,0,0,50,Ending,{28}},
        {"命結びの舞",T::LivingAlly,A::SelfRadius,3,0,50,Ending,{0,25}},
        {"月影一刃",T::Enemy,A::Single,0,0,50,Ending,{24,0,4}},
        {"五芒封陣",T::Enemy,A::TargetCross,3,0,50,Ending,{18,0,0,4}},
        {"天翔一矢",T::Enemy,A::CardinalLine,5,0,50,Ending,{18}},
        {"紅蓮砲火",T::Enemy,A::TargetCross,5,0,50,Ending,{24}},
        {"四方断ち",T::Enemy,A::AdjacentFour,1,0,50,Ending,{20}},
        {"魂鎮め",T::LivingAlly,A::SelfRadius,3,0,50,Ending,{0,20,0,0,0,4}}
    }};
    const std::array<AbilityEffects,9> upgraded={{
        {20},{36,0,0,0,2},{0,35,0,0,0,0,0,0,true},{30,0,8},
        {24,0,0,4,2},{26},{32,0,4},{28,0,0,0,0,0,0,0,false,4},{0,30,0,0,0,6}
    }};
    for(int id=0;id<9;++id){
        for(int slot=0;slot<2;++slot)check(FormalSkill(id,static_cast<Skill>(slot))==skills[id][slot],"承認表の18技能の全フィールド一致");
        for(int bond=0;bond<=100;++bond){
            auto expected=ultimates[id];
            if(bond==100){expected.Effects=upgraded[id];if(id==5)expected.Range=6;}
            check(FormalUltimate(id,bond)==expected,"絆0～99は通常、100だけ強化。主人公は不変");
        }
        for(int invalid:{-1,2,3,4,5,std::numeric_limits<int>::max()})check(!FormalSkill(id,static_cast<Skill>(invalid)),"報酬技能と不正枠は正式初期技能にしない");
        for(int invalid:{-1,101,std::numeric_limits<int>::min(),std::numeric_limits<int>::max()})check(!FormalUltimate(id,invalid),"範囲外の開始時絆を拒否");
    }
    for(int id:{-1,9,100,std::numeric_limits<int>::min(),std::numeric_limits<int>::max()}){
        check(!FormalSkill(id,Skill::PowerStrike) && !FormalSkill(id,Skill::FirstAid),"人物以外/不正IDの正式技能を拒否");
        for(int bond:{0,99,100})check(!FormalUltimate(id,bond),"人物以外/不正IDの正式必殺を拒否");
    }
    auto copy=FormalUltimate(8,100);if(copy)copy->Effects.Healing=999;
    check(FormalUltimate(8,100)==CharacterAbility{"魂鎮め",T::LivingAlly,A::SelfRadius,3,0,50,Ending,{0,30,0,0,0,6}},"返却値の編集で共用定義を書換えない");
    std::cout<<"AbilityCatalog: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
