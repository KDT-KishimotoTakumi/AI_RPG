#include "Infrastructure/DiskSlotFiles.h"
#include <chrono>
#include <iostream>
#include <vector>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
using namespace GirlsSpire;
namespace {
struct ContentionHandle {
    HANDLE Value=INVALID_HANDLE_VALUE;
    ~ContentionHandle(){Close();}
    void Close(){if(Value!=INVALID_HANDLE_VALUE){CloseHandle(Value);Value=INVALID_HANDLE_VALUE;}}
};
}
int TestStorageContention()
{
    namespace fs=std::filesystem;
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL StorageContention: "<<why<<'\n';}return ok;};
    const auto root=fs::temp_directory_path()/("girls-spire-contention-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::error_code error;if(!fs::create_directory(root,error)){check(false,"専用試験フォルダ作成");return failed;}
    const char* names[]={"run.sav","run.bak","profile.sav","profile.bak","transaction.sav"};
    const auto cleanup=[&](const fs::path& folder,const char* name) {
        auto temporary=folder/name;temporary+=".tmp";
        fs::remove(folder/name,error);fs::remove(temporary,error);fs::remove(folder,error);
    };
    for(int route=0;route<5;++route)for(int mode=0;mode<3;++mode) {
        const auto folder=root/("case-"+std::to_string(route)+"-"+std::to_string(mode));
        ContentionHandle handle;std::vector<unsigned> waits;
        DiskSlotFiles files(folder,[&](unsigned delay) {
            waits.push_back(delay);
            // 時間待ちではなく、最初/2回目の失敗が観測された時点で実ハンドルを解放する。
            if(mode==0 || (mode==1 && waits.size()==2))handle.Close();
        });
        const auto write=[&](std::string_view bytes){return route==4?files.ReplaceJournal(bytes):files.Replace(route<2?SaveKind::Run:SaveKind::Profile,route%2!=0,bytes);};
        const auto read=[&]{return route==4?files.ReadJournal():files.Read(route<2?SaveKind::Run:SaveKind::Profile,route%2!=0);};
        check(write("before"),"競合前の確定ファイルを用意");
        check(files.LastMoveError()==0 && files.LastMoveAttempts()==1 && waits.empty(),"正常時は1回・待機なし");
        handle.Value=CreateFileW((folder/names[route]).c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(!check(handle.Value!=INVALID_HANDLE_VALUE,"削除共有を持たない実ハンドルで競合を再現")){continue;}
        const bool result=write("after");
        const auto code=files.LastMoveError();
        check(code==ERROR_ACCESS_DENIED || code==ERROR_SHARING_VIOLATION || code==ERROR_LOCK_VIOLATION,"実際のWindows置換競合コードを保持");
        check(result==(mode<2),"一時競合だけを回復し恒常競合は失敗");
        const std::vector<unsigned> expectedWaits=mode==0?std::vector<unsigned>{20}:std::vector<unsigned>{20,40};
        check(files.LastMoveAttempts()==static_cast<unsigned>(mode==0?2:3) && waits==expectedWaits,"3試行・待機合計60ms以内に制限");
        check(read().Bytes==std::optional<std::string>(mode<2?"after":"before"),"成功は同じ内容、失敗は旧確定内容を保持");
        std::cout<<"StorageContention route="<<route<<" case="<<mode<<" moveError="<<code<<" attempts="<<files.LastMoveAttempts()<<" waits="<<waits.size()<<'\n';
        handle.Close();
        if(mode==2)check(write("after") && read().Bytes=="after" && files.LastMoveError()==0,"ロック解除後は同じ内容を明示再試行できる");
        cleanup(folder,names[route]);
    }
    {
        const auto folder=root/"default-wait";DiskSlotFiles files(folder);ContentionHandle handle;
        check(files.Replace(SaveKind::Run,false,"before"),"実待機経路の確定ファイルを用意");
        handle.Value=CreateFileW((folder/"run.sav").c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(check(handle.Value!=INVALID_HANDLE_VALUE,"実待機経路の競合を用意")) {
            check(!files.Replace(SaveKind::Run,false,"after") && files.LastMoveAttempts()==3 && files.Read(SaveKind::Run,false).Bytes=="before","既定の実待機も3試行で停止・確定内容不変");
        }
        handle.Close();cleanup(folder,"run.sav");
    }
    {
        const auto folder=root/"missing-temporary";ContentionHandle handle;unsigned waits=0;
        DiskSlotFiles files(folder,[&](unsigned){++waits;check(fs::remove(folder/"run.sav.tmp",error),"競合中に一時ファイルが失われた場合を再現");handle.Close();});
        check(files.Replace(SaveKind::Run,false,"before"),"別エラーへの遷移を用意");
        handle.Value=CreateFileW((folder/"run.sav").c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(check(handle.Value!=INVALID_HANDLE_VALUE,"遷移経路の競合を用意")) {
            check(!files.Replace(SaveKind::Run,false,"after") && files.LastMoveError()==ERROR_FILE_NOT_FOUND && files.LastMoveAttempts()==2 && waits==1,"競合以外のエラーへ変わったら即座に停止");
            check(files.Read(SaveKind::Run,false).Bytes=="before","一時ファイル消失でも確定内容を保持");
        }
        handle.Close();cleanup(folder,"run.sav");
    }
    fs::remove(root,error);check(!fs::exists(root),"専用ファイルだけ片付ける");
    std::cout<<"StorageContention: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
