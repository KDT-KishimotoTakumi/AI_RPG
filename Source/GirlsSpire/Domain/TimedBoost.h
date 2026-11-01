#pragma once
namespace GirlsSpire {
// 攻撃/防御/命中の各強化が独立に持つ期限。戦闘外へ持ち越さない。
struct TimedBoost {
    int Amount=0;
    bool SkipCurrentEnd=false;
    bool operator==(const TimedBoost&) const=default;
};
bool ValidTimedBoost(const TimedBoost& boost);
// 受け手自身の手番で付与する場合だけ、その手番終了を期限に数えない。
bool ApplyTimedBoost(TimedBoost& boost,int amount,bool duringRecipientTurn);
// 受け手の手番終了時だけ呼ぶ。他者の手番終了では呼ばない。
bool EndTimedBoostTurn(TimedBoost& boost);
}
