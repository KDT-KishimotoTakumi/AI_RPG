#include "CombatScreen.h"

void SCombatScreen::RefreshTeamUltimate()
{
    const auto& state=Combat->Snapshot();
    if(!SelectingUltimate) {
        const bool ready=Combat->CanTeamUltimate(state.ActiveId);
        Action(ready?TEXT("全員必殺（結・ゲージ100消費）"):TEXT("全員必殺（未行動・5人生存・距離4以内・ゲージ100必要）"),
            "battle.teamultimate.select",[this] {
                SelectingTeamUltimate=true;SelectingUltimate=true;
                SelectedSkill.reset();MoveTarget.reset();Selected.reset();Message.Empty();Refresh();
            },ready);
        return;
    }
    Text(TEXT("敵を中心に上下左右1マスへ攻撃力＋40。味方・地形は対象外。発動者の行動1回とゲージ100を消費し、外れても消費します。"),"battle.teamultimate.help",16);
    Action(TEXT("全員必殺を取り消す"),"battle.teamultimate.cancel",[this] {
        SelectingUltimate=false;SelectingTeamUltimate=false;Selected.reset();Refresh();
    });
    if(!Selected) return;
    const auto preview=Combat->PreviewTeamUltimate(state.ActiveId,*Selected);
    FString label=TEXT("この対象には全員必殺を使用できません（生存敵・射程・射線・発動条件を確認）");
    if(preview.Legal) {
        label=TEXT("ゲージ100消費→0（連携成立分は後から加算）\n");
        for(const auto& hit:preview.Hits) label+=FString::Printf(TEXT("敵%d / 命中%d%% / %s\n"),hit.TargetId,hit.Chance,*PredictedDamage(hit.TargetId,hit.Damage));
    }
    Text(label,"battle.teamultimate.preview",16);
    Action(TEXT("全員必殺を確定"),"battle.teamultimate.confirm",[this,revision=state.Revision,actor=state.ActiveId,target=*Selected] {
        if(Combat->UseTeamUltimate(revision,actor,target)) {
            RecordStrike(actor,target);
            DescribeHits();
            Message=TEXT("全員必殺 / ゲージ100消費。")+Message;
        }
        else Message=TEXT("盤面が変わったため全員必殺を再確認してください");
        SelectingUltimate=false;SelectingTeamUltimate=false;Selected.reset();Refresh();
    },preview.Legal);
}
