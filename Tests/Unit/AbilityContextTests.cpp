#include "../Fixtures/RosterCases.h"
#include <iostream>
#include <limits>
using namespace GirlsSpire;
int TestAbilityContext()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL AbilityContext: "<<why<<'\n';}};
    check(RunPayloadSchema==12 && ProfilePayloadSchema==4,"周回v12だけ拡張し恒久v4を維持");
    auto saved=TestData::RosterCase(0);
    auto& context=saved.Combat->Battle.State;
    check(context.AbilityRules==AbilityRuleSet::Legacy && context.StartingBonds==std::array<int,8>{},"既存生成は旧ルールを維持");
    context.AbilityRules=AbilityRuleSet::Character;
    saved.Combat->Control.ProfileDriven=true;
    for(int rank=0;rank<=100;++rank){
        context.StartingBonds.fill(rank);
        for(int id=1;id<=8;++id)saved.Combat->Control.Trust[id]=rank;
        const auto before=saved;
        const auto bytes=EncodeRunPayload(saved);
        check(bytes && DecodeRunPayload(*bytes)==saved && saved==before,"全絆0～100を固定して状態/乱数/作戦と往復、元状態不変");
    }
    const auto valid=saved;
    auto continued=Battle::Restore(valid.Combat->Battle);
    check(continued && continued->EndTurn(continued->Snapshot().Revision,0,Facing::South) &&
        continued->Snapshot().StartingBonds==context.StartingBonds && continued->Snapshot().AbilityRules==AbilityRuleSet::Character,
        "手番進行でも開始時絆と識別を固定したまま保持");
    for(int id=0;id<8;++id){
        for(int invalid:{-1,101,std::numeric_limits<int>::min(),std::numeric_limits<int>::max()}){
            auto bad=valid;bad.Combat->Battle.State.StartingBonds[id]=invalid;
            check(!Battle::Restore(bad.Combat->Battle) && !EncodeRunPayload(bad),"不正絆を戦闘/保存境界で拒否");
        }
        auto bad=valid;--bad.Combat->Control.Trust[id+1];
        const auto battle=Battle::Restore(bad.Combat->Battle);
        check(battle && !CombatControl::Restore(bad.Combat->Control,*battle) && !EncodeRunPayload(bad),"開始時絆と固定信頼の不一致を拒否");
        bad=TestData::RosterCase(0);bad.Combat->Battle.State.StartingBonds[id]=1;
        check(!Battle::Restore(bad.Combat->Battle) && !EncodeRunPayload(bad),"旧ルールに新しい絆状態を混入しない");
    }
    for(int invalid:{-1,2,std::numeric_limits<int>::max()}){
        auto bad=valid;bad.Combat->Battle.State.AbilityRules=static_cast<AbilityRuleSet>(invalid);
        check(!Battle::Restore(bad.Combat->Battle) && !EncodeRunPayload(bad),"未知ルールを拒否");
    }
    auto bad=valid;bad.Combat->Control.ProfileDriven=false;
    check(!EncodeRunPayload(bad),"正式ルールは開始時信頼の固定を必須にする");
    // 保存表現を独立に組み立てて新規フィールドの厳密な読込も検証する。
    for(int id=0;id<8;++id){context.StartingBonds[id]=10*(id+1);saved.Combat->Control.Trust[id+1]=context.StartingBonds[id];}
    const auto bytes=EncodeRunPayload(saved);
    if(bytes){
        std::string extension(36,'\0');extension[0]=1;
        for(int id=0;id<8;++id)extension[4+4*id]=static_cast<char>(10*(id+1));
        const auto offset=bytes->find(extension);
        check(offset!=std::string::npos && bytes->find(extension,offset+1)==std::string::npos,"識別と8絆を明示的なlittle-endianで一度だけ保存");
        if(offset!=std::string::npos){
            auto corrupt=*bytes;corrupt[offset]=2;check(!DecodeRunPayload(corrupt),"読込時も未知ルールを拒否");
            for(int id=0;id<8;++id){corrupt=*bytes;corrupt[offset+4+4*id]=101;check(!DecodeRunPayload(corrupt),"読込時も絆範囲を検証");}
        }
        for(std::size_t n=0;n<bytes->size();++n)check(!DecodeRunPayload(std::string_view(*bytes).substr(0,n)),"正式状態の全切断を拒否");
        check(!DecodeRunPayload(*bytes+"x"),"余剰を拒否");
        auto forged=*bytes;forged[4]=11;check(!DecodeRunPayload(forged),"新状態を旧v11として偽装できない");
        forged=*bytes;forged[4]=13;check(!DecodeRunPayload(forged) && InspectPayload(forged,SaveKind::Run)==SaveError::UnsupportedVersion,"未知版を破損と区別して拒否");
        forged=*bytes;forged[forged.size()-2]=0;check(!DecodeRunPayload(forged),"読込時も正式戦闘の信頼固定を必須にする");
    } else check(false,"検証用の正式状態を保存できる");
    std::cout<<"AbilityContext: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
