#include "CombatScreen.h"
#include "Widgets/Text/STextBlock.h"

void SCombatScreen::RefreshCombos()
{
    const auto& state=Combat->Snapshot();
    ComboGauge->SetText(FText::FromString(FString::Printf(TEXT("連携ゲージ: %d/100（戦闘内）"),state.Combos.Gauge)));
    FString label=TEXT("起:移動・待機 / 承:通常攻撃・応急手当\n転:強撃・応急手当 / 結:通常攻撃・強撃・個人必殺\n直近の連携: なし");
    if(state.LastCombo) {
        const auto& result=*state.LastCombo;
        const TCHAR* reason=result.RecipeId==2?TEXT("起→結 / 2人が隣接"):
            result.RecipeId==3?TEXT("起→転→結 / 後半2人が同じ敵を攻撃"):
            TEXT("起→承→承→転→結 / 全員が結から4マス以内");
        label=FString::Printf(TEXT("直近の連携: %d人 / ゲージ+%d\n参加者: "),static_cast<int>(result.Participants.size()),result.GaugeGain);
        for(int id:result.Participants) label+=FString::Printf(TEXT("味方%d "),id);
        label+=FString::Printf(TEXT("\n成立理由: %s"),reason);
    }
    label+=TEXT("\n絆獲得（終了時に反映・敗北でも保持）");
    for(int id=1;id<=8;++id) {
        const int combo=state.BondGains.Combo[id-1],aid=state.BondGains.Aid[id-1];
        if(combo+aid>0) label+=FString::Printf(TEXT("\n味方%d: 連携+%d / 援護+%d"),id,combo,aid);
    }
    ComboResult->SetText(FText::FromString(label));
}
