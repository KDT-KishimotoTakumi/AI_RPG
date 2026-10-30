#include "Domain/BattleBond.h"
#include <algorithm>
#include <iostream>
#include <limits>
using namespace GirlsSpire;
int TestBattleBond()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL BattleBond: "<<why<<'\n';}};
    check(ValidBattleBond({}),"初期値");
    for(int count:{2,3,5}) for(int first=0;first<=8;++first) {
        BattleBond gains;std::vector<int> ids;
        for(int n=0;n<count;++n) ids.push_back((first+n)%9);
        for(int attempt=1;attempt<=8;++attempt) {
            check(AddComboBond(gains,ids),"連携加算");
            for(int id=1;id<=8;++id) {
                const bool included=std::find(ids.begin(),ids.end(),id)!=ids.end();
                check(gains.Combo[id-1]==(included?std::min(5,attempt*(count==2?1:count==3?2:3)):0),"参加者・連携上限");
                check(gains.Aid[id-1]==0,"別枠");
            }
        }
    }
    for(int actor=0;actor<=8;++actor) for(int target=0;target<=8;++target) {
        BattleBond gains;
        for(int attempt=1;attempt<=6;++attempt) {
            check(AddAidBond(gains,actor,target,1)==(actor!=target),"自己回復対象外");
            for(int id=1;id<=8;++id)
                check(gains.Aid[id-1]==(actor!=target && (id==actor || id==target)?std::min(3,attempt):0),"援護の双方・上限");
        }
    }
    BattleBond gains;gains.Combo.fill(5);gains.Aid.fill(3);const auto before=gains;
    for(const auto& ids:std::vector<std::vector<int>>{{},{1},{1,2,3,4},{0,1,2,3,4,5},{1,1},{-1,2},{1,9}})
        check(!AddComboBond(gains,ids) && gains==before,"不正連携は不変");
    for(int bad:{-1,9,999}) {
        check(!AddAidBond(gains,bad,1,10) && gains==before,"不正行動者");
        check(!AddAidBond(gains,1,bad,10) && gains==before,"不正対象");
    }
    for(int amount:{-1,0}) check(!AddAidBond(gains,0,1,amount) && gains==before,"回復なし");
    for(int rank=0;rank<=100;++rank) {
        Profile p;p.Bonds.fill(rank);
        check(ApplyBattleBond(p,gains),"反映");
        for(int value:p.Bonds) check(value==std::min(100,rank+8),"100上限");
    }
    for(int id=0;id<8;++id) for(int bad:{-1,6,std::numeric_limits<int>::max()}) {
        auto invalid=gains;invalid.Combo[id]=bad;const auto original=invalid;
        Profile p;check(!ValidBattleBond(invalid),"不正連携値");
        check(!ApplyBattleBond(p,invalid) && p==Profile{},"不正集計の反映拒否");
        check(!AddComboBond(invalid,{0,1}) && invalid==original,"破損集計へ連携追加不可");
        check(!AddAidBond(invalid,0,1,1) && invalid==original,"破損集計へ援護追加不可");
    }
    for(int id=0;id<8;++id) for(int bad:{-1,4}) {
        auto invalid=gains;invalid.Aid[id]=bad;
        check(!ValidBattleBond(invalid),"不正援護値");
    }
    Profile invalid;invalid.Bonds[7]=101;const auto original=invalid;
    check(!ApplyBattleBond(invalid,gains) && invalid==original,"破損Profile不変");
    std::cout<<"BattleBond: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
