#include "Infrastructure/SaveTransaction.h"
#include "Application/Encounter.h"
#include "AI/BasicTurn.h"
#include <array>
#include <iostream>

using namespace GirlsSpire;
namespace {
class ResumeMemory : public SlotFiles,public JournalFiles {
public:
    std::array<std::array<std::optional<std::string>,2>,2> Slots{};
    std::optional<std::string> Journal;
    bool Fail=false;int Writes=0;
    SlotBytes Read(SaveKind kind,bool backup) override {return {false,Slots[static_cast<int>(kind)-1][backup]};}
    SlotBytes ReadJournal() override {return {false,Journal};}
    bool Replace(SaveKind kind,bool backup,std::string_view bytes) override {
        ++Writes;if(Fail) return false;Slots[static_cast<int>(kind)-1][backup]=std::string(bytes);return true;
    }
    bool ReplaceJournal(std::string_view bytes) override {++Writes;if(Fail)return false;Journal=std::string(bytes);return true;}
};
}
int TestResumeConsume()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL ResumeConsume: "<<why<<'\n';}};
    Profile profile;profile.Bonds.fill(25);
    const auto verify=[&](const RunSaveState& checkpoint,bool fail) {
        ResumeMemory files;SaveTransaction first(files,files);
        check(first.Prepare(checkpoint,profile)==SlotIssue::None && first.Retry()==SlotIssue::None,"中断状態保存");
        SaveTransaction consumer(files,files);
        const auto loaded=consumer.Load();check(loaded.Run==checkpoint,"新しい所有者で保存状態を読める");
        check(consumer.PrepareResume()==SlotIssue::None && consumer.HasPending(),"再開より先に消費状態を固定");
        if(fail) {
            files.Fail=true;check(consumer.Retry()==SlotIssue::WriteFailed && consumer.HasPending(),"消費の保存失敗で再開を止める");
            check(consumer.PrepareResume()==SlotIssue::Pending,"失敗中にもう一度消費処理を作らない");files.Fail=false;
        }
        check(consumer.Retry()==SlotIssue::None,"消費をディスク確定できたときのみ再開可能");
        const auto consumed=consumer.Load();auto expected=checkpoint;expected.ResumeAvailable=false;
        check(consumed.Run==expected && consumed.Permanent==profile,"状態・乱数・恒久を保ち消費フラグだけを変える");
        const int writes=files.Writes;
        check(consumer.PrepareResume()==SlotIssue::Invalid && files.Writes==writes,"同じ中断を二度消費できない");
        SaveTransaction another(files,files);
        check(another.PrepareResume()==SlotIssue::Invalid && files.Writes==writes,"別の所有者でも二度目を拒否");
    };
    Run run(42,Difficulty::Beginner,HeroRole::Sword,profile);
    for(int point=0;point<10;++point) {
        verify({run.Checkpoint(),{}},point==3);
        run.ChooseNode(run.Snapshot().Revision,0);run.CompleteEvent(run.Snapshot().Revision);
    }
    Run combatRun(77,Difficulty::Beginner,HeroRole::Sword,profile);
    combatRun.ChooseNode(combatRun.Snapshot().Revision,0);
    auto initial=Encounter::Create(combatRun.Snapshot(),combatRun.RandomState()).Checkpoint();
    for(auto& unit:initial.State.Units) unit.Hp=unit.MaxHp=1000;
    auto battle=Battle::Restore(initial);check(battle.has_value(),"継続戦闘準備");
    if(battle) for(int point=0;point<10;++point) {
        CombatControl control;control.ProfileDriven=true;control.Trust.fill(25);control.ChoiceRandom=Random(100+point);
        verify({combatRun.Checkpoint(),SavedCombat{battle->Checkpoint(),control.Checkpoint()}},point==4);
        check(ApplyBasicAction(*battle,ChooseBasicAction(*battle)),"実行済み行動の次の地点へ進む");
    }
    ResumeMemory empty;SaveTransaction tx(empty,empty);check(tx.PrepareResume()==SlotIssue::Invalid && empty.Writes==0,"中断なしでは書き込まない");
    std::cout<<"ResumeConsume: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
