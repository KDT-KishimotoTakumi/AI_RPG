#include "Application/Encounter.h"
#include "Application/CombatControl.h"
#include "Infrastructure/SavePayload.h"
#include <iostream>
#include <stdexcept>
int TestFormalEncounter()
{
    using namespace GirlsSpire;
    int checks=0,failed=0;
    const auto check=[&](bool pass,const char* message){++checks;if(!pass){++failed;std::cerr<<"FAIL FormalEncounter: "<<message<<'\n';}};
    Profile profile;profile.Bonds={0,24,49,50,74,75,99,100};
    for(int difficulty=0;difficulty<3;++difficulty)for(int role=0;role<4;++role)for(int seed=0;seed<10;++seed){
        RunSnapshot run;run.Phase=RunPhase::Event;run.Level=static_cast<Difficulty>(difficulty);run.Role=static_cast<HeroRole>(role);run.Recruited={0,2,4,6};
        for(auto kind:{NodeKind::Battle,NodeKind::Elite,NodeKind::Rescue,NodeKind::MidBoss,NodeKind::FinalBoss}){
            run.Active={kind,1};run.Day=kind==NodeKind::MidBoss?6:kind==NodeKind::FinalBoss?12:5;
            const auto old=Encounter::Create(run,seed).Checkpoint();
            const auto battle=Encounter::Create(run,seed,profile);auto expected=old;
            expected.State.AbilityRules=AbilityRuleSet::CompleteCharacters;expected.State.StartingBonds=profile.Bonds;
            expected.State.PersonalityAI=true;
            check(battle.Checkpoint()==expected,"正式識別/絆/性格以外の編成/地形/能力/手番/RNGを維持");
            for(const auto& unit:battle.Snapshot().Units)if(unit.Side==Faction::Player)
                check(battle.PotentialEnemyDamage(unit.Id)<unit.Hp,"正式技能を含めて初期即死を回避");
            check(old.State.AbilityRules==AbilityRuleSet::Legacy && old.State.StartingBonds==std::array<int,8>{},"旧APIは従来のまま");
            CombatControl control;control.ProfileDriven=true;for(int id=1;id<=8;++id)control.Trust[id]=profile.Bonds[id-1];
            check(CombatControl::Restore(control.Checkpoint(),battle).has_value(),"固定信頼と開始時絆が一致");
            const auto frozen=battle.Checkpoint();auto changed=profile;changed.Bonds.fill(1);
            const auto next=Encounter::Create(run,seed,changed);
            check(battle.Checkpoint()==frozen && next.Snapshot().StartingBonds==changed.Bonds,"次戦だけ新しい絆を反映");
        }
    }
    Run run(42,Difficulty::Beginner,HeroRole::Sword,profile);run.ChooseNode(0,0);
    GenerationResult generation;const auto fallback=Encounter::Create(run.Snapshot(),42,profile,&generation,true);
    check(generation.UsedFallback && fallback.Snapshot().AbilityRules==AbilityRuleSet::CompleteCharacters && fallback.Snapshot().StartingBonds==profile.Bonds,"固定戦場でも正式識別/絆を保持");
    CombatControl control;control.ProfileDriven=true;for(int id=1;id<=8;++id)control.Trust[id]=profile.Bonds[id-1];
    const RunSaveState saved{run.Checkpoint(),SavedCombat{fallback.Checkpoint(),control.Checkpoint()}};
    const auto bytes=EncodeRunPayload(saved);check(bytes && DecodeRunPayload(*bytes)==saved,"通常生成した戦闘/信頼を保存往復");
    for(int id=0;id<8;++id)for(int rank:{-1,101}){
        auto invalid=profile;invalid.Bonds[id]=rank;
        try{(void)Encounter::Create(run.Snapshot(),42,invalid);check(false,"不正絆を拒否");}catch(const std::invalid_argument&){check(true,"不正絆を拒否");}
    }
    auto invalid=profile;invalid.DiscoveredCombos={2,2};
    try{(void)Encounter::Create(run.Snapshot(),42,invalid);check(false,"不正Profileを拒否");}catch(const std::invalid_argument&){check(true,"不正Profileを拒否");}
    std::cout<<"FormalEncounter: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
