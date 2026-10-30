#include "Shop.h"
namespace GirlsSpire {
int ShopPrice(ShopOffer offer)
{
    switch(offer) {
    case ShopOffer::Power: case ShopOffer::Guard: return 100;
    case ShopOffer::Medicine: case ShopOffer::Heal: return 30;
    case ShopOffer::Treat: return 50;
    default: return 0;
    }
}
bool Buy(Purse& purse,ShopStock& stock,Accessory& accessory,HealthProgress& health,
         int baseMaxHp,ShopOffer offer)
{
    const int price=ShopPrice(offer);
    if(price==0 || purse.Coins<price || purse.Medicines<0 || purse.Medicines>5 ||
       stock.Power<0 || stock.Power>1 || stock.Guard<0 || stock.Guard>1 ||
       stock.Medicines<0 || stock.Medicines>3 || baseMaxHp<10 || baseMaxHp>10000 ||
       health.Wounds<0 || health.Wounds>4) return false;
    const int maxHp=EffectiveMaxHp(baseMaxHp,health.Wounds);
    if(health.Hp<-1 || health.Hp>maxHp ||
       (accessory!=Accessory::None && accessory!=Accessory::Power && accessory!=Accessory::Guard)) return false;
    // 有効性を確定してから変更し、失敗時に部分的な購入を残さない。
    switch(offer) {
    case ShopOffer::Power:
        if(stock.Power==0) return false;
        --stock.Power;accessory=Accessory::Power;break;
    case ShopOffer::Guard:
        if(stock.Guard==0) return false;
        --stock.Guard;accessory=Accessory::Guard;break;
    case ShopOffer::Medicine:
        if(stock.Medicines==0 || purse.Medicines==5) return false;
        --stock.Medicines;++purse.Medicines;break;
    case ShopOffer::Heal:
        if(health.Hp==-1 || health.Hp==maxHp) return false;
        health.Hp=maxHp;break;
    case ShopOffer::Treat:
        if(!TreatHealth(health)) return false;
        break;
    default: return false;
    }
    purse.Coins-=price;
    return true;
}
}
