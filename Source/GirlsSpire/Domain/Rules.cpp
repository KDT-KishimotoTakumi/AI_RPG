#include "Rules.h"
#include <algorithm>

namespace GirlsSpire {
int ClampHitChance(int percent) { return std::clamp(percent, 5, 95); }

Prediction GetPrediction(int bond)
{
    bond = std::clamp(bond, 0, 100);
    if (bond < 25) return {3, false, false, false};
    if (bond < 50) return {3, true, false, false};
    if (bond < 75) return {2, true, false, false};
    return {1, false, true, bond == 100};
}

int AddComboGauge(int current, int participants)
{
    current = std::clamp(current, 0, 100);
    int gain = 0;
    switch (participants) {
    case 2: gain = 10; break;
    case 3: gain = 20; break;
    case 5: gain = 40; break;
    default: return current;
    }
    return std::min(100, current + gain);
}

bool SpendUltimate(int& gauge, bool wholeParty)
{
    const int cost = wholeParty ? 100 : 50;
    // 不正入力を黙って修正すると保存やUIの不整合が隠れるため、変更せず拒否する。
    if (gauge < cost || gauge > 100) return false;
    gauge -= cost;
    return true;
}
}
