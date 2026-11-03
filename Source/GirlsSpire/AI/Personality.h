#pragma once
#include "BasicTurn.h"
namespace GirlsSpire {
// 合法な既存候補だけを評価する。順位への適用と新旧保存の識別は呼出側で行う。
int PersonalityBonus(const Battle& battle,const BasicAction& action);
}
