#include "CombatScreen.h"
#include "Domain/CharacterCatalog.h"
#include "Domain/AbilityCatalog.h"

void SCombatScreen::RefreshUltimate()
{
    if(SelectingUltimate && SelectingTeamUltimate) { RefreshTeamUltimate(); return; }
    const auto& state=Combat->Snapshot();
    const auto definition=GirlsSpire::UsesCharacterAbilities(state.AbilityRules)?GirlsSpire::FormalUltimate(state.ActiveId,state.ActiveId>0 && state.ActiveId<=8?state.StartingBonds[state.ActiveId-1]:0):std::nullopt;
    if(!SelectingUltimate) {
        const auto name=definition?definition->Name:GirlsSpire::CharacterUltimateName(state.ActiveId);
        const FString title=name.empty()?TEXT("開発用個人必殺"):UTF8_TO_TCHAR(name.data());
        const FString label=title+(state.Acted?TEXT("（行動済み）"):
            state.Combos.Gauge<50?TEXT("（ゲージ50必要）"):TEXT("（結・ゲージ50消費）"));
        Action(label,"battle.ultimate.select",[this] {
            SelectingTeamUltimate=false;SelectingUltimate=true;SelectedSkill.reset();MoveTarget.reset();Selected.reset();Message.Empty();Refresh();
        },!state.Acted && state.Combos.Gauge>=50);
        RefreshTeamUltimate();
        return;
    }
    if(definition){
        const auto& ability=*definition;
        const TCHAR* selection=ability.Area==GirlsSpire::AbilityArea::SelfRadius || ability.Area==GirlsSpire::AbilityArea::AdjacentFour?TEXT("自身のマスをクリック"):ability.Area==GirlsSpire::AbilityArea::CardinalLine?TEXT("上下左右のマスで方向を指定"):TEXT("対象の敵をクリック");
        Text(FString::Printf(TEXT("%s：%s / 攻撃+%d・回復%d・防御無視%d / 命中時 攻撃−%d・移動−%d / 攻撃強化%d・自身防御強化%d%s / 結・行動1回とゲージ50消費。同種強化は最大値を維持し期限更新。"),UTF8_TO_TCHAR(ability.Name.data()),selection,ability.Effects.AttackBonus,ability.Effects.Healing,ability.Effects.IgnoreDefense,ability.Effects.AttackPenalty,ability.Effects.MovementPenalty,ability.Effects.AttackBuff,ability.Effects.SelfDefenseBuff,ability.Effects.Cleanse?TEXT("・攻撃/移動低下解除"):TEXT("")),"battle.ultimate.help",16);
    }else Text(TEXT("敵1体をクリックしてください。攻撃力＋20 / 結 / 行動1回とゲージ50消費。外れても消費します。"),"battle.ultimate.help",16);
    Action(TEXT("必殺を取り消す"),"battle.ultimate.cancel",[this] {
        SelectingUltimate=false;Selected.reset();Refresh();
    });
    if(!Selected) return;
    const auto preview=Combat->PreviewUltimate(state.ActiveId,*Selected);
    FString label=TEXT("この対象には必殺を使用できません（生存敵・射程・射線・ゲージを確認）");
    if(preview.Legal){
        if(definition){label.Empty();for(const auto& hit:preview.Hits)label+=FString::Printf(TEXT("敵%d / 命中%d%% / 予想%dダメージ\n"),hit.TargetId,hit.Chance,hit.Damage);for(const auto& aid:preview.Aid)label+=FString::Printf(TEXT("味方%d / 回復%d・攻撃強化%d%s\n"),aid.TargetId,aid.Healing,aid.AttackBuff,aid.Cleanse?TEXT("・低下解除"):TEXT(""));if(preview.SelfDefenseBuff)label+=FString::Printf(TEXT("自身の防御強化%d\n"),preview.SelfDefenseBuff);label+=FString::Printf(TEXT("ゲージ50消費→%d（連携成立分は後から加算）"),state.Combos.Gauge-50);}
        else label=FString::Printf(TEXT("敵%d / 命中%d%% / 予想%dダメージ / ゲージ50消費→%d（連携成立分は後から加算）"),preview.Hits[0].TargetId,preview.Hits[0].Chance,preview.Hits[0].Damage,state.Combos.Gauge-50);
    }
    Text(label,"battle.ultimate.preview",16);
    Action(TEXT("必殺を確定"),"battle.ultimate.confirm",[this,revision=state.Revision,actor=state.ActiveId,target=*Selected,preview] {
        if(Combat->UseUltimate(revision,actor,target)) {
            RecordOffensiveStrike(actor,target,preview);
            DescribeHits();
            if(!preview.Aid.empty())Message=TEXT("味方への回復・強化・解除を解決しました。");
            Message=TEXT("個人必殺 / ゲージ50消費。")+Message;
        }
        else Message=TEXT("盤面が変わったため必殺を再確認してください");
        SelectingUltimate=false;Selected.reset();Refresh();
    },preview.Legal);
}
