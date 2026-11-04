#pragma once
#include "Application/Encounter.h"
#include "Infrastructure/SavePayload.h"
namespace GirlsSpire::TestData {
inline std::optional<std::uint64_t> EnemyEncounterHash()
{
    std::uint64_t hash=UINT64_C(14695981039346656037);Profile profile;profile.Bonds={0,24,49,50,74,75,99,100};
    for(int difficulty=0;difficulty<3;++difficulty)for(int role=0;role<4;++role)for(int seed=0;seed<10;++seed)
        for(auto kind:{NodeKind::Battle,NodeKind::Elite,NodeKind::Rescue,NodeKind::MidBoss,NodeKind::FinalBoss})for(bool formal:{false,true}){
            RunSnapshot run;run.Phase=RunPhase::Event;run.Level=static_cast<Difficulty>(difficulty);run.Role=static_cast<HeroRole>(role);run.Recruited={0,2,4,6};run.Active={kind,kind==NodeKind::Rescue?1:-1};run.Day=kind==NodeKind::MidBoss?6:kind==NodeKind::FinalBoss?12:5;
            auto battle=formal?Encounter::Create(run,seed,profile):Encounter::Create(run,seed);
            CombatControl control;control.ProfileDriven=formal;for(int id=1;id<=8;++id)control.Trust[id]=formal?profile.Bonds[id-1]:50;
            const auto bytes=EncodeRunPayload({{run,42,RunPhase::Route},SavedCombat{battle.Checkpoint(),control.Checkpoint()}});
            if(!bytes)return std::nullopt;
            for(unsigned char byte:*bytes){hash^=byte;hash*=UINT64_C(1099511628211);}
        }
    return hash;
}
}
