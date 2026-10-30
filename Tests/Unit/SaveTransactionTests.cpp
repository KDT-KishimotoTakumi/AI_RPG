#include "Infrastructure/SaveTransaction.h"
#include <array>
#include <iostream>

using namespace GirlsSpire;
namespace {
class TransactionMemory : public SlotFiles,public JournalFiles {
public:
    std::array<std::array<std::optional<std::string>,2>,2> Slots{};
    std::optional<std::string> Journal;
    int Calls=0,FailAt=0;
    bool FailAlways=false,ReadFailure=false;
    SlotBytes Read(SaveKind kind,bool backup) override {return {ReadFailure,Slots[static_cast<int>(kind)-1][backup]};}
    SlotBytes ReadJournal() override {return {ReadFailure,Journal};}
    bool Write(std::optional<std::string>& dst,std::string_view bytes) {
        if(++Calls==FailAt || FailAlways) return false;
        dst=std::string(bytes);return true;
    }
    bool Replace(SaveKind kind,bool backup,std::string_view bytes) override {return Write(Slots[static_cast<int>(kind)-1][backup],bytes);}
    bool ReplaceJournal(std::string_view bytes) override {return Write(Journal,bytes);}
};
}
int TestSaveTransaction()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL SaveTransaction: "<<why<<'\n';}};
    Run first(1,Difficulty::Beginner,HeroRole::Sword),second(2,Difficulty::Advanced,HeroRole::Matchlock);
    const RunSaveState a{first.Checkpoint(),{}},b{second.Checkpoint(),{}};
    Profile oldProfile,newProfile;newProfile.Bonds[0]=25;
    for(int failure=1;failure<=6;++failure) {
        TransactionMemory memory;SaveTransaction tx(memory,memory);
        check(tx.Load().RunIssue==SlotIssue::Empty,"初回は空");
        check(tx.Retry()==SlotIssue::NoPending,"準備なしの再試行拒否");
        check(tx.Prepare(a,oldProfile)==SlotIssue::None && tx.Retry()==SlotIssue::None,"初回の対で保存");
        memory.Calls=0;memory.FailAt=failure;
        check(tx.Prepare(b,newProfile)==SlotIssue::None,"次の結果固定");
        check(tx.Retry()==SlotIssue::WriteFailed && tx.HasPending(),"各書込段階の失敗保持");
        check(tx.Prepare(a,oldProfile)==SlotIssue::Pending && tx.Load().Issue==SlotIssue::Pending,"失敗中の別結果・ロード拒否");
        SaveTransaction restarted(memory,memory);const auto loaded=restarted.Load();
        check(loaded.Issue==SlotIssue::None && loaded.RunIssue==SlotIssue::None && loaded.ProfileIssue==SlotIssue::None,"再起動で未完了トランザクション復旧");
        check(loaded.Run==(failure==1?a:b) && loaded.Permanent==(failure==1?oldProfile:newProfile),"周回と恒久が同じ確定点になる");
        if(failure>1) check(loaded.Recovered,"ジャーナル再生を通知");
        memory.FailAt=0;check(tx.Retry()==SlotIssue::None && !tx.HasPending(),"同一内容の保存再試行");
        SaveTransaction finalRead(memory,memory);const auto result=finalRead.Load();
        check(result.Run==b && result.Permanent==newProfile,"最終的に両方が新しい内容");
        check(tx.Prepare({},newProfile)==SlotIssue::None && tx.Retry()==SlotIssue::None,"恒久を保ち周回だけ終了");
        const auto finished=tx.Load();check(!finished.Run && finished.Permanent==newProfile,"終了後の周回消失と恒久保持");
    }
    TransactionMemory memory;SaveTransaction tx(memory,memory);
    tx.Prepare(a,oldProfile);tx.Retry();
    const auto before=memory.Slots;
    memory.Journal="broken";check(tx.Load().Issue==SlotIssue::Corrupt,"破損ジャーナル拒否");
    check(tx.Prepare(b,newProfile)==SlotIssue::Corrupt && memory.Slots==before,"破損を無断上書きしない");
    memory.Journal.reset();memory.ReadFailure=true;check(tx.Load().Issue==SlotIssue::ReadFailed,"読込失敗報告");memory.ReadFailure=false;
    Profile invalid;invalid.Bonds[0]=101;check(tx.Prepare(a,invalid)==SlotIssue::Invalid,"不正恒久データ拒否");
    memory.Slots[1][0]="broken";memory.Slots[1][1]="broken";
    const auto partial=tx.Load();check(partial.Run==a && partial.ProfileIssue==SlotIssue::Corrupt,"正常周回を保ち恒久の破損を区別");
    check(tx.Prepare(b,newProfile)==SlotIssue::Corrupt,"片側破損を無断初期化しない");
    TransactionMemory missing;SaveTransaction initial(missing,missing);
    initial.Prepare(a,oldProfile);initial.Retry();missing.Slots[1][0].reset();missing.Slots[1][1].reset();
    check(initial.Load().Run==a && initial.Load().ProfileIssue==SlotIssue::Corrupt,"周回があるとき恒久ファイル欠落を初回と誤認しない");
    TransactionMemory concurrent;SaveTransaction oldWriter(concurrent,concurrent);
    oldWriter.Prepare(a,oldProfile);oldWriter.Retry();
    concurrent.Calls=0;concurrent.FailAt=1;oldWriter.Prepare(b,newProfile);oldWriter.Retry();
    concurrent.FailAt=0;SaveTransaction newer(concurrent,concurrent);
    newer.Prepare(b,newProfile);newer.Retry();newer.Prepare({},newProfile);newer.Retry();
    const auto latest=concurrent.Slots;const auto latestJournal=concurrent.Journal;
    check(oldWriter.Retry()==SlotIssue::Invalid && concurrent.Slots==latest && concurrent.Journal==latestJournal,"古い再試行が新しい保存を巻き戻さない");
    for(const auto& payload : {std::string("x"),std::string(4,static_cast<char>(255)),std::string(4,'\0')+"bad inner"}) {
        concurrent.Journal=EncodeSave({SaveKind::Run,4,payload});SaveTransaction badJournal(concurrent,concurrent);
        check(badJournal.Load().Issue==SlotIssue::Corrupt && concurrent.Slots==latest,"包絡が正常でも不正な取引内容は再生しない");
    }
    for(int failure=1;failure<=6;++failure) {
        TransactionMemory ending;SaveTransaction writer(ending,ending);writer.Prepare(a,oldProfile);writer.Retry();
        auto recorded=newProfile;recorded.SpecialConversations[0]=true;recorded.Epilogues[1]=true;
        ending.Calls=0;ending.FailAt=failure;writer.Prepare({},recorded);
        check(writer.Retry()==SlotIssue::WriteFailed,"後日談確定の各書込段階で失敗を通知");
        ending.FailAt=0;SaveTransaction reopened(ending,ending);const auto restored=reopened.Load();
        check(restored.Issue==SlotIssue::None && (failure==1?(restored.Run==a && restored.Permanent==oldProfile):(!restored.Run && restored.Permanent==recorded)),"終了途中の再起動は周回と後日談を同じ確定点へ復旧");
        check(writer.Retry()==SlotIssue::None && !writer.Load().Run && writer.Load().Permanent==recorded,"同じ後日談の再試行で終了が一度だけ成立");
    }
    std::cout<<"SaveTransaction: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
