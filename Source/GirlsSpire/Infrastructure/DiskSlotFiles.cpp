#include "DiskSlotFiles.h"
#include <fstream>
#include <atomic>
#if defined(PLATFORM_WINDOWS)
#include "Windows/WindowsHWrapper.h"
#else
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif
namespace GirlsSpire {
bool DiskSlotFiles::Preserve(SaveKind kind)
{
    if(!Path(kind,false)) return false;
    return PreserveFiles(kind==SaveKind::Run?"run-":"profile-",{*Path(kind,false),*Path(kind,true)});
}
bool DiskSlotFiles::PreserveJournal()
{
    return PreserveFiles("transaction-",{root_/"transaction.sav"});
}
bool DiskSlotFiles::PreserveFiles(std::string_view prefix,std::initializer_list<std::filesystem::path> sources)
{
    std::error_code error;
    if(!std::filesystem::is_directory(root_,error) || error) return false;
    static std::atomic_uint64_t sequence{0};
    const auto folder=root_/"Corrupt"/(std::string(prefix)+
        std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64())+"-"+std::to_string(sequence.fetch_add(1)));
    std::filesystem::create_directories(folder,error);
    if(error) return false;
    for(const auto& source : sources) {
        const bool exists=std::filesystem::exists(source,error);
        if(error) return false;
        if(!exists) continue;
        if(!std::filesystem::is_regular_file(source,error) || error) return false;
        if(!std::filesystem::copy_file(source,folder/source.filename(),std::filesystem::copy_options::none,error) || error) return false;
    }
    return true;
}
SlotBytes DiskSlotFiles::ReadJournal() {return ReadAt(root_/"transaction.sav");}
bool DiskSlotFiles::ReplaceJournal(std::string_view bytes) {return ReplaceAt(root_/"transaction.sav",bytes);}
std::optional<std::filesystem::path> DiskSlotFiles::Path(SaveKind kind,bool backup) const
{
    if(kind!=SaveKind::Run && kind!=SaveKind::Profile) return std::nullopt;
    return root_/((kind==SaveKind::Run?std::string("run"):std::string("profile"))+(backup?".bak":".sav"));
}
SlotBytes DiskSlotFiles::Read(SaveKind kind,bool backup)
{
    const auto path=Path(kind,backup);
    if(!path) return {true,{}};
    return ReadAt(*path);
}
SlotBytes DiskSlotFiles::ReadAt(const std::filesystem::path& path) const
{
    std::error_code error;
    const bool exists=std::filesystem::exists(path,error);
    if(error) return {true,{}};
    if(!exists) {
        const bool parentExists=std::filesystem::exists(path.parent_path(),error);
        if(error) return {true,{}};
        if(parentExists && !std::filesystem::is_directory(path.parent_path(),error)) return {true,{}};
        if(error) return {true,{}};
        return {};
    }
    if(!std::filesystem::is_regular_file(path,error) || error) return {true,{}};
    const auto length=std::filesystem::file_size(path,error);
    if(error || length>MaxSavePayload+32) return {true,{}};
    std::ifstream input(path,std::ios::binary);
    if(!input) return {true,{}};
    std::string bytes(static_cast<std::size_t>(length),'\0');
    input.read(bytes.data(),static_cast<std::streamsize>(bytes.size()));
    if(!input || input.peek()!=std::char_traits<char>::eof()) return {true,{}};
    return {false,std::move(bytes)};
}
bool DiskSlotFiles::Replace(SaveKind kind,bool backup,std::string_view bytes)
{
    const auto path=Path(kind,backup);
    return path && ReplaceAt(*path,bytes);
}
bool DiskSlotFiles::ReplaceAt(const std::filesystem::path& path,std::string_view bytes) const
{
    if(bytes.size()>MaxSavePayload+32) return false;
    std::error_code error;
    std::filesystem::create_directories(root_,error);
    if(error) return false;
    auto temporary=path;temporary+=".tmp";
    const HANDLE file=CreateFileW(temporary.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr);
    if(file==INVALID_HANDLE_VALUE) return false;
    DWORD written=0;
    bool complete=WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)!=0 && written==bytes.size();
    complete=(FlushFileBuffers(file)!=0) && complete;
    complete=(CloseHandle(file)!=0) && complete;
    if(!complete) return false;
    // 同一フォルダの一時ファイルを置換する。既存の正常ファイルを先に消さない。
    return MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
}
}
