#pragma once
#include "RosterCases.h"
namespace GirlsSpire::TestData {
inline RunSaveState AbilityContextCase(bool formal)
{
    auto saved=RosterCase(42);
    if(formal){
        saved.Combat->Battle.State.AbilityRules=AbilityRuleSet::Character;
        saved.Combat->Control.ProfileDriven=true;
        for(int id=0;id<8;++id){
            saved.Combat->Battle.State.StartingBonds[id]=10*(id+1);
            saved.Combat->Control.Trust[id+1]=10*(id+1);
        }
    }
    return saved;
}
}
