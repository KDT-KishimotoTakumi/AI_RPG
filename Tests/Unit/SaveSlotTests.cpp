#include "Infrastructure/SaveSlot.h"
#include <array>
#include <iostream>
#include <limits>
#include <vector>

using namespace GirlsSpire;
namespace {
class MemorySlotFiles : public SlotFiles {
public:
    std::array<std::array<std::optional<std::string>,2>,2> Data{};
    bool ReadFailure=false;
    int FailAt=0,Calls=0;
    std::vector<std::string> Written;
    SlotBytes Read(SaveKind kind,bool backup) override {return {ReadFailure,Data[static_cast<int>(kind)-1][backup]};}
    bool Replace(SaveKind kind,bool backup,std::string_view bytes) override {
        ++Calls;Written.emplace_back(bytes);
        if(Calls==FailAt) return false;
        Data[static_cast<int>(kind)-1][backup]=std::string(bytes);return true;
    }
};
}
int TestSaveSlot()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL SaveSlot: "<<why<<'\n';}};
    for(SaveKind kind : {SaveKind::Run,SaveKind::Profile}) {
        MemorySlotFiles files;SaveSlot slot(files,kind);
        check(slot.Load().Issue==SlotIssue::Empty,"初回は空");
        check(slot.Retry()==SlotIssue::NoPending,"準備前に書き込まない");
        for(int generation=1;generation<=10;++generation) {
            const auto payload=std::string("記録")+std::to_string(generation);
            check(slot.Prepare(payload)==SlotIssue::None && slot.HasPending(),"確定内容を固定");
            check(slot.Load().Issue==SlotIssue::Pending,"保存待ちに別のロード不可");
            check(slot.Prepare("別選択")==SlotIssue::Pending,"保存待ちを上書き不可");
            check(slot.Retry()==SlotIssue::None && !slot.HasPending(),"保存完了");
            const auto loaded=slot.Load();
            check(loaded.Issue==SlotIssue::None && loaded.Value && loaded.Value->Payload==payload && loaded.Value->Generation==static_cast<std::uint64_t>(generation),"世代単調増加と内容一致");
            const int index=static_cast<int>(kind)-1;
            check(files.Data[index][0]==files.Data[index][1],"成功時は同じ確定世代を二重化");
            check(!files.Data[1-index][0] && !files.Data[1-index][1],"別スロットは変更しない");
        }
        const int index=static_cast<int>(kind)-1;
        files.Data[index][0]="broken";
        const auto recovered=slot.Load();check(recovered.Issue==SlotIssue::None && recovered.Recovered && recovered.Value->Generation==10,"破損本体からバックアップ復旧");
        files.Data[index][1]="broken";
        check(slot.Load().Issue==SlotIssue::Corrupt,"両方破損を報告");
        check(slot.Prepare("初期化しない")==SlotIssue::Corrupt && !slot.HasPending(),"破損を無断上書きしない");
        check(*files.Data[index][0]=="broken" && *files.Data[index][1]=="broken","破損データ保全");
    }
    for(int failAt : {1,2}) {
        MemorySlotFiles files;SaveSlot slot(files,SaveKind::Run);
        slot.Prepare("前の記録");slot.Retry();
        files.Calls=0;files.FailAt=failAt;files.Written.clear();
        check(slot.Prepare("確定した結果")==SlotIssue::None,"失敗用準備");
        check(slot.Retry()==SlotIssue::WriteFailed && slot.HasPending(),"保存失敗で未確定内容を保持");
        check(slot.Prepare("やり直し")==SlotIssue::Pending,"失敗後も別内容を拒否");
        SaveSlot restarted(files,SaveKind::Run);
        const auto recovery=restarted.Load();
        check(recovery.Value && recovery.Value->Payload==(failAt==1?"前の記録":"確定した結果"),"途中失敗の再起動で直近の完全記録を回収");
        if(failAt==2) check(recovery.Recovered,"先行バックアップからの復旧を通知");
        files.FailAt=0;
        check(slot.Retry()==SlotIssue::None && !slot.HasPending(),"同じ内容で再試行成功");
        for(const auto& bytes:files.Written) check(bytes==files.Written.front(),"全再試行のバイト列が完全一致");
        check(slot.Load().Value && slot.Load().Value->Generation==2,"再試行で世代を増やさない");
    }
    MemorySlotFiles files;SaveSlot slot(files,SaveKind::Profile);
    check(!files.Preserve(SaveKind::Run),"保全未対応のIOでは初期化を許可しない");
    files.ReadFailure=true;check(slot.Load().Issue==SlotIssue::ReadFailed,"読み込み失敗表示");
    check(slot.Prepare("x")==SlotIssue::ReadFailed && files.Calls==0,"読めない既存ファイルを上書きしない");files.ReadFailure=false;
    files.Data[1][0]=EncodeSave({SaveKind::Profile,std::numeric_limits<std::uint64_t>::max(),"last"});
    check(slot.Prepare("x")==SlotIssue::GenerationOverflow,"世代の桁あふれ拒否");files.Data[1][0].reset();
    check(slot.Prepare(std::string(MaxSavePayload+1,'x'))==SlotIssue::Invalid,"過大な保存拒否");
    files.Data[1][0]=EncodeSave({SaveKind::Profile,3,"main"});files.Data[1][1]=EncodeSave({SaveKind::Profile,3,"other"});
    check(slot.Load().Issue==SlotIssue::Corrupt,"同世代で内容が異なる記録を拒否");
    files.Data[1][1]=EncodeSave({SaveKind::Profile,2,"older"});check(!slot.Load().Recovered,"古いバックアップより本体を優先");
    files.Data[1][0].reset();check(slot.Load().Recovered,"本体欠落から復旧");
    files.Data[1][0]=EncodeSave({SaveKind::Run,3,"wrong"});check(slot.Load().Recovered,"別種別の本体から正しいバックアップへ復旧");
    auto future=*EncodeSave({SaveKind::Profile,4,"new format"});future[4]=2;
    std::uint64_t hash=14695981039346656037ULL;
    for(std::size_t i=0;i<future.size()-8;++i){hash^=static_cast<unsigned char>(future[i]);hash*=1099511628211ULL;}
    for(int i=0;i<8;++i) future[future.size()-8+i]=static_cast<char>((hash>>(8*i))&255);
    files.Data[1][0]=future;check(slot.Load().Issue==SlotIssue::UnsupportedVersion,"将来版を古いバックアップで上書きしない");
    SaveSlot invalidSlot(files,static_cast<SaveKind>(0));check(invalidSlot.Load().Issue==SlotIssue::Invalid,"不正スロット種別拒否");
    std::cout<<"SaveSlot: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
