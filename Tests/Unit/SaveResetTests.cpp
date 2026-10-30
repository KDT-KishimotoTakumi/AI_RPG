#include "Infrastructure/SaveTransaction.h"
#include <array>
#include <iostream>
#include <limits>

using namespace GirlsSpire;
namespace {
class ResetMemory : public SlotFiles,public JournalFiles {
public:
    std::array<std::array<std::optional<std::string>,2>,2> Slots{},Archived{};
    std::optional<std::string> Journal;
    bool ArchiveFailure=false;
    bool ReadFailure=false,JournalFailure=false;
    std::optional<std::string> ArchivedJournal;
    bool PreserveJournal() override {
        if(ArchiveFailure)return false;ArchivedJournal=Journal;return true;
    }
    int Archives=0,Calls=0,FailAt=0;
    SlotBytes Read(SaveKind kind,bool backup) override {return {ReadFailure,Slots[static_cast<int>(kind)-1][backup]};}
    SlotBytes ReadJournal() override {return {JournalFailure,Journal};}
    bool Replace(SaveKind kind,bool backup,std::string_view bytes) override {
        if(++Calls==FailAt) return false;Slots[static_cast<int>(kind)-1][backup]=std::string(bytes);return true;
    }
    bool ReplaceJournal(std::string_view bytes) override {if(++Calls==FailAt)return false;Journal=std::string(bytes);return true;}
    bool Preserve(SaveKind kind) override {
        ++Archives;if(ArchiveFailure)return false;Archived[static_cast<int>(kind)-1]=Slots[static_cast<int>(kind)-1];return true;
    }
};
}
int TestSaveReset()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL SaveReset: "<<why<<'\n';}};
    Profile profile;profile.Bonds.fill(55);profile.SpecialConversations.fill(true);
    profile.Epilogues.fill(true);
    Run run(42,Difficulty::Intermediate,HeroRole::CommandFan,profile);
    const RunSaveState saved{run.Checkpoint(),{}};
    for(const auto kind : {SaveKind::Run,SaveKind::Profile}) {
        ResetMemory files;SaveTransaction tx(files,files);tx.Prepare(saved,profile);tx.Retry();
        check(tx.PrepareReset(kind)==SlotIssue::Invalid && files.Archives==0,"正常な記録は初期化しない");
        const int index=static_cast<int>(kind)-1;
        files.Slots[index]={"broken main","broken backup"};const auto before=files.Slots;
        files.ArchiveFailure=true;
        check(tx.PrepareReset(kind)==SlotIssue::WriteFailed && files.Slots==before && !tx.HasPending(),"保全失敗時は上書きしない");
        files.ArchiveFailure=false;
        check(tx.PrepareReset(kind)==SlotIssue::None && files.Slots==before,"先に破損ファイルを保全");
        check(files.Archived[index]==before[index],"両方の破損内容を保持");
        check(tx.Retry()==SlotIssue::None,"対象側を初期化");const auto loaded=tx.Load();
        check(loaded.Issue==SlotIssue::None && loaded.RunIssue==SlotIssue::None && loaded.ProfileIssue==SlotIssue::None,"整合する状態で復旧");
        if(kind==SaveKind::Run) check(!loaded.Run && loaded.Permanent==profile,"周回だけ消し恒久を維持");
        else check(loaded.Run==saved && loaded.Permanent==Profile{},"恒久だけ初期化し正常周回を維持");
    }
    ResetMemory both;SaveTransaction tx(both,both);tx.Prepare(saved,profile);tx.Retry();
    both.Slots[0]={"bad run","bad run backup"};both.Slots[1]={"bad profile","bad profile backup"};
    const auto oldProfile=both.Slots[1];
    check(tx.PrepareReset(SaveKind::Run)==SlotIssue::None && tx.Retry()==SlotIssue::None,"両側破損でも一方だけ初期化できる");
    check(both.Slots[1]==oldProfile && tx.Load().ProfileIssue==SlotIssue::Corrupt,"未確認の側には触れない");
    check(tx.PrepareReset(SaveKind::Profile)==SlotIssue::None && tx.Retry()==SlotIssue::None,"残る側も別の確認で初期化");
    check(!tx.Load().Run && tx.Load().Permanent==Profile{} && tx.Load().Issue==SlotIssue::None,"両側を正常化");
    ResetMemory interrupted;SaveTransaction initial(interrupted,interrupted);initial.Prepare(saved,profile);initial.Retry();
    interrupted.Slots[0]={"broken","broken"};initial.PrepareReset(SaveKind::Run);
    interrupted.Calls=0;interrupted.FailAt=3;check(initial.Retry()==SlotIssue::WriteFailed,"初期化途中の失敗を保持");
    interrupted.FailAt=0;SaveTransaction restarted(interrupted,interrupted);const auto recovery=restarted.Load();
    check(recovery.Issue==SlotIssue::None && !recovery.Run && recovery.Permanent==profile && interrupted.Archives==1,"再起動で同じ初期化を完了し正常側を保つ");
    ResetMemory future;SaveTransaction guarded(future,future);guarded.Prepare(saved,profile);guarded.Retry();
    auto payload=*EncodeProfilePayload(profile);payload[4]=9;
    future.Slots[1][0]=EncodeSave({SaveKind::Profile,1,payload});future.Slots[1][1]=future.Slots[1][0];
    check(guarded.Load().ProfileIssue==SlotIssue::UnsupportedVersion,"内部形式の将来版も破損と区別");
    check(guarded.PrepareReset(SaveKind::Profile)==SlotIssue::UnsupportedVersion && future.Archives==0,"将来版を初期化しない");
    const auto runBytes=*EncodeSave({SaveKind::Run,2,*EncodeRunPayload(saved)});
    const auto profileBytes=*EncodeSave({SaveKind::Profile,2,payload});
    std::string journalPayload;
    for(int i=0;i<4;++i) journalPayload.push_back(static_cast<char>((runBytes.size()>>(8*i))&255));
    journalPayload+=runBytes;journalPayload+=profileBytes;
    future.Journal=EncodeSave({SaveKind::Run,2,journalPayload});
    check(guarded.Load().Issue==SlotIssue::UnsupportedVersion,"将来版を含む取引を再構成しない");
    ResetMemory repair;SaveTransaction journalRepair(repair,repair);
    journalRepair.Prepare(saved,profile);journalRepair.Retry();
    check(journalRepair.PrepareJournalRepair()==SlotIssue::Invalid,"正常な取引記録を再構築しない");
    repair.Journal="broken journal";
    const auto intactSlots=repair.Slots;
    repair.ArchiveFailure=true;
    check(journalRepair.PrepareJournalRepair()==SlotIssue::WriteFailed && repair.Slots==intactSlots && repair.Journal=="broken journal","取引記録の保全失敗で元データを維持");
    repair.ArchiveFailure=false;
    check(journalRepair.PrepareJournalRepair()==SlotIssue::None && repair.Slots==intactSlots && repair.ArchivedJournal=="broken journal","一致する正常データから保全後に再構築を準備");
    check(journalRepair.PrepareJournalRepair()==SlotIssue::Pending,"準備後の再構築を重ねない");
    repair.Calls=0;repair.FailAt=3;
    check(journalRepair.Retry()==SlotIssue::WriteFailed,"再構築の書込失敗は固定内容を保持");
    repair.FailAt=0;SaveTransaction repairRestart(repair,repair);const auto repaired=repairRestart.Load();
    check(repaired.Issue==SlotIssue::None && repaired.Run==saved && repaired.Permanent==profile && repaired.Generation==2,"再起動で同じ再構築を完了し両側を維持");
    repair.Journal="broken again";
    repair.Slots[0][0]=EncodeSave({SaveKind::Run,3,*EncodeRunPayload(saved)});
    repair.Slots[0][1]=repair.Slots[0][0];
    const auto mismatch=repair.Slots;
    check(repairRestart.PrepareJournalRepair()==SlotIssue::Corrupt && repair.Slots==mismatch,"世代不一致の正常データを勝手に組み合わせない");
    check(!repair.JournalFiles::PreserveJournal(),"保全機能のない保存先は修復不可");
    check(!repairRestart.Load().JournalRepairable,"世代不一致の修復を提示しない");
    for(int scenario=0;scenario<10;++scenario) {
        ResetMemory boundary;SaveTransaction txBoundary(boundary,boundary);
        txBoundary.Prepare(saved,profile);txBoundary.Retry();boundary.Journal="broken";
        SlotIssue expected=SlotIssue::Corrupt;
        if(scenario==0) {boundary.Journal.reset();expected=SlotIssue::Invalid;}
        if(scenario==1) {boundary.JournalFailure=true;expected=SlotIssue::ReadFailed;}
        if(scenario==2) {boundary.ReadFailure=true;expected=SlotIssue::ReadFailed;}
        if(scenario==3) boundary.Slots[0]={};
        if(scenario==4) boundary.Slots[1]={"bad","bad"};
        if(scenario==5) {
            auto bytes=*EncodeSave({SaveKind::Run,1,{}});bytes[4]=9;
            std::uint64_t hash=14695981039346656037ULL;
            for(std::size_t i=0;i<bytes.size()-8;++i) {hash^=static_cast<unsigned char>(bytes[i]);hash*=1099511628211ULL;}
            for(int i=0;i<8;++i) bytes[bytes.size()-8+i]=static_cast<char>((hash>>(8*i))&255);
            boundary.Journal=bytes;expected=SlotIssue::UnsupportedVersion;
        }
        if(scenario==6 || scenario==7) {
            const auto kind=scenario==6?SaveKind::Run:SaveKind::Profile;
            auto futurePayload=kind==SaveKind::Run?*EncodeRunPayload(saved):*EncodeProfilePayload(profile);
            const auto futureVersion=(kind==SaveKind::Run?RunPayloadSchema:ProfilePayloadSchema)+1;
            for(int i=0;i<4;++i)futurePayload[4+i]=static_cast<char>((futureVersion>>(8*i))&255);
            boundary.Slots[scenario==6?0:1].fill(EncodeSave({kind,1,futurePayload}));expected=SlotIssue::UnsupportedVersion;
        }
        if(scenario==8) boundary.Slots[0].fill(EncodeSave({SaveKind::Run,1,"bad payload"}));
        if(scenario==9) {boundary.Journal=EncodeSave({SaveKind::Run,std::numeric_limits<std::uint64_t>::max(),"invalid transaction"});expected=SlotIssue::GenerationOverflow;}
        const auto before=boundary.Slots;
        check(txBoundary.PrepareJournalRepair()==expected && boundary.Slots==before && !txBoundary.HasPending(),"修復の境界では元データを上書きしない");
    }
    ResetMemory semantic;SaveTransaction semanticTx(semantic,semantic);semanticTx.Prepare(saved,profile);semanticTx.Retry();
    semantic.Journal=EncodeSave({SaveKind::Run,7,"invalid transaction"});
    check(semanticTx.Load().JournalRepairable,"内部破損でも正常な同一世代から修復可能");
    check(semanticTx.PrepareJournalRepair()==SlotIssue::None && semanticTx.Retry()==SlotIssue::None && semanticTx.Load().Generation==8,"破損記録の世代より先で修復する");
    std::cout<<"SaveReset: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
