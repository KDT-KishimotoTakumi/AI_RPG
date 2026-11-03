#include "Infrastructure/SaveTransaction.h"
#include <iostream>
#include <limits>

namespace {
class GenerationRecoveryMemory : public GirlsSpire::SlotFiles,public GirlsSpire::JournalFiles {
public:
    std::array<std::optional<std::string>,4> Files,Archived;
    std::optional<std::string> Journal,ArchivedJournal;
    int Calls=0,FailAt=0,Archives=0,ArchiveFailAt=0;
    bool ChangeDuringArchive=false;
    bool ReadFailure=false,JournalReadFailure=false;
    GirlsSpire::SlotBytes Read(GirlsSpire::SaveKind kind,bool backup)override{return {ReadFailure,Files[(static_cast<int>(kind)-1)*2+backup]};}
    GirlsSpire::SlotBytes ReadJournal()override{return {JournalReadFailure,Journal};}
    bool Replace(GirlsSpire::SaveKind kind,bool backup,std::string_view bytes)override{
        if(++Calls==FailAt)return false;Files[(static_cast<int>(kind)-1)*2+backup]=std::string(bytes);return true;
    }
    bool ReplaceJournal(std::string_view bytes)override{if(++Calls==FailAt)return false;Journal=std::string(bytes);return true;}
    bool Preserve(GirlsSpire::SaveKind kind)override{
        if(++Archives==ArchiveFailAt)return false;const auto n=(static_cast<int>(kind)-1)*2;Archived[n]=Files[n];Archived[n+1]=Files[n+1];return true;
    }
    bool PreserveJournal()override{
        if(++Archives==ArchiveFailAt)return false;ArchivedJournal=Journal;
        if(ChangeDuringArchive)Files[0]="changed after confirmation";return true;
    }
};
}
int TestGenerationRecovery()
{
    using namespace GirlsSpire;int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL GenerationRecovery: "<<why<<'\n';}};
    Profile profile;profile.Bonds.fill(55);Run run(42,Difficulty::Beginner,HeroRole::Sword,profile);const RunSaveState saved{run.Checkpoint(),{}};
    const auto rp=*EncodeRunPayload(saved),pp=*EncodeProfilePayload(profile);
    GenerationRecoveryMemory base;
    Run newerRun(43,Difficulty::Beginner,HeroRole::Sword,profile);const auto newerPayload=*EncodeRunPayload(RunSaveState{newerRun.Checkpoint(),{}});
    base.Files={EncodeSave({SaveKind::Run,3,newerPayload}),EncodeSave({SaveKind::Run,2,rp}),EncodeSave({SaveKind::Profile,2,pp}),EncodeSave({SaveKind::Profile,1,pp})};
    base.Journal=EncodeSave({SaveKind::Run,3,{}});
    auto files=base;SaveTransaction tx(files,files);const auto preview=tx.PreviewGenerationRecovery();
    check(preview.Issue==SlotIssue::None && preview.Value.has_value(),"世代不一致から共通世代を予告");
    check(files.Files==base.Files && files.Journal==base.Journal && files.Calls==0 && files.Archives==0 && !tx.HasPending(),"予告/取消は読取だけ");
    if(preview.Value){
        check(preview.Value->Candidate.Run.Generation==2 && preview.Value->Candidate.HighestGeneration==3,"選択世代と戻る上限を予告");
        auto altered=*preview.Value;altered.Originals[0]="forged";
        check(tx.PrepareGenerationRecovery(altered)==SlotIssue::Invalid && files.Archives==0,"一致しない確認情報は拒否");
        check(tx.PrepareGenerationRecovery(*preview.Value)==SlotIssue::None && files.Files==base.Files && files.Journal==base.Journal,"全保全が終わるまで上書きしない");
        check(files.Archived==base.Files && files.ArchivedJournal==base.Journal && tx.HasPending(),"4原本と取引を保全して固定内容を準備");
        check(tx.PreviewGenerationRecovery().Issue==SlotIssue::Pending,"準備中に別候補を作らない");
        check(tx.Retry()==SlotIssue::None,"確認したペアを新世代へ保存");const auto loaded=tx.Load();
        check(loaded.Issue==SlotIssue::None && loaded.Generation==4 && loaded.Run==saved && loaded.Permanent==profile,"同じ世代の内容だけで復旧");
    }
    for(int fail=1;fail<=3;++fail){
        files=base;files.ArchiveFailAt=fail;SaveTransaction service(files,files);const auto candidate=service.PreviewGenerationRecovery();
        if(!candidate.Value){check(false,"保全失敗試験の候補");continue;}
        check(service.PrepareGenerationRecovery(*candidate.Value)==SlotIssue::WriteFailed && !service.HasPending() && files.Files==base.Files && files.Journal==base.Journal && files.Calls==0,"各保全失敗で正常原本も維持");
    }
    for(int fail=1;fail<=6;++fail){
        files=base;SaveTransaction service(files,files);const auto candidate=service.PreviewGenerationRecovery();
        if(!candidate.Value){check(false,"固定再試行試験の候補");continue;}
        check(service.PrepareGenerationRecovery(*candidate.Value)==SlotIssue::None,"再試行前に候補固定");files.FailAt=fail;
        check(service.Retry()==SlotIssue::WriteFailed && service.HasPending(),"全6書込段階の失敗中に進行停止");
        const auto archived=files.Archived;files.FailAt=0;
        if(fail>1){SaveTransaction restarted(files,files);const auto loaded=restarted.Load();check(loaded.Issue==SlotIssue::None && loaded.Run==saved && loaded.Permanent==profile,"ジャーナル確定後は別所有者も同じ復旧を完了");}
        check(service.Retry()==SlotIssue::None && files.Archived==archived && service.Load().Generation==4,"失敗前と同内容の再試行で再保全しない");
    }
    files=base;SaveTransaction changed(files,files);const auto old=changed.PreviewGenerationRecovery();
    if(old.Value){files.Files[0]="external change";check(changed.PrepareGenerationRecovery(*old.Value)==SlotIssue::Invalid && files.Archives==0 && files.Calls==0,"確認後の原本変更を再読込で停止");}
    files=base;files.ChangeDuringArchive=true;SaveTransaction raced(files,files);const auto before=raced.PreviewGenerationRecovery();
    if(before.Value)check(raced.PrepareGenerationRecovery(*before.Value)==SlotIssue::Invalid && !raced.HasPending() && files.Calls==0,"保全中の原本変更も上書きしない");
    files=base;files.Journal.reset();SaveTransaction absent(files,files);const auto noJournal=absent.PreviewGenerationRecovery();
    check(noJournal.Value.has_value(),"取引記録不在でも整合ペアがあれば確認可能");
    if(noJournal.Value)check(absent.PrepareGenerationRecovery(*noJournal.Value)==SlotIssue::None && absent.Retry()==SlotIssue::None,"不在も記録して復旧");
    check(!absent.PreviewGenerationRecovery().Value,"正常状態では巻戻し候補を提供しない");
    files=base;files.Files[0]=files.Files[1];files.Journal=EncodeSave({SaveKind::Run,2,{}});
    SaveTransaction owner(files,files);owner.Prepare(saved,profile);files.Calls=0;files.FailAt=2;owner.Retry();
    const auto interruptedFiles=files.Files;const auto interruptedJournal=files.Journal;const int writes=files.Calls;
    files.FailAt=0;SaveTransaction observer(files,files);const auto observed=observer.PreviewGenerationRecovery();
    check(!observed.Value && files.Calls==writes && files.Files==interruptedFiles && files.Journal==interruptedJournal,"有効な未完了取引があっても候補予告は書込しない");
    for(int mode=0;mode<5;++mode){
        files=base;SaveTransaction guarded(files,files);const auto candidate=guarded.PreviewGenerationRecovery();
        if(!candidate.Value){check(false,"確認情報変更試験の候補");continue;}
        auto token=*candidate.Value;
        if(mode==0)token.Journal="different";
        if(mode==1)token.Candidate.Run.Payload="different";
        if(mode==2)token.Candidate.Profile.Payload="different";
        if(mode==3)++token.Candidate.HighestGeneration;
        if(mode==4)++token.Candidate.Run.Generation;
        check(guarded.PrepareGenerationRecovery(token)==SlotIssue::Invalid && files.Archives==0,"予告情報の各部を改変しても保全/書込を開始しない");
    }
    files=base;files.ReadFailure=true;SaveTransaction unread(files,files);
    check(unread.PreviewGenerationRecovery().Issue==SlotIssue::ReadFailed && files.Calls==0,"本体読込失敗で停止");
    files=base;files.JournalReadFailure=true;SaveTransaction noJournalRead(files,files);
    check(noJournalRead.PreviewGenerationRecovery().Issue==SlotIssue::ReadFailed && files.Calls==0,"取引読込失敗で停止");
    files=base;files.Files[1]=files.Files[0];SaveTransaction noCommon(files,files);
    check(!noCommon.PreviewGenerationRecovery().Value && files.Calls==0,"共通世代なしでは原本を維持して停止");
    files=base;files.Journal="broken journal";SaveTransaction damaged(files,files);const auto damagedCandidate=damaged.PreviewGenerationRecovery();
    check(damagedCandidate.Value.has_value(),"取引破損と世代不一致を併発しても共通ペアを選ぶ");
    if(damagedCandidate.Value)check(damaged.PrepareGenerationRecovery(*damagedCandidate.Value)==SlotIssue::None && damaged.Retry()==SlotIssue::None && files.ArchivedJournal=="broken journal","破損取引も保全して復旧");
    files=base;files.Journal=EncodeSave({SaveKind::Run,5,"bad payload"});SaveTransaction malformed(files,files);const auto malformedCandidate=malformed.PreviewGenerationRecovery();
    check(malformedCandidate.Value && malformedCandidate.Value->Candidate.HighestGeneration==5,"不正取引payloadの有効包絡世代も追い越す");
    files=base;files.Journal=EncodeSave({SaveKind::Run,std::numeric_limits<std::uint64_t>::max(),{}});SaveTransaction overflow(files,files);
    check(overflow.PreviewGenerationRecovery().Issue==SlotIssue::GenerationOverflow && files.Calls==0,"取引側の世代上限も上書きしない");
    const auto futureEnvelope=[](std::string bytes){
        bytes[4]=99;std::uint64_t checksum=14695981039346656037ULL;
        for(std::size_t i=0;i<bytes.size()-8;++i){checksum^=static_cast<unsigned char>(bytes[i]);checksum*=1099511628211ULL;}
        for(int i=0;i<8;++i)bytes[bytes.size()-8+i]=static_cast<char>((checksum>>(i*8))&255);
        return bytes;
    };
    files=base;files.Journal=futureEnvelope(*base.Journal);SaveTransaction futureJournal(files,files);
    check(futureJournal.PreviewGenerationRecovery().Issue==SlotIssue::UnsupportedVersion && files.Calls==0,"未来の取引包絡を保持して停止");
    auto futurePayload=pp;futurePayload[4]=99;
    const auto runBytes=*EncodeSave({SaveKind::Run,3,rp}),profileBytes=*EncodeSave({SaveKind::Profile,3,futurePayload});
    std::string transaction;for(int i=0;i<4;++i)transaction.push_back(static_cast<char>((runBytes.size()>>(i*8))&255));transaction+=runBytes+profileBytes;
    files=base;files.Journal=EncodeSave({SaveKind::Run,3,transaction});SaveTransaction futureNested(files,files);
    check(futureNested.PreviewGenerationRecovery().Issue==SlotIssue::UnsupportedVersion && files.Calls==0,"取引内の未来版を共通世代で上書きしない");
    files=base;files.Files[2]=EncodeSave({SaveKind::Profile,2,futurePayload});SaveTransaction futureSlot(files,files);
    check(futureSlot.PreviewGenerationRecovery().Issue==SlotIssue::UnsupportedVersion && files.Calls==0,"原本内の未来版も停止");
    files=base;files.Files[0]=files.Files[1];files.Journal="broken journal";SaveTransaction sameGeneration(files,files);
    check(!sameGeneration.PreviewGenerationRecovery().Value && files.Calls==0,"同世代修復は既存の取引修復へ委ねる");
    std::cout<<"GenerationRecovery: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
