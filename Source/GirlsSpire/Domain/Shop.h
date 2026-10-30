#pragma once
#include "Health.h"
namespace GirlsSpire {
enum class Accessory { None, Power, Guard };
enum class ShopOffer { Power, Guard, Medicine, Heal, Treat };
struct Purse {
    int Coins=100;
    int Medicines=0;
    bool operator==(const Purse&) const = default;
};
struct ShopStock {
    int Power=1;
    int Guard=1;
    int Medicines=3;
    bool operator==(const ShopStock&) const = default;
};
int ShopPrice(ShopOffer offer);
// 呼出元で加入者・入店状態・世代を検証する。失敗時は全引数を保持する。
bool Buy(Purse& purse,ShopStock& stock,Accessory& accessory,HealthProgress& health,
         int baseMaxHp,ShopOffer offer);
}
