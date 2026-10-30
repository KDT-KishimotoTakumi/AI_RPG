#include "Infrastructure/SaveEnvelope.h"
#include <iostream>
#include <limits>
using namespace GirlsSpire;
int TestSaveEnvelope()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL SaveEnvelope: "<<why<<'\n';}};
    for(auto kind:{SaveKind::Run,SaveKind::Profile})
        for(auto generation:{std::uint64_t{1},std::numeric_limits<std::uint64_t>::max()})
            for(const auto& payload:{std::string{},std::string("日本語"),std::string("a\0b",3)}) {
                SaveEnvelope source{kind,generation,payload};
                auto encoded=EncodeSave(source);
                check(encoded.has_value(),"保存包絡の作成");
                if(!encoded) continue;
                auto decoded=DecodeSave(*encoded,kind);
                check(decoded.Error==SaveError::None && decoded.Value==source,"全項目往復");
                for(std::size_t length=0;length<encoded->size();++length)
                    check(!DecodeSave(std::string_view(*encoded).substr(0,length),kind).Value,"途中切断拒否");
                for(std::size_t index=0;index<encoded->size();++index) {
                    auto corrupt=*encoded;corrupt[index]^=1;
                    check(!DecodeSave(corrupt,kind).Value,"各バイト破損検出");
                }
                check(!DecodeSave(*encoded+"x",kind).Value,"余剰バイト拒否");
                auto wrong=DecodeSave(*encoded,kind==SaveKind::Run?SaveKind::Profile:SaveKind::Run);
                check(wrong.Error==SaveError::WrongKind && !wrong.Value,"別スロット拒否");
            }
    check(!EncodeSave({SaveKind::Run,0,""}),"世代0拒否");
    check(!EncodeSave({static_cast<SaveKind>(3),1,""}),"不明種別拒否");
    check(!EncodeSave({SaveKind::Run,1,std::string(MaxSavePayload+1,'x')}),"上限超過拒否");
    auto largest=EncodeSave({SaveKind::Profile,1,std::string(MaxSavePayload,'x')});
    auto largeResult=largest?DecodeSave(*largest,SaveKind::Profile):SaveDecodeResult{};
    check(largeResult.Value && largeResult.Value->Payload.size()==MaxSavePayload,"上限往復");
    check(!DecodeSave(std::string(MaxSavePayload+33,'x'),SaveKind::Run).Value,"入力サイズ上限");
    // 独立したテスト側の再計算で、正しいチェックサムでも不正なヘッダを拒否する。
    auto base=EncodeSave({SaveKind::Run,1,""});
    if(base) for(int field:{4,8,12}) {
        auto bytes=*base;bytes[field]=field==12?0:3;
        std::uint64_t hash=14695981039346656037ULL;
        for(std::size_t i=0;i<bytes.size()-8;++i) { hash^=static_cast<unsigned char>(bytes[i]);hash*=1099511628211ULL; }
        for(int i=0;i<8;++i) bytes[bytes.size()-8+i]=static_cast<char>((hash>>(8*i))&255);
        auto result=DecodeSave(bytes,SaveKind::Run);
        check(!result.Value && result.Error==(field==4?SaveError::UnsupportedVersion:SaveError::Corrupt),"不正ヘッダと非対応版の区別");
    }
    std::cout<<"SaveEnvelope: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
