#pragma once
#include "Domain/Battle.h"
#include <vector>
#include <string>
namespace GirlsSpire::TestData {
struct ComboCase {int Id;std::vector<Position> Players;std::vector<Position> Enemies;std::vector<int> Tags;std::vector<std::vector<int>> Targets;};
inline std::vector<ComboCase> ComboCatalogCases() {
    const int C=Continuation|Ending,T=Twist|Ending,E=Continuation|Ending;
    std::vector<ComboCase> cases={
        {2,{{1,1},{2,1}},{{6,1}},{Opening,E},{{},{100}}},
        {3,{{1,1},{3,1},{5,1}},{{6,1}},{Opening,T,E},{{},{100},{100}}},
        {5,{{1,4},{2,3},{2,5},{5,4},{4,4}},{{6,4}},{Opening,Continuation|Twist,Continuation|Twist,T,E},{{},{},{},{100},{100}}},
        {101,{{1,1},{3,1}},{{2,2}},{C,E},{{100},{100}}},
        {102,{{1,1},{2,1}},{{1,2},{2,2}},{C,E},{{100},{101}}},
        {103,{{1,1},{3,1}},{{2,1}},{T,E},{{100},{100}}},
        {104,{{1,1},{3,1}},{{1,2},{3,2}},{T,E},{{100},{101}}},
        {105,{{1,1},{2,2}},{{2,1}},{E,E},{{100},{100}}},
        {106,{{1,1},{4,1},{6,1}},{{3,2},{4,2}},{C,T,E},{{100},{101},{100}}},
        {107,{{1,1},{1,4},{1,6}},{{2,3},{2,4}},{C,T,E},{{100},{101},{100}}},
        {108,{{1,1},{1,4},{3,1}},{{3,3}},{T,C,E},{{100},{100},{100}}},
        {109,{{1,1},{4,1},{6,1}},{{3,2}},{T,C,E},{{100},{100},{100}}},
        {110,{{1,1},{3,2},{5,1}},{{1,2},{3,3},{5,2}},{C,C,E},{{100},{101},{102}}},
        {111,{{1,1},{3,2},{4,2}},{{3,3},{4,3}},{T,T,E},{{100},{100},{101}}}
    };
    const std::vector<std::vector<int>> five={{C,C,T,T,E},{C,T,C,T,E},{T,C,T,C,E},{T,T,C,C,E},{C,C,C,T,E},{T,C,C,C,E}};
    for(std::size_t i=0;i<five.size();++i)cases.push_back({112+static_cast<int>(i),{{1,1},{3,2},{2,4},{5,6},{4,3}},{{3,3}},five[i],{{100},{100},{100},{100},{100}}});
    return cases;
}
inline const char* ComboExpectedName(int id) {
    static const char* names[]={"2人・隣接連携","3人・同一対象連携","5人・集合連携","直線の追撃","交差の追撃","対面の挟撃","陽動の受け渡し","角の包囲","横陣の受け渡し","縦陣の受け渡し","鉤形の追撃","横列の畳み掛け","三方面の攻勢","集中からの転進","二段の仕込み","交互の攻勢","交互の崩し","先手の崩し","連撃からの崩し","崩して繋ぐ総攻め"};
    return id==2?names[0]:id==3?names[1]:id==5?names[2]:id>=101 && id<=117?names[id-98]:"";
}
inline unsigned ComboWinningSeed() {
    for(unsigned seed=0;seed<1000;++seed) {Random random(seed);bool hit=true;for(int i=0;i<5;++i)hit=hit && random.Uniform(100)<95;if(hit)return seed;}
    return 0;
}inline std::vector<int> ComboExpectedPattern(const ComboCase& fixture) {
    std::vector<int> result;
    for(std::size_t i=0;i<fixture.Tags.size();++i) {
        if(i+1==fixture.Tags.size() || fixture.Id==105)result.push_back(Ending);
        else if(fixture.Tags[i]==Opening)result.push_back(Opening);
        else if(fixture.Targets[i].empty())result.push_back(Continuation);
        else result.push_back(fixture.Tags[i]&Twist?Twist:Continuation);
    }
    return result;
}
inline std::string ComboExpectedSyntax(const ComboCase& fixture) {
    std::string text;for(int tag:ComboExpectedPattern(fixture)){if(!text.empty())text+="→";text+=tag==Opening?"起":tag==Continuation?"承":tag==Twist?"転":"結";}return text;
}}
