#include "Domain/CharacterCatalog.h"
#include "../Fixtures/RosterCases.h"
#include "../Fixtures/RosterBaseline.h"
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace GirlsSpire;
int TestCharacterCatalog()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL CharacterCatalog: "<<why<<'\n';}};
    const char* names[]={"黒鉄 蓮","真田 茜","白峰 澪","朧","土御門 紫苑","鷹司 楓","雑賀 火乃香","柳生 千景","鈴鹿 小夜"};
    const char* portraits[]={"hero-ren","akane","mio","oboro","shion","kaede","honoka","chikage","sayo"};
    const BattleRole roles[]={BattleRole::Standard,BattleRole::Standard,BattleRole::Dancer,BattleRole::Ninja,BattleRole::Standard,BattleRole::Archer,BattleRole::Gunner,BattleRole::Standard,BattleRole::Dancer};
    const int ranges[]={1,1,1,1,1,4,5,1,1};
    check(CharacterCatalog().size()==9,"主人公と8候補を一元定義");
    for(int id=0;id<9;++id) {
        const auto definition=FindCharacter(id);check(definition && definition->Id==id && definition->Name==names[id] && definition->Portrait==portraits[id],"固定人物ID・名前・既存画像キーを維持");
        const auto unit=CreateCharacter(id,{1,2});
        check(unit.Id==id && unit.Side==Faction::Player && unit.At==Position{1,2} && unit.Role==roles[id] && unit.Range==ranges[id],"既存役割・配置・射程を維持");
        check(unit.Hp==(id==0?80:40) && unit.MaxHp==unit.Hp && unit.Attack==(id==0?18:12) && unit.Speed==(id==0?20:15) && unit.Accuracy==(id==0?95:90),"既存の初期能力を維持");
        check(unit.Movement==3 && unit.Defense==2 && unit.Evasion==5 && unit.Skills==std::array<bool,SkillCount>{true,true},"既存の移動・防御・回避・初期技能を維持");
        auto changed=unit;changed.Hp=1;changed.Skills.fill(false);check(CreateCharacter(id,{1,2})==unit,"生成した人物を変えても原型と次の生成へ影響しない");
    }
    for(int invalid:{-1,9,100,std::numeric_limits<int>::min(),std::numeric_limits<int>::max()}) {
        check(!FindCharacter(invalid),"不正人物IDを検索できない");
        try{(void)CreateCharacter(invalid,{});check(false,"不正人物の生成を拒否");}catch(const std::invalid_argument&){check(true,"不正人物の生成を拒否");}
    }
    for(unsigned seed=0;seed<100;++seed) {
        const auto state=TestData::RosterCase(seed);const auto bytes=EncodeRunPayload(state);
        check(bytes && TestData::RosterPayloadHash(*bytes)==TestData::RosterBaselineHashes[seed],"100シードの人物・編成・戦場・乱数・保存が共用前と一致");
    }
    for(int id=1;id<=8;++id) {
        RunSnapshot joined;joined.Level=Difficulty::Intermediate;joined.Day=5;joined.Phase=RunPhase::Event;joined.Active={NodeKind::Battle};joined.Recruited={id-1};
        const auto party=Encounter::Create(joined,42).Snapshot();
        auto absent=joined;absent.Day=12;absent.Active={NodeKind::FinalBoss};absent.Recruited.clear();
        for(int candidate=0;candidate<8 && absent.Recruited.size()<4;++candidate)if(candidate!=id-1)absent.Recruited.push_back(candidate);
        const auto final=Encounter::Create(absent,42).Snapshot();
        const auto ally=std::find_if(party.Units.begin(),party.Units.end(),[id](const BattleUnit& unit){return unit.Id==id;});
        const auto guest=std::find_if(final.Units.begin(),final.Units.end(),[id](const BattleUnit& unit){return unit.Id==id;});
        check(ally!=party.Units.end() && guest!=final.Units.end(),"加入時/未加入時の両方に同じ人物がいる");
        if(ally!=party.Units.end() && guest!=final.Units.end()){auto normalized=*guest;normalized.Side=Faction::Player;normalized.At=ally->At;check(normalized==*ally,"陣営と配置以外の既存能力・技能は同じ");}
    }
    std::cout<<"CharacterCatalog: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
