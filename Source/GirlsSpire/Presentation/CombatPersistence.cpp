#include "CombatScreen.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

bool SCombatScreen::PersistCheckpoint()
{
    if(SavingBlocked) return false;
    auto& save=SuspensionPending?OnSuspend:OnCheckpoint;
    if(!save) return true;
    SavingBlocked=!save({Combat->Checkpoint(),Control.Checkpoint()});
    return !SavingBlocked;
}
void SCombatScreen::ShowSaveFailure()
{
    Grid->ClearChildren();Panel->ClearChildren();
    MovingUnits->ClearChildren();Movements.Reset();
    BoardObjects->ClearChildren();
    ComboGauge->SetText(FText::GetEmpty());ComboResult->SetText(FText::GetEmpty());
    Text(TEXT("保存に失敗しました。進行を停止しています。同じ結果の保存を再試行してください。"),"save.error");
    Action(TEXT("同じ内容で保存を再試行"),"save.retry",[this] {
        const auto keepAlive=SharedThis(this);
        SavingBlocked=false;
        if(!PersistCheckpoint()) {Refresh();return;}
        if(!SuspensionPending) {AdvanceEnemies();Refresh();}
    });
}
