#include "Domain/Shop.h"
#include <iostream>
using namespace GirlsSpire;
int TestShop()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL Shop: "<<why<<'\n';}};
    check(Purse{}==Purse{100,0} && ShopStock{}==ShopStock{1,1,3},"初期資源・店舗在庫");
    for(auto offer:{ShopOffer::Power,ShopOffer::Guard,ShopOffer::Medicine,ShopOffer::Heal,ShopOffer::Treat}) {
        const int price=offer==ShopOffer::Treat?50:(offer==ShopOffer::Power || offer==ShopOffer::Guard?100:30);
        check(ShopPrice(offer)==price,"固定価格");
        for(int coins:{0,price-1,price,price+1,1000}) {
            Purse p{coins,0};ShopStock s;Accessory a=Accessory::None;HealthProgress h{10,1};
            const auto oldP=p;const auto oldS=s;const auto oldH=h;
            const bool ok=Buy(p,s,a,h,80,offer);
            check(ok==(coins>=price),"不足・ちょうど・余剰");
            if(!ok) check(p==oldP && s==oldS && h==oldH && a==Accessory::None,"失敗は原子的");
            else {
                check(p.Coins==coins-price,"代金1回");
                check(p.Medicines==(offer==ShopOffer::Medicine?1:0),"所持薬");
                check(s==ShopStock{offer==ShopOffer::Power?0:1,offer==ShopOffer::Guard?0:1,offer==ShopOffer::Medicine?2:3},"該当在庫だけ消費");
                check(a==(offer==ShopOffer::Power?Accessory::Power:offer==ShopOffer::Guard?Accessory::Guard:Accessory::None),"装備");
                check(h==HealthProgress{offer==ShopOffer::Heal?70:10,offer==ShopOffer::Treat?0:1},"回復と治療の独立");
            }
        }
    }
    Purse p{1000,0};ShopStock s;Accessory a=Accessory::None;HealthProgress h{1,4};
    check(Buy(p,s,a,h,80,ShopOffer::Power) && Buy(p,s,a,h,80,ShopOffer::Guard) && a==Accessory::Guard && p.Coins==800,"旧装備置換");
    check(!Buy(p,s,a,h,80,ShopOffer::Power) && p.Coins==800,"売切れ");
    for(int wound=3;wound>=0;--wound) check(Buy(p,s,a,h,80,ShopOffer::Treat) && h==HealthProgress{1,wound},"重傷を含む反復治療");
    check(!Buy(p,s,a,h,80,ShopOffer::Treat) && p.Coins==600,"無負傷拒否");
    check(Buy(p,s,a,h,80,ShopOffer::Heal) && h.Hp==80 && !Buy(p,s,a,h,80,ShopOffer::Heal) && p.Coins==570,"満タン拒否");
    for(int i=0;i<3;++i) check(Buy(p,s,a,h,80,ShopOffer::Medicine),"薬在庫3個");
    check(!Buy(p,s,a,h,80,ShopOffer::Medicine),"薬売切れ");
    s=ShopStock{};p.Medicines=4;
    check(Buy(p,s,a,h,80,ShopOffer::Medicine) && p.Medicines==5,"上限直前");
    const auto full=p;const auto stock=s;
    check(!Buy(p,s,a,h,80,ShopOffer::Medicine) && p==full && s==stock,"上限で無消費");
    for(int invalid:{-1,5,999}) {
        check(ShopPrice(static_cast<ShopOffer>(invalid))==0,"未知商品");
        check(!Buy(p,s,a,h,80,static_cast<ShopOffer>(invalid)) && p==full && s==stock,"未知商品で無消費");
    }
    for(int invalid=0;invalid<19;++invalid) {
        Purse bad{100,0};ShopStock bs;Accessory ba=Accessory::None;HealthProgress bh{1,0};int base=80;
        switch(invalid) {
        case 0:bad.Coins=-1;break;
        case 1:bad.Medicines=-1;break;
        case 2:bad.Medicines=6;break;
        case 3:bs.Power=-1;break;
        case 4:bs.Power=2;break;
        case 5:bs.Guard=-1;break;
        case 6:bs.Guard=2;break;
        case 7:bs.Medicines=-1;break;
        case 8:bs.Medicines=4;break;
        case 9:base=9;break;
        case 10:base=10001;break;
        case 11:bh.Wounds=-1;break;
        case 12:bh.Wounds=5;break;
        case 13:bh.Hp=-2;break;
        case 14:bh.Hp=81;break;
        case 15:ba=static_cast<Accessory>(-1);break;
        case 16:ba=static_cast<Accessory>(3);break;
        case 17:bh.Hp=-1;break;
        case 18:bh.Hp=80;break;
        }
        const auto bp=bad;const auto st=bs;const auto ac=ba;const auto hp=bh;
        check(!Buy(bad,bs,ba,bh,base,ShopOffer::Heal) && bad==bp && bs==st && ba==ac && bh==hp,"不正値・効果なしは全状態不変");
    }
    for(int seed=0;seed<100;++seed) {
        Purse x{100+seed,0},y=x;ShopStock sx,sy;Accessory ax=Accessory::None,ay=ax;HealthProgress hx{1,0},hy=hx;
        for(int i=0;i<6;++i) {
            const bool rx=Buy(x,sx,ax,hx,80,ShopOffer::Medicine),ry=Buy(y,sy,ay,hy,80,ShopOffer::Medicine);
            check(rx==ry && x==y && sx==sy && ax==ay && hx==hy,"同じ初期資源・操作で同じ結果");
        }
    }
    std::cout<<"Shop: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
