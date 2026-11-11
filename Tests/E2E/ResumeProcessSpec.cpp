#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Presentation/RunScreen.h"
#include "Infrastructure/DiskSlotFiles.h"
#include "Application/Encounter.h"
#include "AI/BasicTurn.h"
#include "Domain/ComboCatalog.h"
#include "Domain/EnemyCatalog.h"
#include "Domain/CharacterCatalog.h"
#include "../Fixtures/ComboCatalogCases.h"
#include "../Fixtures/PersonalityCase.h"
#include "../Fixtures/EnemyThinkingCase.h"
#include <algorithm>
#include <atomic>
#include <fstream>
#include <iterator>

namespace {
class ProcessResumeFiles : public GirlsSpire::DiskSlotFiles {
public:
    explicit ProcessResumeFiles(const std::filesystem::path& root):DiskSlotFiles(root){}
    std::atomic_uint32_t LastWriteError=0;
    std::atomic_int UnexpectedWrites=0;
    bool Replace(GirlsSpire::SaveKind kind,bool backup,std::string_view bytes) override {
        if(DiskSlotFiles::Replace(kind,backup,bytes))return true;
        LastWriteError=FPlatformMisc::GetLastError();++UnexpectedWrites;return false;
    }
    bool ReplaceJournal(std::string_view bytes) override {
        if(DiskSlotFiles::ReplaceJournal(bytes))return true;
        LastWriteError=FPlatformMisc::GetLastError();++UnexpectedWrites;return false;
    }
};
struct ProcessResumeFixture {
    GirlsSpire::RunSaveState State;
    GirlsSpire::Profile Permanent;
};
std::optional<ProcessResumeFixture> ProcessFixture(int point)
{
    if(point==58 || point==59){
        auto fixture=ProcessFixture(0);if(!fixture)return std::nullopt;
        fixture->Permanent.HighestDifficulty=point==58?0:2;fixture->Permanent.Bonds.fill(42);fixture->Permanent.DiscoveredCombos={2};
        return fixture;
    }
    if(point==56 || point==57){
        using namespace GirlsSpire;Run run(42,static_cast<Difficulty>(point-56),HeroRole::Sword);
        for(int step=0;step<24 && run.Snapshot().Phase==RunPhase::Route;++step){run.ChooseNode(run.Snapshot().Revision,0);run.CompleteEvent(run.Snapshot().Revision);}
        if(run.Snapshot().Phase!=RunPhase::Won || run.ProfileState().HighestDifficulty!=point-55)return std::nullopt;
        return ProcessResumeFixture{{run.Checkpoint(),{}},run.ProfileState()};
    }
    if(point==55){
        using namespace GirlsSpire;
        const auto path=std::filesystem::path(u8"" __FILE__).parent_path().parent_path()/"Fixtures"/"enemy-v18.bin";
        std::ifstream input(path,std::ios::binary);const std::string bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
        const auto state=DecodeRunPayload(bytes);if(!state || !state->Combat)return std::nullopt;
        Profile profile;profile.Bonds=state->Combat->Battle.State.StartingBonds;
        return ProcessResumeFixture{*state,profile};
    }
    if(point==54){
        using namespace GirlsSpire;Profile profile;profile.Bonds.fill(55);
        Run run(42,Difficulty::Beginner,HeroRole::Sword,profile);auto progress=run.Checkpoint();progress.State.Day=5;progress.State.Phase=RunPhase::Event;progress.State.Active={NodeKind::Battle};progress.State.Choices.clear();
        BattleUnit hero;hero.At={2,1};hero.Speed=50;hero.Hp=hero.MaxHp=100;
        auto caster=CreateCharacter(2,{1,1});caster.Side=Faction::Enemy;caster.Speed=40;
        BattleUnit ally;ally.Id=100;ally.Side=Faction::Enemy;ally.At={1,2};ally.Speed=10;ally.Hp=40;ally.MaxHp=100;ally.Attack=0;ally.Movement=0;
        Battle arena(8,8,std::vector<Tile>(64),{hero,caster,ally},42);auto checkpoint=arena.Checkpoint();checkpoint.State.AbilityRules=AbilityRuleSet::CompleteCharacters;
        checkpoint.State.StartingBonds=profile.Bonds;checkpoint.State.EnemyAI=EnemyThinking::Basic;checkpoint.State.EnemyCharacterSkills=true;
        CombatControl control;control.ProfileDriven=true;control.ChoiceRandom=Random(42);for(int id=1;id<=8;++id)control.Trust[id]=55;
        return ProcessResumeFixture{RunSaveState{progress,SavedCombat{checkpoint,control.Checkpoint()}},profile};
    }
    if(point>=50){GirlsSpire::Profile profile;profile.Bonds.fill(75);return ProcessResumeFixture{GirlsSpire::TestData::EnemyThinkingCase(static_cast<GirlsSpire::EnemyThinking>(point==53?0:point-49)),profile};}
    if(point>=48){GirlsSpire::Profile profile;profile.Bonds.fill(75);return ProcessResumeFixture{GirlsSpire::TestData::PersonalityCase(point==49),profile};}
    using namespace GirlsSpire;
    if(point>=46){
        Profile profile;profile.Bonds={0,24,49,50,74,75,99,100};
        Run run(42,Difficulty::Beginner,HeroRole::Sword,profile);run.ChooseNode(0,0);auto progress=run.Checkpoint();progress.State.Health[0].Hp=30;
        auto restored=Run::Restore(progress,profile);if(!restored)return std::nullopt;
        auto battle=Encounter::Create(restored->Snapshot(),restored->RandomState(),profile);
        if(point==47 && !battle.UseSkill(battle.Snapshot().Revision,0,Skill::FirstAid,battle.Snapshot().Units.front().At))return std::nullopt;
        CombatControl control;control.ProfileDriven=true;control.ChoiceRandom=Random(battle.RandomState());for(int id=1;id<=8;++id)control.Trust[id]=profile.Bonds[id-1];
        return ProcessResumeFixture{RunSaveState{progress,SavedCombat{battle.Checkpoint(),control.Checkpoint()}},profile};
    }
    if(point>=44){
        auto fixture=ProcessFixture(30);if(!fixture)return std::nullopt;auto& checkpoint=fixture->State.Combat->Battle;
        checkpoint.State.AbilityRules=AbilityRuleSet::CompleteCharacters;checkpoint.State.StartingBonds.fill(100);fixture->Permanent.Bonds.fill(100);for(int id=1;id<=8;++id)fixture->State.Combat->Control.Trust[id]=100;
        checkpoint.State.Acted=false;checkpoint.State.UsedSkill=-1;checkpoint.State.LastCombo.reset();checkpoint.State.Combos={1,0,1,0,{{1,0,Continuation,{100}}}};checkpoint.State.Revision=1;checkpoint.LastHits.clear();
        if(point==45){auto battle=Battle::Restore(checkpoint);if(!battle || !battle->Attack(battle->Snapshot().Revision,1,checkpoint.State.Units.back().At) || !battle->Snapshot().LastCombo || battle->Snapshot().LastCombo->RecipeId!=201)return std::nullopt;checkpoint=battle->Checkpoint();if(!DiscoverCombo(fixture->Permanent,201))return std::nullopt;}
        return fixture;
    }
    if(point>=40){
        auto fixture=ProcessFixture(38);if(!fixture)return std::nullopt;auto& checkpoint=fixture->State.Combat->Battle;checkpoint.State.AbilityRules=AbilityRuleSet::CharacterAbilities;checkpoint.State.Combos.Gauge=50;checkpoint.State.StartingBonds.fill(100);fixture->Permanent.Bonds.fill(100);for(int id=1;id<=8;++id)fixture->State.Combat->Control.Trust[id]=100;
        if(point>=42){auto& units=checkpoint.State.Units;units[1].Id=4;fixture->State.Run.State.Recruited={3};checkpoint.State.ActiveId=4;for(auto& id:checkpoint.State.Order)if(id==8)id=4;units[2].At={2,2};units[2].Hp=units[2].MaxHp=500;}
        auto battle=Battle::Restore(checkpoint);if(!battle)return std::nullopt;
        if(point%2){const auto at=point>=42?checkpoint.State.Units[2].At:checkpoint.State.Units[1].At;if(!battle->UseUltimate(battle->Snapshot().Revision,point>=42?4:8,at))return std::nullopt;checkpoint=battle->Checkpoint();}
        return fixture;
    }
    if(point>=38){
        Profile profile;profile.Bonds.fill(80);Run run(42,Difficulty::Beginner,HeroRole::Sword,profile);auto progress=run.Checkpoint();
        progress.State.Day=5;progress.State.Phase=RunPhase::Event;progress.State.Active={NodeKind::Battle};progress.State.Choices.clear();progress.State.Recruited={7};
        BattleUnit hero;hero.At={1,1};hero.Hp=50;hero.MaxHp=80;hero.Speed=20;hero.AttackCursed=true;hero.MovementWebbed=true;
        BattleUnit sayo;sayo.Id=8;sayo.At={1,2};sayo.Hp=sayo.MaxHp=40;sayo.Speed=30;sayo.Skills={true,true};
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={4,2};enemy.Speed=10;
        Battle arena(8,8,std::vector<Tile>(64),{hero,sayo,enemy},42);auto checkpoint=arena.Checkpoint();checkpoint.State.AbilityRules=AbilityRuleSet::CharacterSkills;checkpoint.State.StartingBonds=profile.Bonds;
        auto battle=Battle::Restore(checkpoint);if(!battle)return std::nullopt;
        if(point==39 && !battle->UseSkill(battle->Snapshot().Revision,8,Skill::PowerStrike,hero.At))return std::nullopt;
        CombatControl control;control.ProfileDriven=true;for(int id=1;id<=8;++id)control.Trust[id]=80;
        return ProcessResumeFixture{{progress,SavedCombat{battle->Checkpoint(),control.Checkpoint()}},profile};
    }
    if(point>=36) {
        auto fixture=ProcessFixture(34);if(!fixture)return std::nullopt;
        auto& checkpoint=fixture->State.Combat->Battle;
        checkpoint.State.Units[0].AttackBoost={6,true};checkpoint.State.Units[0].DefenseBoost={4,true};checkpoint.State.Units[0].AccuracyBoost={15,true};
        if(point==37){auto battle=Battle::Restore(checkpoint);if(!battle || !battle->EndTurn(battle->Snapshot().Revision,0,Facing::South))return std::nullopt;checkpoint=battle->Checkpoint();}
        return fixture;
    }
    if(point>=34) {
        auto fixture=ProcessFixture(10+point-34);if(!fixture || !fixture->State.Combat)return std::nullopt;
        auto& combat=*fixture->State.Combat;
        combat.Battle.State.AbilityRules=AbilityRuleSet::Character;combat.Control.ProfileDriven=true;
        for(int id=0;id<8;++id){
            const int rank=point==34?((id%2)?100:0):10*(id+1);
            combat.Battle.State.StartingBonds[id]=rank;combat.Control.Trust[id+1]=rank;
        }
        // 現在の恒久絆から戦闘開始時絆を再計算しないことも検証する。
        fixture->Permanent.Bonds.fill(42);return fixture;
    }
    Profile profile;profile.Bonds.fill(25+point);
    if(point%2)profile.DiscoveredCombos={2,3,5};
    for(int i=0;i<8;++i)profile.SpecialConversations[i]=(point&(1<<(i%4)))!=0;
    for(int i=0;i<9;++i)profile.Epilogues[i]=(point&(1<<(i%5)))!=0;
    Run run(42+point,static_cast<Difficulty>(point%3),static_cast<HeroRole>(point%4),profile);
    if(point>=32) {
        profile.Bonds.fill(75);profile.DiscoveredCombos.clear();
        for(const auto& recipe:ComboCatalog())if(recipe.Id!=102)profile.DiscoveredCombos.push_back(recipe.Id);
        auto progress=run.Checkpoint();progress.State.Day=5;progress.State.Phase=RunPhase::Event;progress.State.Role=HeroRole::Matchlock;
        progress.State.Active={NodeKind::Battle};progress.State.Choices.clear();progress.State.Recruited={4};
        BattleUnit hero;hero.At={1,1};hero.Hp=hero.MaxHp=80;hero.Speed=20;hero.Role=BattleRole::Gunner;hero.Range=5;hero.Accuracy=200;
        auto ally=hero;ally.Id=5;ally.At={2,1};ally.Hp=ally.MaxHp=40;ally.Speed=15;ally.Role=BattleRole::Archer;ally.Range=4;
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={1,2};enemy.Hp=enemy.MaxHp=1000;enemy.Speed=0;enemy.Attack=0;enemy.Movement=0;
        auto second=enemy;second.Id=101;second.At={2,2};
        Battle battle(8,8,std::vector<Tile>(64),{hero,ally,enemy,second},TestData::ComboWinningSeed());
        if(!battle.Attack(0,0,enemy.At) || !battle.EndTurn(battle.Snapshot().Revision,0,Facing::South))return std::nullopt;
        if(point==33) {
            if(!battle.Attack(battle.Snapshot().Revision,5,second.At) || !battle.Snapshot().LastCombo || battle.Snapshot().LastCombo->RecipeId!=102 || !DiscoverCombo(profile,102))return std::nullopt;
        }
        CombatControl control;control.ProfileDriven=true;control.Trust[5]=75;
        return ProcessResumeFixture{{progress,SavedCombat{battle.Checkpoint(),control.Checkpoint()}},profile};
    }
    if(point>=30) {
        profile.Bonds.fill(75);profile.DiscoveredCombos={3,5};
        auto progress=run.Checkpoint();progress.State.Day=5;progress.State.Phase=RunPhase::Event;
        progress.State.Active={NodeKind::Battle};progress.State.Choices.clear();progress.State.Recruited={0};
        BattleUnit hero;hero.At={1,1};hero.Hp=hero.MaxHp=80;hero.Speed=20;
        auto ally=hero;ally.Id=1;ally.At={2,1};ally.Hp=ally.MaxHp=40;ally.Speed=15;ally.Range=8;ally.Accuracy=200;
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={6,1};enemy.Hp=enemy.MaxHp=1000;enemy.Speed=0;enemy.Attack=0;enemy.Movement=0;
        Battle battle(8,8,std::vector<Tile>(64),{hero,ally,enemy},0);
        if(!battle.EndTurn(0,0,Facing::South))return std::nullopt;
        if(point==31) {
            if(!battle.Attack(battle.Snapshot().Revision,1,enemy.At) || !battle.Snapshot().LastCombo || battle.Snapshot().LastCombo->RecipeId!=2)return std::nullopt;
            if(!DiscoverCombo(profile,2))return std::nullopt;
        }
        CombatControl control;control.ProfileDriven=true;control.Trust[1]=75;
        return ProcessResumeFixture{{progress,SavedCombat{battle.Checkpoint(),control.Checkpoint()}},profile};
    }
    if(point>=26) {
        auto progress=run.Checkpoint();progress.State.Day=12;progress.State.Phase=RunPhase::Event;
        progress.State.Active={NodeKind::FinalBoss};progress.State.Choices.clear();progress.State.Recruited={0,1,2,3};
        auto finalRun=Run::Restore(progress,profile);if(!finalRun)return std::nullopt;
        auto checkpoint=Encounter::Create(finalRun->Snapshot(),point>=28?42:finalRun->RandomState()).Checkpoint();
        checkpoint.State.Tiles.assign(checkpoint.State.Tiles.size(),Tile{});
        checkpoint.State.Round=3;checkpoint.State.Combos.Round=3;
        if(point>=28) {
            for(auto& unit:checkpoint.State.Units) {
                if(unit.Side==Faction::Player)unit.Speed=30;
                if(unit.Id==100)unit.Speed=0;
            }
            std::stable_sort(checkpoint.State.Order.begin(),checkpoint.State.Order.end(),[&](int left,int right) {
                const auto find=[&](int id)->const BattleUnit& {return *std::find_if(checkpoint.State.Units.begin(),checkpoint.State.Units.end(),[id](const BattleUnit& unit){return unit.Id==id;});};
                const auto& a=find(left);const auto& b=find(right);
                if(a.Speed!=b.Speed)return a.Speed>b.Speed;
                if(a.Side!=b.Side)return a.Side==Faction::Player;
                return a.Id<b.Id;
            });
        }
        auto battle=Battle::Restore(checkpoint);if(!battle)return std::nullopt;
        if(point==27) {
            int guard=0;while(battle->Snapshot().ActiveId!=100 && guard++<10)
                if(!battle->EndTurn(battle->Snapshot().Revision,battle->Snapshot().ActiveId,Facing::South))return std::nullopt;
            if(battle->Snapshot().Units.back().SummonerId!=100)return std::nullopt;
        }
        if(point>=28) {
            int guard=0;
            while(battle->Snapshot().ActiveId==0 || std::any_of(battle->Snapshot().Units.begin(),battle->Snapshot().Units.end(),[&](const BattleUnit& unit){return unit.Id==battle->Snapshot().ActiveId && unit.Side==Faction::Player;})) {
                if(++guard>10 || !battle->EndTurn(battle->Snapshot().Revision,battle->Snapshot().ActiveId,Facing::South))return std::nullopt;
            }
            if(battle->Snapshot().ActiveId<1 || battle->Snapshot().ActiveId>8)return std::nullopt;
            if(point==29 && !ApplyBasicAction(*battle,ChooseBasicAction(*battle)))return std::nullopt;
        }
        CombatControl control;control.ProfileDriven=true;control.ChoiceRandom=Random(999+point);
        for(int id=1;id<=8;++id)control.Trust[id]=profile.Bonds[id-1];
        return ProcessResumeFixture{{finalRun->Checkpoint(),SavedCombat{battle->Checkpoint(),control.Checkpoint()}},profile};
    }
    if(point<10) {
        if(point<5) {
            if(point>0) run.ChooseNode(run.Snapshot().Revision,0);
            if(point==4) {
                auto battle=Encounter::Create(run.Snapshot(),run.RandomState()).Snapshot();battle.Outcome=BattleOutcome::Won;
                run.CompleteBattle(run.Snapshot().Revision,battle);
            } else if(point>1)run.CompleteBattle(run.Snapshot().Revision,BattleOutcome::Won);
            if(point>2) run.ChooseRecipient(run.Snapshot().Revision,0);
            if(point>3) run.ClaimReward(run.Snapshot().Revision,0);
            return ProcessResumeFixture{{run.Checkpoint(),{}},profile};
        }
        const NodeKind targets[]={NodeKind::Battle,NodeKind::Social,NodeKind::Shop,NodeKind::Rest,NodeKind::Anomaly};
        const auto target=targets[point-5];
        for(int seed=0;seed<300;++seed) {
            Run candidate(seed,static_cast<Difficulty>(point%3),static_cast<HeroRole>(point%4),profile);
            for(int event=0;event<24 && candidate.Snapshot().Phase==RunPhase::Route;++event) {
                const auto& choices=candidate.Snapshot().Choices;int selected=0;
                for(int i=0;i<static_cast<int>(choices.size());++i) if(choices[i].Kind==target) selected=i;
                candidate.ChooseNode(candidate.Snapshot().Revision,selected);
                if(candidate.Snapshot().Active.Kind==target) return ProcessResumeFixture{{candidate.Checkpoint(),{}},profile};
                candidate.CompleteEvent(candidate.Snapshot().Revision);
            }
        }
        return std::nullopt;
    }
    run.ChooseNode(run.Snapshot().Revision,0);
    auto checkpoint=Encounter::Create(run.Snapshot(),run.RandomState()).Checkpoint();
    for(auto& unit:checkpoint.State.Units) if(unit.Side==Faction::Enemy) {
        unit.Hp=unit.MaxHp=1000;
        if(point>=15){unit.CreatesCover=true;unit.Defense=8;}
        if(point>=18){unit.CreatesCover=false;unit.Role=BattleRole::Cavalry;unit.Movement=5;unit.Range=1;unit.At={6,1};}
        if(point==16 || point==17){unit.CreatesCover=false;unit.Role=BattleRole::Ninja;unit.Movement=4;unit.Range=1;unit.At={2,1};}
        if(point==13 || point==14){unit.CreatesCover=false;unit.Role=BattleRole::Oni;unit.Movement=2;unit.Range=1;unit.Attack=16;unit.Skills[0]=true;unit.At={2,1};}
        if(point==11 || point==12){unit.CreatesCover=false;unit.Role=BattleRole::CurseMonk;unit.Movement=2;unit.Range=3;unit.Attack=10;unit.Accuracy=200;unit.At={2,1};}
        if(point>=20){unit.CreatesCover=false;unit.Role=BattleRole::Fox;unit.Movement=3;unit.Range=3;unit.Attack=8;unit.Hp=20;unit.Skills={false,true};unit.At={2,1};}
        if(point>=22){unit.Role=BattleRole::Spider;unit.Movement=2;unit.Attack=10;unit.Hp=unit.MaxHp;unit.Skills={};unit.Accuracy=200;}
        if(point>=24){unit.Role=BattleRole::FallenPrincess;unit.Range=2;unit.Defense=4;unit.Speed=12;unit.Attack=18;unit.MaxHp=160;unit.Hp=point==24?81:80;unit.At={3,1};}
    }
    if(point==11 || point==12 || point==13 || point==14 || point==16 || point==17 || point>=20)checkpoint.State.Tiles.assign(checkpoint.State.Tiles.size(),Tile{});
    if(point==11 || point==12 || point>=22)checkpoint.RandomState=42;
    auto battle=Battle::Restore(checkpoint);if(!battle) return std::nullopt;
    if(point>=24) {
        if(!battle->EndTurn(battle->Snapshot().Revision,0,Facing::South) || battle->NormalAttackRadius(100)!=(point==25?1:0))return std::nullopt;
    } else if(point>=22) {
        if(!battle->EndTurn(battle->Snapshot().Revision,0,Facing::South) ||
           !battle->Attack(battle->Snapshot().Revision,100,{1,1}) || !battle->Snapshot().Units[0].MovementWebbed)return std::nullopt;
        if(point==23 && (!battle->EndTurn(battle->Snapshot().Revision,100,Facing::South) ||
           !battle->EndTurn(battle->Snapshot().Revision,0,Facing::South) || battle->Snapshot().Units[0].MovementWebbed))return std::nullopt;
    } else if(point>=20) {
        if(!battle->EndTurn(battle->Snapshot().Revision,0,Facing::South))return std::nullopt;
        const auto recovery=ChooseBasicAction(*battle);
        if(recovery.Kind!=BasicActionKind::UseSkill || recovery.Ability!=Skill::FirstAid || !ApplyBasicAction(*battle,recovery))return std::nullopt;
        if(point==21 && (!battle->EndTurn(battle->Snapshot().Revision,100,Facing::South) ||
           !battle->EndTurn(battle->Snapshot().Revision,0,Facing::South) || ChooseBasicAction(*battle).Kind!=BasicActionKind::Attack))return std::nullopt;
    } else if(point==11 || point==12) {
        if(!battle->EndTurn(battle->Snapshot().Revision,0,Facing::South) ||
           !battle->Attack(battle->Snapshot().Revision,100,{1,1}) || !battle->Snapshot().Units[0].AttackCursed)return std::nullopt;
        if(point==12 && (!battle->EndTurn(battle->Snapshot().Revision,100,Facing::South) ||
           !battle->EndTurn(battle->Snapshot().Revision,0,Facing::South) || battle->Snapshot().Units[0].AttackCursed))return std::nullopt;
    } else if(point==13 || point==14) {
        if(!battle->EndTurn(battle->Snapshot().Revision,0,Facing::South))return std::nullopt;
        const auto strike=ChooseBasicAction(*battle);
        if(strike.Kind!=BasicActionKind::UseSkill || !ApplyBasicAction(*battle,strike))return std::nullopt;
        if(point==14 && (!battle->EndTurn(battle->Snapshot().Revision,100,Facing::South) ||
           !battle->EndTurn(battle->Snapshot().Revision,0,Facing::South)))return std::nullopt;
    } else if(point==16 || point==17) {
        if(!battle->EndTurn(battle->Snapshot().Revision,0,Facing::South) ||
           !battle->Attack(battle->Snapshot().Revision,100,{1,1}))return std::nullopt;
        if(point==17) {
            const auto retreat=ChooseBasicAction(*battle);
            if(retreat.Kind!=BasicActionKind::Move || !ApplyBasicAction(*battle,retreat))return std::nullopt;
        }
    } else if(point>=18) {
        if(!battle->EndTurn(battle->Snapshot().Revision,0,Facing::South) ||
           !battle->Move(battle->Snapshot().Revision,100,{2,1}) || !battle->Snapshot().ChargeReady)return std::nullopt;
        if(point==19 && (!battle->Attack(battle->Snapshot().Revision,100,{1,1}) || battle->Snapshot().ChargeReady))return std::nullopt;
    } else for(int action=0;action<point-10;++action) if(!ApplyBasicAction(*battle,ChooseBasicAction(*battle))) return std::nullopt;
    CombatControl control;control.ProfileDriven=true;control.ChoiceRandom=Random(999+point);
    for(int id=1;id<=8;++id) control.Trust[id]=profile.Bonds[id-1];
    return ProcessResumeFixture{{run.Checkpoint(),SavedCombat{battle->Checkpoint(),control.Checkpoint()}},profile};
}
}
BEGIN_DEFINE_SPEC(FGirlsSpireResumeProcessSpec,"GirlsSpire.UI.SaveProcess",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
END_DEFINE_SPEC(FGirlsSpireResumeProcessSpec)
void FGirlsSpireResumeProcessSpec::Define()
{
    FString stage,root;
    if(!FParse::Value(FCommandLine::Get(),TEXT("GirlsSpireResumeStage="),stage)) return;
    FParse::Value(FCommandLine::Get(),TEXT("GirlsSpireResumeRoot="),root);
    root=FPaths::ConvertRelativePathToFull(root);
    const auto prefix=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Tests/ProcessResume"));
    const bool validRoot=root!=prefix && FPaths::IsUnderDirectory(root,prefix);
    BeforeEach([this] {
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        IConsoleManager::Get().ProcessUserConsoleInput(TEXT("GirlsSpire.Preview"),*GLog,nullptr);
        for(const auto& w:FSlateApplication::Get().GetTopLevelWindows()) if(w->GetTitle().ToString()==TEXT("Spiral RPG — 周回プレビュー")) Window=w;
        Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    It(stage==TEXT("Save")?TEXT("60地点を別プロセス用に保存する"):TEXT("別プロセスで60地点を画面から消費再開する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(240),[this,stage,root,validRoot] {
        using namespace GirlsSpire;
        if(!TestTrue(TEXT("専用試験保存先"),validRoot) || !TestTrue(TEXT("有効な試験段階"),stage==TEXT("Save") || stage==TEXT("Verify"))) return;
        for(int point=0;point<60;++point) {
            auto fixture=ProcessFixture(point);if(fixture && fixture->State.Combat)DiscoverEnemies(fixture->Permanent,fixture->State.Combat->Battle.State.Units);if(!TestTrue(TEXT("決定論的な再開地点"),fixture.has_value())) return;
            const auto directory=root/FString::Printf(TEXT("point-%02d"),point);
            auto files=std::make_shared<ProcessResumeFiles>(std::filesystem::path(*directory));
            auto service=std::make_shared<SaveTransaction>(*files,*files);
            if(stage==TEXT("Save")) {
                if(!TestTrue(TEXT("保存準備と確定"),service->Prepare(fixture->State,fixture->Permanent)==SlotIssue::None && service->Retry()==SlotIssue::None)) return;
                if(point==58 || point==59){
                    auto legacy=EncodeProfilePayload(fixture->Permanent);if(!legacy)return;
                    // v7の外見37バイトと、v6の空の敵一覧4バイトを除く。
                    legacy->resize(legacy->size()-41);(*legacy)[4]=5;
                    const auto bytes=EncodeSave({SaveKind::Profile,service->Load().Generation,*legacy});
                    if(!TestTrue(TEXT("前プロセスで旧v5プロフィール原本を保存"),bytes && files->Replace(SaveKind::Profile,false,*bytes) && files->Replace(SaveKind::Profile,true,*bytes)))return;
                }
                continue;
            }
            const auto saved=service->Load();
            if(!TestTrue(TEXT("前プロセスの全状態を復元"),saved.Run==fixture->State && saved.Permanent==fixture->Permanent)) return;
            Async(EAsyncExecution::TaskGraphMainThread,[this,service]{Window->SetContent(SNew(SRunScreen).SaveService(service));}).Wait();
            if(point==45){
                const auto read=[&](const char* key){const auto element=Driver->FindElement(By::Id(key));if(!element->IsVisible()){const auto scroll=Driver->FindElement(By::Id("run.scroll"));scroll->ScrollToBeginning();DrainSlateLayout();if(!element->IsVisible()){scroll->ScrollToEndUntil(By::Id(key));DrainSlateLayout();}}return TestTrue(UTF8_TO_TCHAR(key),ClickAndDrain(element));};
                if(!read("title.combo.library") || !read("combo.library.entry.201"))return;
                const auto recipe=FindComboRecipe(201);
                if(!TestTrue(TEXT("前プロセスで発見した専用連携を同じ条件で再読"),recipe && Driver->FindElement(By::Id("combo.library.reason"))->GetText().ToString()==UTF8_TO_TCHAR(recipe->Reason.data())))return;
                if(!read("combo.library.list") || !read("combo.library.back"))return;
                if(!read("title.enemy.library"))return;
                const int enemy=fixture->Permanent.DiscoveredEnemies.front();
                const auto key=FString::Printf(TEXT("enemy.library.entry.%d"),enemy);if(!read(TCHAR_TO_UTF8(*key)))return;
                const auto definition=FindEnemyDefinition(enemy);
                if(!TestTrue(TEXT("別プロセスの発見敵を共有定義で再読"),definition && Driver->FindElement(By::Id("enemy.library.description"))->GetText().ToString()==UTF8_TO_TCHAR(definition->Description.data())))return;
                if(!read("enemy.library.list") || !read("enemy.library.back"))return;
                const auto after=service->Load();
                if(!TestTrue(TEXT("閲覧では中断を消費せず保存世代も維持"),after.Run==saved.Run && after.Permanent==saved.Permanent && after.Generation==saved.Generation))return;
            }
            if(!TestTrue(TEXT("画面の再開操作"),ClickAndDrain(Driver->FindElement(By::Id("title.resume"))))) return;
            if(Driver->FindElement(By::Id("save.error"))->Exists() && files->UnexpectedWrites.load()>0) {
                const auto error=files->LastWriteError.load();
                AddInfo(FString::Printf(TEXT("再開地点%dで実IO拒否%u、固定取引を保持してUIから再試行。保存先=%s"),point,error,*directory));
                if(!TestTrue(TEXT("OS共有制約で進行を止める"),(error==5 || error==32) && service->HasPending()) ||
                   !TestTrue(TEXT("再開の元データと取引を保全"),files->Preserve(SaveKind::Run) && files->Preserve(SaveKind::Profile) && files->PreserveJournal()))return;
                for(int retry=0;retry<2 && Driver->FindElement(By::Id("save.error"))->Exists();++retry)
                    if(!TestTrue(TEXT("再開結果を画面から明示再試行"),ClickAndDrain(Driver->FindElement(By::Id("save.retry")))))return;
                if(!TestTrue(TEXT("再試行で固定取引を完了"),!service->HasPending() && !Driver->FindElement(By::Id("save.error"))->Exists()))return;
            }
            auto expected=fixture->State;expected.ResumeAvailable=false;auto expectedPermanent=fixture->Permanent;
            // 敵手番で保存された地点は、復元後に通常の敵行動を継続する。
            if(expected.Combat) {
                auto battle=Battle::Restore(expected.Combat->Battle);if(!TestTrue(TEXT("戦闘復元"),battle.has_value())) return;
                for(int step=0;step<384 && battle->Snapshot().Outcome==BattleOutcome::Ongoing;++step) {
                    const auto& state=battle->Snapshot();
                    const auto actor=std::find_if(state.Units.begin(),state.Units.end(),[&](const BattleUnit& unit){return unit.Id==state.ActiveId;});
                    if(actor->Side==Faction::Player) break;
                    if(!ApplyBasicAction(*battle,ChooseBasicAction(*battle))) break;
                    DiscoverEnemies(expectedPermanent,battle->Snapshot().Units);
                }
                expected.Combat->Battle=battle->Checkpoint();
            }
            const auto resumed=service->Load();
            if(resumed.Run!=expected || resumed.Permanent!=expectedPermanent)
                AddInfo(FString::Printf(TEXT("再開地点%d不一致: Issue=%d Pending=%d Writes=%d OS=%u RunEqual=%d ProfileEqual=%d / %s"),point,static_cast<int>(resumed.Issue),service->HasPending(),files->UnexpectedWrites.load(),files->LastWriteError.load(),resumed.Run==expected,resumed.Permanent==expectedPermanent,*directory));
            if(!TestTrue(TEXT("非中断時の継続結果と一致"),resumed.Run==expected && resumed.Permanent==expectedPermanent)) return;
            if(!TestTrue(TEXT("二度目の中断ロード拒否"),service->PrepareResume()==SlotIssue::Invalid)) return;
            if(point==48 || point==49){
                const Position target=point==49?Position{3,1}:Position{1,3};
                const auto label=Driver->FindElement(By::Id("intent.forecast"))->GetText().ToString();
                if(!TestTrue(TEXT("別プロセスでも新旧の性格順位を維持"),label.StartsWith(FString::Printf(TEXT("確定予定\n通常攻撃(%d,%d)"),target.X,target.Y))))return;
                auto battle=*Battle::Restore(expected.Combat->Battle);battle.Attack(battle.Snapshot().Revision,3,target);expected.Combat->Battle=battle.Checkpoint();
                const auto execute=Driver->FindElement(By::Id("intent.execute"));
                if(!execute->IsVisible()){Driver->FindElement(By::Id("battle.scroll"))->ScrollToEndUntil(By::Id("intent.execute"));DrainSlateLayout();}
                if(!TestTrue(TEXT("再開後の予告を画面から実行"),ClickAndDrain(execute)) || !TestTrue(TEXT("再開後も効果/乱数/識別/恒久データを保存"),service->Load().Run==expected && service->Load().Permanent==fixture->Permanent))return;
            }
            if(point>=50 && point<54){
                auto battle=*Battle::Restore(expected.Combat->Battle);battle.EndTurn(battle.Snapshot().Revision,0,Facing::South);
                const auto next=ChooseBasicAction(battle);const bool preferNear=point==50 || point==52;
                if(!TestTrue(TEXT("別プロセスでも敵思考の標的を維持"),next.Kind==BasicActionKind::Attack && next.Target==(preferNear?Position{3,1}:Position{1,4})))return;
                for(int step=0;step<8 && battle.Snapshot().ActiveId==100;++step)if(!ApplyBasicAction(battle,ChooseBasicAction(battle)))break;
                expected.Combat->Battle=battle.Checkpoint();const auto wait=Driver->FindElement(By::Id("battle.wait.south"));
                if(!wait->IsVisible()){Driver->FindElement(By::Id("battle.scroll"))->ScrollToEndUntil(By::Id("battle.wait.south"));DrainSlateLayout();}
                if(!TestTrue(TEXT("再開後に敵手番へ進める"),ClickAndDrain(wait)) || !TestTrue(TEXT("難易度思考の継続結果と全状態/RNGを保存"),service->Load().Run==expected && service->Load().Permanent==fixture->Permanent))return;
            }
            if(point==54){
                auto battle=*Battle::Restore(expected.Combat->Battle);battle.EndTurn(battle.Snapshot().Revision,0,Facing::South);
                const auto heal=ChooseBasicAction(battle);
                if(!TestTrue(TEXT("別プロセスでも敵の正式回復技能を選ぶ"),heal.Kind==BasicActionKind::UseSkill && heal.Ability==Skill::PowerStrike && heal.Target==Position{1,2}))return;
                for(int step=0;step<16 && battle.Snapshot().ActiveId!=0;++step)if(!ApplyBasicAction(battle,ChooseBasicAction(battle)))break;
                expected.Combat->Battle=battle.Checkpoint();const auto wait=Driver->FindElement(By::Id("battle.wait.south"));
                if(!wait->IsVisible()){Driver->FindElement(By::Id("battle.scroll"))->ScrollToEndUntil(By::Id("battle.wait.south"));DrainSlateLayout();}
                if(!TestTrue(TEXT("再開後に敵技能を画面から進行"),ClickAndDrain(wait)) || !TestTrue(TEXT("敵技能の実結果と識別/RNGを保存"),service->Load().Run==expected && service->Load().Permanent==fixture->Permanent))return;
            }
            if(point==55){
                auto checkpoint=expected.Combat->Battle;checkpoint.State.ActiveId=2;checkpoint.State.Moved=false;checkpoint.State.Acted=false;checkpoint.State.UsedSkill=-1;checkpoint.State.ChargeReady=false;
                checkpoint.TurnIndex=static_cast<std::size_t>(std::find(checkpoint.State.Order.begin(),checkpoint.State.Order.end(),2)-checkpoint.State.Order.begin());
                const auto battle=Battle::Restore(checkpoint);const auto action=ChooseBasicAction(*battle);
                if(!TestTrue(TEXT("v18由来の戦闘を別プロセスでも旧敵技能AIで維持"),!checkpoint.State.EnemyCharacterSkills && action.Kind==BasicActionKind::Move && action.Target==Position{7,2}))return;
            }
            const auto marker=root/FString::Printf(TEXT("point-%02d.done"),point);
            if(!TestTrue(TEXT("成功記録"),FFileHelper::SaveStringToFile(TEXT("pass"),*marker))) return;
            Async(EAsyncExecution::TaskGraphMainThread,[this]{Window->SetContent(SNullWidget::NullWidget);}).Wait();
        }
    });
    It(stage==TEXT("Save")?TEXT("世代不一致原本を別プロセス用に保存する"):TEXT("別プロセスで共通世代から確認復旧し消費再開する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(180),[this,stage,root,validRoot]{
        using namespace GirlsSpire;
        if(!TestTrue(TEXT("復旧用の専用試験保存先"),validRoot))return;
        const auto fixture=ProcessFixture(0);if(!TestTrue(TEXT("復旧前の全状態"),fixture.has_value()))return;
        auto files=std::make_shared<ProcessResumeFiles>(std::filesystem::path(*(root/TEXT("generation-recovery"))));
        auto service=std::make_shared<SaveTransaction>(*files,*files);
        if(stage==TEXT("Save")){
            Run unilateral(987654321,Difficulty::Beginner,HeroRole::Sword,fixture->Permanent);
            const auto newer=*EncodeRunPayload(RunSaveState{unilateral.Checkpoint(),{}}),rp=*EncodeRunPayload(fixture->State),pp=*EncodeProfilePayload(fixture->Permanent);
            const std::array<std::string,4> originals={*EncodeSave({SaveKind::Run,3,newer}),*EncodeSave({SaveKind::Run,2,rp}),*EncodeSave({SaveKind::Profile,2,pp}),*EncodeSave({SaveKind::Profile,1,pp})};
            for(int i=0;i<4;++i)if(!TestTrue(TEXT("次プロセス用の世代不一致原本"),files->Replace(i<2?SaveKind::Run:SaveKind::Profile,(i%2)!=0,originals[i])))return;
            TestTrue(TEXT("元取引の確定世代"),files->ReplaceJournal(*EncodeSave({SaveKind::Run,3,{}})));return;
        }
        if(!TestTrue(TEXT("有効な復旧試験段階"),stage==TEXT("Verify")))return;
        Async(EAsyncExecution::TaskGraphMainThread,[this,service]{Window->SetContent(SNew(SRunScreen).SaveService(service));}).Wait();DrainSlateLayout();
        const auto click=[this](const char* id){
            const auto target=Driver->FindElement(By::Id(id));
            if(!target->IsVisible()){
                const auto scroll=Driver->FindElement(By::Id("run.scroll"));scroll->ScrollToBeginning();DrainSlateLayout();
                if(!target->IsVisible()){scroll->ScrollToEndUntil(By::Id(id));DrainSlateLayout();}
            }
            return TestTrue(*FString::Printf(TEXT("別プロセス復旧操作 %hs"),id),ClickAndDrain(target));
        };
        if(!click("save.generation.repair") || !click("save.generation.confirm"))return;
        const auto restored=service->Load();
        if(!TestTrue(TEXT("別プロセスでも共通世代の状態・乱数・恒久記録だけを使用"),restored.Generation==4 && restored.Run==fixture->State && restored.Permanent==fixture->Permanent))return;
        if(!click("title.resume"))return;
        auto expected=fixture->State;expected.ResumeAvailable=false;const auto resumed=service->Load();
        if(!TestTrue(TEXT("確認復旧後の消費再開も全状態一致"),resumed.Run==expected && resumed.Permanent==fixture->Permanent && service->PrepareResume()==SlotIssue::Invalid))return;
        TestTrue(TEXT("別プロセス復旧の成功記録"),FFileHelper::SaveStringToFile(TEXT("pass"),*(root/TEXT("generation-recovery.done"))));
    });
    It(stage==TEXT("Save")?TEXT("配色v20の固定外見を別プロセス用に保存する"):TEXT("別プロセスで配色v20を再開し固定外見のまま再中断する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this,stage,root,validRoot]{
        using namespace GirlsSpire;
        if(!TestTrue(TEXT("配色用の専用試験保存先"),validRoot) ||
           !TestTrue(TEXT("有効な配色試験段階"),stage==TEXT("Save") || stage==TEXT("Verify")))return;
        auto fixture=ProcessFixture(46);
        if(!TestTrue(TEXT("正式技能のプレイヤー手番を準備"),fixture && fixture->State.Combat && fixture->State.Combat->Battle.State.ActiveId==0))return;
        fixture->Permanent.VictoryRecorded=true;fixture->Permanent.SpecialConversations.fill(true);
        fixture->Permanent.SelectedAppearances.fill(AppearanceVariant::Standard);
        DiscoverEnemies(fixture->Permanent,fixture->State.Combat->Battle.State.Units);
        // 現在の恒久選択と異なる外見を保存し、再開時の上書きを検出する。
        fixture->State.Combat->Appearances.fill(AppearanceVariant::Alternate);
        const auto payload=EncodeRunPayload(fixture->State);
        if(!TestTrue(TEXT("配色保存はv20"),payload && static_cast<unsigned char>((*payload)[4])==20))return;
        const auto directory=root/TEXT("appearance");
        auto files=std::make_shared<ProcessResumeFiles>(std::filesystem::path(*directory));
        auto service=std::make_shared<SaveTransaction>(*files,*files);
        if(stage==TEXT("Save")){
            TestTrue(TEXT("次プロセス用に配色戦闘を実保存"),service->Prepare(fixture->State,fixture->Permanent)==SlotIssue::None && service->Retry()==SlotIssue::None);return;
        }
        const auto saved=service->Load();
        if(!TestTrue(TEXT("前プロセスの9人の固定衣装/全戦闘/乱数/恒久を復元"),saved.Issue==SlotIssue::None && saved.Run==fixture->State && saved.Permanent==fixture->Permanent))return;
        Async(EAsyncExecution::TaskGraphMainThread,[this,service]{Window->SetContent(SNew(SRunScreen).SaveService(service));}).Wait();DrainSlateLayout();
        if(!TestTrue(TEXT("配色戦闘を画面から再開"),ClickAndDrain(Driver->FindElement(By::Id("title.resume")))))return;
        auto expected=fixture->State;expected.ResumeAvailable=false;
        const auto resumed=service->Load();
        if(!TestTrue(TEXT("現在標準でも固定配色と戦闘/作戦/抽選状態を維持"),resumed.Run==expected && resumed.Permanent==fixture->Permanent && resumed.Generation>saved.Generation) ||
           !TestTrue(TEXT("再開権を二重消費しない"),service->PrepareResume()==SlotIssue::Invalid) ||
           !TestTrue(TEXT("再開した戦場に主人公画像を表示"),Driver->FindElement(By::Id("battle.portrait.image.0"))->Exists()))return;
        const auto suspend=Driver->FindElement(By::Id("save.suspend"));
        if(!suspend->IsVisible()){Driver->FindElement(By::Id("battle.scroll"))->ScrollToEndUntil(By::Id("save.suspend"));DrainSlateLayout();}
        if(!TestTrue(TEXT("配色を保持して画面から再中断"),ClickAndDrain(suspend)))return;
        DiskSlotFiles reopened{std::filesystem::path(*directory)};SaveTransaction reader(reopened,reopened);const auto suspended=reader.Load();
        if(!TestTrue(TEXT("別ストアでも中断権以外の全状態と9人の配色を保持"),suspended.Run==fixture->State && suspended.Permanent==fixture->Permanent && suspended.Generation>resumed.Generation))return;
        TestTrue(TEXT("配色の別プロセス成功記録"),FFileHelper::SaveStringToFile(TEXT("pass"),*(root/TEXT("appearance.done"))));
    });
    AfterEach([this] {
        Driver.Reset();if(Window.IsValid()) {Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());}
        Window.Reset();IAutomationDriverModule::Get().Disable();
    });
}
