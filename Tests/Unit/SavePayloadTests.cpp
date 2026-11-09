#include "Infrastructure/SavePayload.h"
#include "Application/Encounter.h"
#include "AI/BasicTurn.h"
#include <iostream>

using namespace GirlsSpire;
int TestSavePayload()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL SavePayload: "<<why<<'\n';}};
    const auto verify=[&](const RunSaveState& state) {
        const auto before=state;
        const auto bytes=EncodeRunPayload(state);
        check(bytes.has_value(),"有効状態を符号化");
        check(state==before,"符号化で元状態不変");
        if(!bytes) return;
        const auto restored=DecodeRunPayload(*bytes);
        check(restored && *restored==state,"全フィールドの往復一致");
        check(!DecodeRunPayload(*bytes+"x"),"余剰バイト拒否");
        for(std::size_t n=0;n<bytes->size();++n) check(!DecodeRunPayload(std::string_view(*bytes).substr(0,n)),"切断拒否");
    };
    check(!DecodeRunPayload(""),"空の周回拒否");check(!DecodeProfilePayload(""),"空の恒久拒否");
    for(int rank=0;rank<=100;++rank) {
        Profile profile;profile.Bonds.fill(rank);
        const auto bytes=EncodeProfilePayload(profile);check(bytes.has_value(),"全絆値の符号化");
        if(bytes) {
            check(DecodeProfilePayload(*bytes)==profile,"恒久の往復");
            check(!DecodeRunPayload(*bytes),"恒久を周回として拒否");
            check(!DecodeProfilePayload(*bytes+"x"),"恒久の余剰拒否");
            for(std::size_t n=0;n<bytes->size();++n) check(!DecodeProfilePayload(std::string_view(*bytes).substr(0,n)),"恒久の切断拒否");
        }
    }
    Profile invalid;invalid.Bonds[0]=101;check(!EncodeProfilePayload(invalid),"不正絆拒否");
    for(int seed=0;seed<12;++seed) {
        Run run(seed,static_cast<Difficulty>(seed%3),static_cast<HeroRole>(seed%4));
        for(int step=0;step<8;++step) {
            verify({run.Checkpoint(),{}});
            if(run.Snapshot().Phase!=RunPhase::Route) break;
            run.ChooseNode(run.Snapshot().Revision,0);
            verify({run.Checkpoint(),{}});
            const auto kind=run.Snapshot().Active.Kind;
            if(kind==NodeKind::Battle || kind==NodeKind::Rescue || kind==NodeKind::Elite || kind==NodeKind::MidBoss || kind==NodeKind::FinalBoss) {
                auto battle=Encounter::Create(run.Snapshot(),run.RandomState());
                CombatControl control;control.ProfileDriven=true;control.ChoiceRandom=Random(seed+33);
                for(int action=0;action<3;++action) {
                    verify({run.Checkpoint(),SavedCombat{battle.Checkpoint(),control.Checkpoint()}});
                    if(battle.Snapshot().Outcome!=BattleOutcome::Ongoing) break;
                    ApplyBasicAction(battle,ChooseBasicAction(battle));
                }
            }
            run.CompleteEvent(run.Snapshot().Revision);
        }
    }
    Run run(42,Difficulty::Beginner,HeroRole::Sword);
    auto invalidRun=RunSaveState{run.Checkpoint(),{}};invalidRun.Run.State.Day=0;
    check(!EncodeRunPayload(invalidRun),"不正周回拒否");
    run.ChooseNode(0,0);auto battle=Encounter::Create(run.Snapshot(),run.RandomState());
    RunSaveState valid{run.Checkpoint(),SavedCombat{battle.Checkpoint(),CombatControl{}.Checkpoint()}};
    auto bad=valid;bad.Combat->Control.Trust[0]=-1;check(!EncodeRunPayload(bad),"不正作戦拒否");
    bad=valid;bad.Combat->Battle.State.Units[0].Hp=-1;check(!EncodeRunPayload(bad),"不正戦闘拒否");
    bad=valid;bad.Run.State.Phase=RunPhase::Route;check(!EncodeRunPayload(bad),"戦闘と周回局面の不一致拒否");
    bad=valid;bad.Combat->Battle.State.Units[0].Id=1;check(!EncodeRunPayload(bad),"周回と戦闘の編成不一致拒否");
    const auto bytes=EncodeRunPayload(valid);
    if(bytes) {
        check(!DecodeProfilePayload(*bytes),"周回を恒久として拒否");
        auto corrupt=*bytes;corrupt[0]=99;check(!DecodeRunPayload(corrupt),"不明形式拒否");
    }
    // FR-022: 精鋭戦の生存戦（偶数日）と全滅戦（奇数日・旧保存）はどちらも往復でき、他の種別への生存目的は拒否する。
    {
        auto eliteRun=run.Checkpoint();eliteRun.State.Active={NodeKind::Elite,-1};
        for(int day:{5,6}) {
            eliteRun.State.Day=day;
            auto elite=Encounter::Create(eliteRun.State,7);
            check(elite.Checkpoint().Goal==(day%2==0?BattleGoal::Survive:BattleGoal::Eliminate),"精鋭戦の目的が日で決まる");
            CombatControl control;control.ProfileDriven=true;control.ChoiceRandom=Random(day+33);
            for(int action=0;action<4;++action) {
                verify({eliteRun,SavedCombat{elite.Checkpoint(),control.Checkpoint()}});
                if(elite.Snapshot().Outcome!=BattleOutcome::Ongoing) break;
                ApplyBasicAction(elite,ChooseBasicAction(elite));
            }
        }
        eliteRun.State.Day=6;
        const RunSaveState surviveState{eliteRun,SavedCombat{Encounter::Create(eliteRun.State,7).Checkpoint(),CombatControl{}.Checkpoint()}};
        check(EncodeRunPayload(surviveState).has_value(),"精鋭戦の生存戦を保存できる");
        for(auto kind:{NodeKind::Battle,NodeKind::Rescue,NodeKind::MidBoss,NodeKind::FinalBoss}) {
            auto wrong=surviveState;wrong.Run.State.Active={kind,kind==NodeKind::Rescue?1:-1};
            check(!EncodeRunPayload(wrong),"精鋭戦以外への生存目的は拒否");
        }
        auto wrongRounds=surviveState;wrongRounds.Combat->Battle.SurvivalRounds=0;
        check(!EncodeRunPayload(wrongRounds),"生存ラウンド数の不正は拒否");
    }
    auto richRun=run.Checkpoint();richRun.State.Day=5;richRun.State.Active={NodeKind::Battle};
    richRun.State.Recruited={0,1,2,3};richRun.State.Wallet.Medicines=5;
    for(int id=0;id<9;++id) {
        richRun.State.Growth[id].Bonuses={1,2,3,4};richRun.State.Growth[id].Learned={true,true,true};
        richRun.State.Levels[id]={9,199};richRun.State.Accessories[id]=static_cast<Accessory>(id%3);
        richRun.State.Health[id].Wounds=id%5;
    }
    auto richBattle=Encounter::Create(richRun.State,123).Checkpoint();
    richBattle.State.Revision=2;richBattle.State.Combos={1,90,2,1,{{1,0,Opening,{100}},{2,1,Ending,{100}}}};
    richBattle.State.LastCombo=ComboResolution{1,{0,1},10};richBattle.State.BondGains.Combo[0]=1;richBattle.State.BondGains.Aid[1]=1;
    richBattle.LastHits={{100,true,2},{101,false,0}};
    // 5日目は敵2体。直近の技・再使用待ちも初期化せず保持する。
    richBattle.State.Acted=true;richBattle.State.UsedSkill=0;richBattle.State.Units[0].Cooldowns[0]=1;
    CombatControl richControl;richControl.ProfileDriven=true;richControl.Trust.fill(50);
    richControl.Orders[1]={TacticalIntent::Assault,100,{}};richControl.Orders[2]={TacticalIntent::Escort,{},Position{2,2}};
    verify({richRun,SavedCombat{richBattle,richControl.Checkpoint()}});
    run.CompleteBattle(run.Snapshot().Revision,BattleOutcome::Won);verify({run.Checkpoint(),{}});
    run.ChooseRecipient(run.Snapshot().Revision,0);verify({run.Checkpoint(),{}});
    const auto routeBytes=EncodeRunPayload({run.Checkpoint(),{}});
    if(routeBytes) {
        auto corrupt=*routeBytes;corrupt.back()=2;check(!DecodeRunPayload(corrupt),"不正な真偽値拒否");
        corrupt=*routeBytes;for(int i=40;i<44;++i) corrupt[i]=static_cast<char>(255);
        check(!DecodeRunPayload(corrupt),"過大な配列長を確保前に拒否");
    }
    check(!DecodeRunPayload(std::string(16*1024*1024+1,'x')),"過大payload拒否");
    std::cout<<"SavePayload: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
