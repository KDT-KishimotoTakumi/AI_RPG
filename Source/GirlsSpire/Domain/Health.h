#pragma once
namespace GirlsSpire {
struct HealthProgress {
    int Hp=-1; // 未参加の人物は初期最大HPを使用する。
    int Wounds=0; // 0～3は通常負傷数、4は重傷。
    bool operator==(const HealthProgress&) const = default;
};
int EffectiveMaxHp(int baseMaxHp,int wounds);
bool RecoverHealth(HealthProgress& health,int baseMaxHp,int finalHp);
bool HealAtRest(HealthProgress& health,int baseMaxHp);
bool TreatHealth(HealthProgress& health);
}
