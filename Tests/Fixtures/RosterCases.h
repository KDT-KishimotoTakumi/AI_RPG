#pragma once
#include "Application/Encounter.h"
#include "Infrastructure/SavePayload.h"
namespace GirlsSpire::TestData {
// 個人セーブではなく、人物定義を共用する前後の同一性を調べる架空の100シード。
inline RunSaveState RosterCase(unsigned seed)
{
    Run run(seed,static_cast<Difficulty>(seed%3),static_cast<HeroRole>((seed/3)%4));
    auto progress=run.Checkpoint();const auto kind=(seed/12)%4;
    progress.State.Phase=RunPhase::Event;progress.State.Choices.clear();
    const auto count=kind>=2?4:kind==0?seed%4:seed%5;
    for(unsigned i=0;i<count;++i)progress.State.Recruited.push_back(static_cast<int>((seed+i)%8));
    if(kind==0) {
        int candidate=static_cast<int>((seed+count)%8);
        progress.State.Day=static_cast<int>(count)+1;progress.State.Active={NodeKind::Rescue,candidate};
    } else {progress.State.Day=kind==1?5:kind==2?6:12;progress.State.Active={kind==1?NodeKind::Battle:kind==2?NodeKind::MidBoss:NodeKind::FinalBoss};}
    auto battle=Encounter::Create(progress.State,progress.RandomState);
    return {progress,SavedCombat{battle.Checkpoint(),CombatControl{}.Checkpoint()}};
}
inline std::uint64_t RosterPayloadHash(std::string_view bytes)
{
    std::uint64_t hash=14695981039346656037ULL;
    for(unsigned char value:bytes){hash^=value;hash*=1099511628211ULL;}return hash;
}
}
