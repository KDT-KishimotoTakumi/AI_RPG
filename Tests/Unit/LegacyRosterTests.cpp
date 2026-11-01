#include "../Fixtures/RosterCases.h"
#include "../Fixtures/RosterBaseline.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace GirlsSpire;
int TestLegacyRoster()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL LegacyRoster: "<<why<<'\n';}return ok;};
    const auto finish=[&]{std::cout<<"LegacyRoster: "<<checks<<" checks, "<<failed<<" failed\n";return failed;};
    // コンパイル時のソース位置から解決し、実行場所と日本語パスに依存しない。
    const auto path=std::filesystem::path(u8"" __FILE__).parent_path().parent_path()/"Fixtures"/"roster-v11.bin";
    std::error_code error;
    if(!check(std::filesystem::file_size(path,error)==271819 && !error,"再生成しない固定v11原本のサイズ"))return finish();
    std::ifstream input(path,std::ios::binary);
    const std::string bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
    if(!check(bytes.size()==271819 && bytes.substr(0,5)=="GSR11","固定原本の読込と署名"))return finish();
    std::size_t offset=5;
    for(unsigned seed=0;seed<100;++seed){
        if(!check(bytes.size()-offset>=4,"100件すべての長さフィールドが存在"))return finish();
        std::uint32_t length=0;
        for(unsigned i=0;i<4;++i)length|=static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset+i]))<<(8*i);
        offset+=4;
        if(!check(length<=bytes.size()-offset,"payloadの境界を越えない"))return finish();
        const std::string_view payload(bytes.data()+offset,length);offset+=length;
        check(TestData::RosterPayloadHash(payload)==TestData::RosterBaselineHashes[seed],"各v11 payloadは共用前の既知100ハッシュに一致");
        const auto decoded=DecodeRunPayload(payload);
        check(decoded && *decoded==TestData::RosterCase(seed),"旧形式の全状態/人物/戦場/乱数を同じ意味で復元");
        if(!decoded)continue;
        const auto current=EncodeRunPayload(*decoded);
        check(current && DecodeRunPayload(*current)==decoded,"旧形式から現行形式へ往復して意味を維持");
        if constexpr(RunPayloadSchema==11)check(current && *current==payload,"現行形式がv11の間は全バイトも一致");
    }
    check(offset==bytes.size(),"末尾に未検証の余剰データがない");
    return finish();
}
