#include "CombatScreen.h"
#include "Domain/CharacterCatalog.h"

void SCombatScreen::RefreshUltimate()
{
    if(SelectingUltimate && SelectingTeamUltimate) { RefreshTeamUltimate(); return; }
    const auto& state=Combat->Snapshot();
    if(!SelectingUltimate) {
        const auto name=GirlsSpire::CharacterUltimateName(state.ActiveId);
        const FString title=name.empty()?TEXT("開発用個人必殺"):UTF8_TO_TCHAR(name.data());
        const FString label=title+(state.Acted?TEXT("（行動済み）"):
            state.Combos.Gauge<50?TEXT("（ゲージ50必要）"):TEXT("（結・ゲージ50消費）"));
        Action(label,"battle.ultimate.select",[this] {
            SelectingTeamUltimate=false;SelectingUltimate=true;SelectedSkill.reset();MoveTarget.reset();Selected.reset();Message.Empty();Refresh();
        },!state.Acted && state.Combos.Gauge>=50);
        RefreshTeamUltimate();
        return;
    }
    Text(TEXT("敵1体をクリックしてください。攻撃力＋20 / 結 / 行動1回とゲージ50消費。外れても消費します。"),"battle.ultimate.help",16);
    Action(TEXT("必殺を取り消す"),"battle.ultimate.cancel",[this] {
        SelectingUltimate=false;Selected.reset();Refresh();
    });
    if(!Selected) return;
    const auto preview=Combat->PreviewUltimate(state.ActiveId,*Selected);
    FString label=TEXT("この対象には必殺を使用できません（生存敵・射程・射線・ゲージを確認）");
    if(preview.Legal) label=FString::Printf(TEXT("敵%d / 命中%d%% / 予想%dダメージ / ゲージ50消費→%d（連携成立分は後から加算）"),
        preview.Hits[0].TargetId,preview.Hits[0].Chance,preview.Hits[0].Damage,state.Combos.Gauge-50);
    Text(label,"battle.ultimate.preview",16);
    Action(TEXT("必殺を確定"),"battle.ultimate.confirm",[this,revision=state.Revision,actor=state.ActiveId,target=*Selected] {
        if(Combat->UseUltimate(revision,actor,target)) { DescribeHits(); Message=TEXT("個人必殺 / ゲージ50消費。")+Message; }
        else Message=TEXT("盤面が変わったため必殺を再確認してください");
        SelectingUltimate=false;Selected.reset();Refresh();
    },preview.Legal);
}
