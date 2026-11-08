#include "Domain/NarrationText.h"
#include <iostream>
#include <limits>
int TestNarrationText()
{
    using namespace GirlsSpire;int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL NarrationText: "<<why<<'\n';}};
    // 利用者が承認した文面を一字一句固定する。
    check(SetupNarration()=="十二日後、死者の城が目を覚ます。落ちた者たちを呼び戻し、歴史そのものを書き換える城だ。蓮は、わずかな仲間と共に出陣する。","出陣の地の文が承認文面と一致");
    check(DayNarration(6)=="落城姫の合戦が始まる。この一戦で勝敗が変われば、歴史は書き換えられる。","6日目の地の文が承認文面と一致");
    check(DayNarration(12)=="城の主は、歴史を喰らう鬼。斬るたびに、仲間の記憶が一つずつ奪われていく。","12日目の地の文が承認文面と一致");
    // 境界と範囲外は何も返さない。
    for(int day:{std::numeric_limits<int>::min(),-1,0,1,5,7,11,13,100,std::numeric_limits<int>::max()})check(DayNarration(day).empty(),"対象外の日は空");
    std::cout<<"NarrationText: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
