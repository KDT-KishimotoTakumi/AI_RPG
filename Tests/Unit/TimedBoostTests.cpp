#include "Domain/TimedBoost.h"
#include "Domain/AbilityCatalog.h"
#include <algorithm>
#include <iostream>
#include <limits>
using namespace GirlsSpire;
int TestTimedBoost()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL TimedBoost: "<<why<<'\n';}};
    TimedBoost empty;
    check(ValidTimedBoost(empty) && EndTimedBoostTurn(empty) && empty==TimedBoost{},"未付与の期限終了は無変更で成功");
    const int amounts[]={2,4,6,10,15,std::numeric_limits<int>::max()};
    for(int amount:amounts)for(bool self:{false,true}){
        TimedBoost boost;
        check(ApplyTimedBoost(boost,amount,self) && boost==TimedBoost{amount,self},"付与直後の強度と自己手番除外");
        check(EndTimedBoostTurn(boost) && boost==TimedBoost{self?amount:0,false},"外部付与は次手番終了、自己付与は使用手番を除外");
        if(self)check(EndTimedBoostTurn(boost) && boost==TimedBoost{},"自己付与も次の自身の手番終了で失効");
        check(EndTimedBoostTurn(boost) && boost==TimedBoost{},"失効後の追加終了は無変更");
    }
    for(int old:amounts)for(int incoming:amounts)for(bool oldSelf:{false,true})for(bool newSelf:{false,true}){
        TimedBoost boost{old,oldSelf};
        check(ApplyTimedBoost(boost,incoming,newSelf) && boost==TimedBoost{std::max(old,incoming),newSelf},"弱い/等しい/強い再付与でも加算せず最大、期限は今回で更新");
        auto copy=boost;
        check(EndTimedBoostTurn(copy) && copy==TimedBoost{newSelf?std::max(old,incoming):0,false},"更新前の期限を引き継がない");
        check(boost==TimedBoost{std::max(old,incoming),newSelf},"独立コピーの期限終了で原本を変更しない");
    }
    for(int amount:{-1,std::numeric_limits<int>::min(),0})for(TimedBoost boost:{TimedBoost{},TimedBoost{4,false},TimedBoost{6,true}}){
        const auto before=boost;
        check(!ApplyTimedBoost(boost,amount,false) && boost==before,"0/負の付与を拒否して既存効果と期限を維持");
        check(!ApplyTimedBoost(boost,amount,true) && boost==before,"不正自己付与で期限も更新しない");
    }
    for(TimedBoost boost:{TimedBoost{-1,false},TimedBoost{-1,true},TimedBoost{std::numeric_limits<int>::min(),false},TimedBoost{0,true}}){
        const auto before=boost;
        check(!ValidTimedBoost(boost),"負の強化/強度なしの自己期限を不正とする");
        check(!ApplyTimedBoost(boost,4,false) && boost==before,"不正状態へ付与して正当化しない");
        check(!EndTimedBoostTurn(boost) && boost==before,"不正状態を黙って消去しない");
    }
    TimedBoost attack{4,false},defense{4,true},accuracy{15,false};
    check(EndTimedBoostTurn(attack) && attack==TimedBoost{} && defense==TimedBoost{4,true} && accuracy==TimedBoost{15,false},"各能力の強化は独立した期限を持つ");
    // 承認済みカタログに現れる全強化値を同じ期限処理で扱える。
    for(int id=0;id<9;++id)for(int slot=0;slot<4;++slot){
        const auto ability=slot<2?FormalSkill(id,static_cast<Skill>(slot)):FormalUltimate(id,slot==2?0:100);
        if(!ability){check(false,"カタログ定義が存在");continue;}
        for(int amount:{ability->Effects.AttackBuff,ability->Effects.DefenseBuff,ability->Effects.AccuracyBuff,ability->Effects.SelfDefenseBuff}){
            if(!amount)continue;
            TimedBoost boost;
            check(ApplyTimedBoost(boost,amount,true) && EndTimedBoostTurn(boost) && boost.Amount==amount && EndTimedBoostTurn(boost) && boost==TimedBoost{},"全正式強化値で自己付与から失効まで扱える");
        }
    }
    std::cout<<"TimedBoost: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
