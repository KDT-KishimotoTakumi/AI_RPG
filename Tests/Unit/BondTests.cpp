#include "Domain/Bond.h"
#include <algorithm>
#include <iostream>
using namespace GirlsSpire;
int TestBond()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL Bond: "<<why<<'\n';}};
    check(ValidProfile(Profile{}),"初期プロフィール");
    for(int id=1;id<=8;++id) for(int rank=0;rank<=100;++rank) {
        for(auto choice:{ConversationChoice::Warm,ConversationChoice::Neutral,ConversationChoice::Cold}) {
            Profile p;p.Bonds.fill(25);p.Bonds[id-1]=rank;
            auto expected=p;const int delta=choice==ConversationChoice::Warm?10:choice==ConversationChoice::Cold?-5:0;
            expected.Bonds[id-1]=std::clamp(rank+delta,0,100);
            check(ValidProfile(p),"全境界は有効");
            check(ApplyConversation(p,id,choice) && p==expected,"該当人物だけ変動・上下限");
        }
    }
    Profile p;const auto initial=p;
    for(int value:p.Bonds) check(value==0,"8人全員0");
    for(int id:{-1,0,9,999}) check(!ApplyConversation(p,id,ConversationChoice::Warm) && p==initial,"不正人物は不変");
    for(int choice:{-1,3,999}) check(!ApplyConversation(p,1,static_cast<ConversationChoice>(choice)) && p==initial,"不正選択は不変");
    for(int id=0;id<8;++id) for(int rank:{-1,101}) {
        Profile bad;bad.Bonds[id]=rank;const auto before=bad;
        check(!ValidProfile(bad),"不正プロフィール");
        check(!ApplyConversation(bad,1,ConversationChoice::Warm) && bad==before,"破損時は他人物も変更しない");
    }
    std::cout<<"Bond: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
