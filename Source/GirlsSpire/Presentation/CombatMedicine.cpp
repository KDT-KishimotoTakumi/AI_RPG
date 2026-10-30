#include "CombatScreen.h"
void SCombatScreen::RefreshMedicine()
{
    // 持ち込んだ戦闘だけ表示し、使い切った後も残数0と無効ボタンを残す。
    if(!MedicineVisible) return;
    const auto& state=Combat->Snapshot();
    Text(FString::Printf(TEXT("共有傷薬：%d個 / 自身か上下左右の味方 / HP30回復 / 1行動"),state.Medicines),"battle.medicine.count",13);
    Action(TEXT("傷薬を使う"),"battle.medicine",[this] {
        SelectingMedicine=true;Selected.reset();SelectedSkill.reset();MoveTarget.reset();Refresh();
    },state.Medicines>0 && !state.Acted);
    if(!SelectingMedicine) return;
    Action(TEXT("傷薬を取り消す"),"battle.medicine.cancel",[this]{SelectingMedicine=false;Selected.reset();Refresh();});
    if(!Selected) {Text(TEXT("自身か隣接する味方のマスを選んでください。"),"battle.medicine.help",15);return;}
    const auto preview=Combat->PreviewMedicine(state.ActiveId,*Selected);
    Text(preview.Legal?FString::Printf(TEXT("対象%dを%d回復 / 傷薬1個・行動1回を消費"),preview.TargetId,preview.Healing):TEXT("この対象には使用できません"),"battle.medicine.preview",15);
    Action(TEXT("傷薬使用を確定"),"battle.medicine.confirm",[this,revision=state.Revision,actor=state.ActiveId,target=*Selected] {
        Message=Combat->UseMedicine(revision,actor,target)?TEXT("傷薬で回復しました。"):TEXT("状態が変わりました。対象を選び直してください。");
        SelectingMedicine=false;Selected.reset();Refresh();
    },preview.Legal);
}
