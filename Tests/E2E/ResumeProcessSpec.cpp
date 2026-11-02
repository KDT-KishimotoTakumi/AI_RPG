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
#include "../Fixtures/ComboCatalogCases.h"
#include <algorithm>
#include <atomic>

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
    using namespace GirlsSpire;
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
        for(const auto& w:FSlateApplication::Get().GetTopLevelWindows()) if(w->GetTitle().ToString()==TEXT("Girls Spire — 周回プレビュー")) Window=w;
        Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    It(stage==TEXT("Save")?TEXT("44地点を別プロセス用に保存する"):TEXT("別プロセスで44地点を画面から消費再開する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(240),[this,stage,root,validRoot] {
        using namespace GirlsSpire;
        if(!TestTrue(TEXT("専用試験保存先"),validRoot) || !TestTrue(TEXT("有効な試験段階"),stage==TEXT("Save") || stage==TEXT("Verify"))) return;
        for(int point=0;point<44;++point) {
            const auto fixture=ProcessFixture(point);if(!TestTrue(TEXT("決定論的な再開地点"),fixture.has_value())) return;
            const auto directory=root/FString::Printf(TEXT("point-%02d"),point);
            auto files=std::make_shared<ProcessResumeFiles>(std::filesystem::path(*directory));
            auto service=std::make_shared<SaveTransaction>(*files,*files);
            if(stage==TEXT("Save")) {
                if(!TestTrue(TEXT("保存準備と確定"),service->Prepare(fixture->State,fixture->Permanent)==SlotIssue::None && service->Retry()==SlotIssue::None)) return;
                continue;
            }
            const auto saved=service->Load();
            if(!TestTrue(TEXT("前プロセスの全状態を復元"),saved.Run==fixture->State && saved.Permanent==fixture->Permanent)) return;
            Async(EAsyncExecution::TaskGraphMainThread,[this,service]{Window->SetContent(SNew(SRunScreen).SaveService(service));}).Wait();
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
            auto expected=fixture->State;expected.ResumeAvailable=false;
            // 敵手番で保存された地点は、復元後に通常の敵行動を継続する。
            if(expected.Combat) {
                auto battle=Battle::Restore(expected.Combat->Battle);if(!TestTrue(TEXT("戦闘復元"),battle.has_value())) return;
                for(int step=0;step<384 && battle->Snapshot().Outcome==BattleOutcome::Ongoing;++step) {
                    const auto& state=battle->Snapshot();
                    const auto actor=std::find_if(state.Units.begin(),state.Units.end(),[&](const BattleUnit& unit){return unit.Id==state.ActiveId;});
                    if(actor->Side==Faction::Player) break;
                    if(!ApplyBasicAction(*battle,ChooseBasicAction(*battle))) break;
                }
                expected.Combat->Battle=battle->Checkpoint();
            }
            const auto resumed=service->Load();
            if(resumed.Run!=expected || resumed.Permanent!=fixture->Permanent)
                AddInfo(FString::Printf(TEXT("再開地点%d不一致: Issue=%d Pending=%d Writes=%d OS=%u / %s"),point,static_cast<int>(resumed.Issue),service->HasPending(),files->UnexpectedWrites.load(),files->LastWriteError.load(),*directory));
            if(!TestTrue(TEXT("非中断時の継続結果と一致"),resumed.Run==expected && resumed.Permanent==fixture->Permanent)) return;
            if(!TestTrue(TEXT("二度目の中断ロード拒否"),service->PrepareResume()==SlotIssue::Invalid)) return;
            const auto marker=root/FString::Printf(TEXT("point-%02d.done"),point);
            if(!TestTrue(TEXT("成功記録"),FFileHelper::SaveStringToFile(TEXT("pass"),*marker))) return;
            Async(EAsyncExecution::TaskGraphMainThread,[this]{Window->SetContent(SNullWidget::NullWidget);}).Wait();
        }
    });
    AfterEach([this] {
        Driver.Reset();if(Window.IsValid()) {Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());}
        Window.Reset();IAutomationDriverModule::Get().Disable();
    });
}
