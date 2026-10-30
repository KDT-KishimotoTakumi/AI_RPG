#include "SaveEnvelope.h"
namespace GirlsSpire {
namespace {
constexpr std::size_t HeaderSize=24, ChecksumSize=8;
bool ValidKind(SaveKind kind) { return kind==SaveKind::Run || kind==SaveKind::Profile; }
void Append(std::string& bytes,std::uint64_t value,int width)
{
    for(int i=0;i<width;++i) bytes.push_back(static_cast<char>((value>>(i*8))&255));
}
std::uint64_t Read(std::string_view bytes,std::size_t offset,int width)
{
    std::uint64_t value=0;
    for(int i=0;i<width;++i) value|=static_cast<std::uint64_t>(static_cast<unsigned char>(bytes[offset+i]))<<(i*8);
    return value;
}
std::uint64_t Checksum(std::string_view bytes)
{
    // 偶発的な破損の検出用。改ざん防止や暗号認証には使用しない。
    std::uint64_t value=14695981039346656037ULL;
    for(unsigned char byte:bytes) { value^=byte;value*=1099511628211ULL; }
    return value;
}
}
std::optional<std::string> EncodeSave(const SaveEnvelope& value)
{
    if(!ValidKind(value.Kind) || value.Generation==0 || value.Payload.size()>MaxSavePayload) return std::nullopt;
    std::string bytes="GSSV";
    Append(bytes,1,4);
    Append(bytes,static_cast<std::uint32_t>(value.Kind),4);
    Append(bytes,value.Generation,8);
    Append(bytes,value.Payload.size(),4);
    bytes+=value.Payload;
    Append(bytes,Checksum(bytes),8);
    return bytes;
}
SaveDecodeResult DecodeSave(std::string_view bytes,SaveKind expected)
{
    if(bytes.size()<HeaderSize+ChecksumSize || bytes.size()>HeaderSize+MaxSavePayload+ChecksumSize) return {};
    if(bytes.substr(0,4)!="GSSV") return {};
    const auto length=Read(bytes,20,4);
    if(length!=bytes.size()-HeaderSize-ChecksumSize) return {};
    if(Read(bytes,bytes.size()-ChecksumSize,8)!=Checksum(bytes.substr(0,bytes.size()-ChecksumSize))) return {};
    if(Read(bytes,4,4)!=1) return {SaveError::UnsupportedVersion,std::nullopt};
    const auto kind=static_cast<SaveKind>(Read(bytes,8,4));
    const auto generation=Read(bytes,12,8);
    if(!ValidKind(kind) || generation==0) return {};
    if(kind!=expected) return {SaveError::WrongKind,std::nullopt};
    return {SaveError::None,SaveEnvelope{kind,generation,std::string(bytes.substr(HeaderSize,static_cast<std::size_t>(length)))}};
}
}
