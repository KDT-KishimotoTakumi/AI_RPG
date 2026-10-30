#include "Infrastructure/DiskSlotFiles.h"
#include <chrono>
#include <fstream>
#include <iostream>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

using namespace GirlsSpire;
int TestDiskSlotFiles()
{
    namespace fs=std::filesystem;
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL DiskSlotFiles: "<<why<<'\n';}};
    const auto suffix=std::chrono::steady_clock::now().time_since_epoch().count();
    // 同期対象の作業ツリーと分離する。実際のゲーム保存先は別途UI/再起動試験で検証する。
    const auto root=fs::temp_directory_path()/("girls-spire-save-io-test-"+std::to_string(suffix));
    std::error_code error;
    if(!fs::create_directory(root,error)) {check(false,"専用試験フォルダ作成");return failed;}
    DiskSlotFiles files(root/"nested");
    check(!files.Read(SaveKind::Run,false).Failed && !files.Read(SaveKind::Run,false).Bytes,"未作成スロットは空");
    for(const auto kind : {SaveKind::Run,SaveKind::Profile}) for(bool backup : {false,true}) {
        const std::string bytes{"日本語\0binary",16};
        check(files.Replace(kind,backup,bytes),"日本語・NULを含む原子的書込");
        check(files.Read(kind,backup).Bytes==bytes,"バイト列一致");
        check(files.Replace(kind,backup,"new"),"既存ファイルの原子的置換");
        check(files.Read(kind,backup).Bytes=="new","置換結果一致");
        const bool emptyWritten=files.Replace(kind,backup,"");
        const auto writeError=GetLastError();
        check(emptyWritten,"空データもIO層で保持");
        if(!emptyWritten) {
            // 原因不明の実IO失敗は再試行で隠さず、元ファイルを保全して止める。
            std::cerr<<"DiskSlotFiles OS error="<<writeError<<" kind="<<static_cast<int>(kind)
                <<" backup="<<backup<<" preserved="<<root.string()<<'\n';
            return failed;
        }
        check(files.Read(kind,backup).Bytes=="","空データは未作成と区別");
    }
    SaveSlot slot(files,SaveKind::Run);
    // 外部プロセスの共有制約で置換を拒否されても、確定済みの元ファイルを壊さない。
    check(files.Replace(SaveKind::Run,false,"locked original"),"共有制約試験の元データを保存");
    const auto lockedPath=root/"nested/run.sav";
    const HANDLE locked=CreateFileW(lockedPath.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    check(locked!=INVALID_HANDLE_VALUE,"削除共有を許可しない読込みハンドルを保持");
    if(locked!=INVALID_HANDLE_VALUE) {
        check(!files.Replace(SaveKind::Run,false,"blocked replacement") && files.Read(SaveKind::Run,false).Bytes=="locked original","共有違反時は失敗を返し元データを維持");
        CloseHandle(locked);
        check(files.Replace(SaveKind::Run,false,"unlocked replacement") && files.Read(SaveKind::Run,false).Bytes=="unlocked replacement","ロック解除後は明示した再試行で置換できる");
    }
    // 既知の試験ファイルだけを消す。破損初期化は製品APIで無断実行しない。
    fs::remove(root/"nested/run.sav",error);fs::remove(root/"nested/run.bak",error);
    check(slot.Prepare("実ディスクの確定結果")==SlotIssue::None && slot.Retry()==SlotIssue::None,"保存スロットを実ディスクへ接続");
    check(slot.Load().Value && slot.Load().Value->Payload=="実ディスクの確定結果","別読み込みで復元");
    fs::remove(root/"nested/run.sav",error);fs::create_directory(root/"nested/run.sav",error);
    check(!files.Replace(SaveKind::Run,false,"cannot replace directory"),"置換不能を失敗として返す");
    check(files.Read(SaveKind::Run,false).Failed,"ディレクトリをデータとして読まない");
    fs::remove(root/"nested/run.sav",error);
    {std::ofstream large(root/"nested/run.sav",std::ios::binary);large.seekp(static_cast<std::streamoff>(MaxSavePayload+32));large.put('x');}
    check(files.Read(SaveKind::Run,false).Failed,"過大ファイルを読み込む前に拒否");
    check(!files.Replace(SaveKind::Run,false,std::string(MaxSavePayload+33,'x')),"過大な書き込み拒否");
    check(files.Read(static_cast<SaveKind>(0),false).Failed,"無効種別の読み込み拒否");
    check(!files.Replace(static_cast<SaveKind>(0),false,"x"),"無効種別の書き込み拒否");
    {std::ofstream blocked(root/"blocked");blocked<<"file";}
    DiskSlotFiles blocked(root/"blocked");check(!blocked.Replace(SaveKind::Run,false,"x"),"保存先の作成失敗を返す");
    for(const char* name : {"run.sav","run.bak","profile.sav","profile.bak"}) fs::remove(root/"nested"/name,error);
    check(!files.ReadJournal().Failed && !files.ReadJournal().Bytes,"初回ジャーナルは空");
    SaveTransaction transaction(files,files);Run run(7,Difficulty::Beginner,HeroRole::Sword);
    Profile permanent;permanent.Bonds[3]=75;
    check(transaction.Prepare(RunSaveState{run.Checkpoint(),{}},permanent)==SlotIssue::None,"実ディスクの同時保存準備");
    check(transaction.Retry()==SlotIssue::None,"実ディスクへ両スロットを確定");
    DiskSlotFiles reopened(root/"nested");SaveTransaction restarted(reopened,reopened);
    const auto restored=restarted.Load();
    check(restored.Issue==SlotIssue::None && restored.Run==RunSaveState{run.Checkpoint(),{}} && restored.Permanent==permanent,"別の保存サービスから両方を復元");
    check(!blocked.ReplaceJournal("x") && blocked.ReadJournal().Failed,"ジャーナルのIO失敗を伝える");
    const auto raw=files.Read(SaveKind::Run,false).Bytes;
    check(files.Preserve(SaveKind::Run),"実ファイルを初期化前に保全");
    const auto journalRaw=files.ReadJournal().Bytes;
    check(files.PreserveJournal(),"取引記録も修復前に保全");
    bool archived=false;
    bool journalArchived=false;
    if(fs::exists(root/"nested/Corrupt")) for(const auto& directory:fs::directory_iterator(root/"nested/Corrupt")) {
        std::ifstream input(directory.path()/"run.sav",std::ios::binary);
        const std::string copy((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());
        input.close();
        archived=archived || (raw && copy==*raw);
        std::ifstream journalInput(directory.path()/"transaction.sav",std::ios::binary);
        const std::string journalCopy((std::istreambuf_iterator<char>(journalInput)),std::istreambuf_iterator<char>());
        journalInput.close();
        journalArchived=journalArchived || (journalRaw && journalCopy==*journalRaw);
        for(const char* name : {"run.sav","run.bak","profile.sav","profile.bak","transaction.sav"}) fs::remove(directory.path()/name,error);
        fs::remove(directory.path(),error);
    }
    check(archived && files.Read(SaveKind::Run,false).Bytes==raw,"保全内容一致と元ファイル維持");
    check(journalArchived && files.ReadJournal().Bytes==journalRaw,"取引保全内容一致と元ファイル維持");
    fs::remove(root/"nested/Corrupt",error);
    check(!files.Preserve(static_cast<SaveKind>(0)),"不正種別を保全先にしない");
    for(const char* name : {"run.sav","run.bak","profile.sav","profile.bak","run.sav.tmp","run.bak.tmp","profile.sav.tmp","profile.bak.tmp","transaction.sav","transaction.sav.tmp"})
        fs::remove(root/"nested"/name,error);
    fs::remove(root/"nested",error);fs::remove(root/"blocked",error);fs::remove(root,error);
    check(!fs::exists(root),"専用試験ファイルの片付け");
    std::cout<<"DiskSlotFiles: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
