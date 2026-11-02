#include "CombatScreen.h"
#include "Widgets/SBoxPanel.h"
#include <algorithm>

void SCombatScreen::RefreshIntent()
{
    using namespace GirlsSpire;
    const int actor=Combat->Snapshot().ActiveId;
    Text(TEXT("仲間へ作戦を指示。マスで対象・地点を指定できます。数値はHP。"),"intent.help",15);
    TSharedPtr<SHorizontalBox> kinds;
    Panel->AddSlot().AutoHeight().Padding(0,0,0,8) [SAssignNew(kinds,SHorizontalBox)];
    const TCHAR* names[]={TEXT("攻勢"),TEXT("護衛"),TEXT("支援"),TEXT("妨害")};
    for(int kind=0;kind<4;++kind) {
        const auto label=FString::Printf(TEXT("%s%s"),static_cast<int>(Control.Orders[actor].Kind)==kind?TEXT("●"):TEXT(""),names[kind]);
        Action(label,*FString::Printf(TEXT("intent.kind.%d"),kind),[this,actor,kind] {
            Control.Orders[actor]={static_cast<TacticalIntent>(kind),{}, {}}; Refresh();
        },true,kinds);
    }
    Text(Control.ProfileDriven?FString::Printf(TEXT("絆（信頼）: %d"),Control.Trust[actor]):FString::Printf(TEXT("検証用信頼: %d（単独戦場）"),Control.Trust[actor]),"intent.trust",14);
    TSharedPtr<SHorizontalBox> bonds;
    Panel->AddSlot().AutoHeight().Padding(0,0,0,8) [SAssignNew(bonds,SHorizontalBox)];
    if(!Control.ProfileDriven) for(int trust : {0,25,50,75})
        Action(FString::FromInt(trust),*FString::Printf(TEXT("intent.trust.%d"),trust),[this,actor,trust] {
            Control.Trust[actor]=trust; Refresh();
        },true,bonds);
    const auto& order=Control.Orders[actor];
    FString target=TEXT("対象: 仲間に任せる");
    if(order.TargetId) target=FString::Printf(TEXT("対象: 人物%d"),*order.TargetId);
    if(order.Point) target=FString::Printf(TEXT("地点: %d,%d"),order.Point->X,order.Point->Y);
    Text(target,"intent.target",15);
    Action(TEXT("対象指定を解除"),"intent.clear",[this,actor] { Control.Orders[actor].TargetId.reset(); Control.Orders[actor].Point.reset(); Refresh(); });
    const auto forecast=IntentForecast::Build(*Combat,RankIntentActions(*Combat,order),Control.Trust[actor]);
    FString label=forecast.Certain()?TEXT("確定予定\n"):TEXT("行動候補\n");
    if(forecast.Fallback()==IntentFallback::NoSafeCandidate) label+=TEXT("合法で安全な候補がないため待機\n");
    for(const auto& candidate : forecast.Candidates()) {
        const auto& action=candidate.Action;
        const FString verb=action.Kind==BasicActionKind::Move?TEXT("移動"):action.Kind==BasicActionKind::Attack?TEXT("通常攻撃"):
            action.Kind==BasicActionKind::UseSkill?OwnedSkillLabel(actor,action.Ability):FString(TEXT("待機"));
        label+=FString::Printf(TEXT("%s(%d,%d)　被害上限%d"),*verb,action.Target.X,action.Target.Y,candidate.PotentialDamage);
        if(candidate.Probability) label+=FString::Printf(TEXT("　%d%%"),*candidate.Probability);
        if(action.Kind==BasicActionKind::UseSkill) {
            const auto preview=Combat->PreviewSkill(actor,action.Ability,action.Target);
            if(preview.Support)label+=FString::Printf(TEXT(" / 回復%d 攻撃+%d 防御+%d 命中+%d%s"),preview.Healing,preview.AttackBuff,preview.DefenseBuff,preview.AccuracyBuff,preview.Cleanse?TEXT(" 低下解除"):TEXT(""));
            else if(preview.Healing>0) label+=FString::Printf(TEXT(" / 回復%d"),preview.Healing);
            else for(const auto& hit:preview.Attack.Hits)
                label+=FString::Printf(TEXT(" / 敵%d 命中%d%% ダメージ%d"),hit.TargetId,hit.Chance,hit.Damage);
        }
        label+=TEXT("\n");
        if(action.Kind==BasicActionKind::Move) {
            const auto movement=Combat->PreviewMove(actor,action.Target);
            label+=TEXT("経路: ");
            for(const auto at:movement.Path) label+=FString::Printf(TEXT("(%d,%d) "),at.X,at.Y);
            label+=FString::Printf(TEXT("/ 罠被害%d\n"),movement.Damage);
        }
    }
    Text(label.TrimEnd(),"intent.forecast",15);
    Action(TEXT("予告した候補から行動する"),"intent.execute",[this,forecast] { ExecuteIntent(forecast); });
    TSharedPtr<SHorizontalBox> directions;
    Panel->AddSlot().AutoHeight().Padding(0,0,0,8) [SAssignNew(directions,SHorizontalBox)];
    const TCHAR* labels[]={TEXT("北待機"),TEXT("東待機"),TEXT("南待機"),TEXT("西待機")};
    const TCHAR* ids[]={TEXT("battle.wait.north"),TEXT("battle.wait.east"),TEXT("battle.wait.south"),TEXT("battle.wait.west")};
    for(int facing=0;facing<4;++facing)
        Action(labels[facing],ids[facing],[this,facing,revision=Combat->Snapshot().Revision] { Wait(static_cast<Facing>(facing),revision); },true,directions);
    const auto& units=Combat->Snapshot().Units;
    const auto unit=std::find_if(units.begin(),units.end(),[actor](const BattleUnit& value) {return value.Id==actor;});
    if(UsesCharacterSkills(Combat->Snapshot().AbilityRules))
        Text(FString::Printf(TEXT("人物技能 / 再使用待ち %s:%d %s:%d"),*OwnedSkillLabel(actor,Skill::PowerStrike),unit->Cooldowns[0],*OwnedSkillLabel(actor,Skill::FirstAid),unit->Cooldowns[1]),"battle.skill.status",13);
    else Text(FString::Printf(TEXT("開発用技 / 再使用待ち 強撃:%d 応急手当:%d"),unit->Cooldowns[0],unit->Cooldowns[1]),"battle.skill.status",13);
    FString additional;
    for(int i=2;i<SkillCount;++i) if(unit->Skills[i]) additional+=FString::Printf(TEXT("%s:%d "),SkillLabel(static_cast<Skill>(i)),unit->Cooldowns[i]);
    Text(additional,"battle.skill.additional",13);
    Text(UsesCharacterSkills(Combat->Snapshot().AbilityRules)?TEXT("技能の対象とクールダウンに従って候補を表示します。"):TEXT("所持する開発用技を候補化。人物固有技・妨害技は未実装です。"),"intent.notice",13);
    RefreshUltimate();
    Text(Message,"battle.log",14);
}

void SCombatScreen::SelectIntentCell(GirlsSpire::Position cell)
{
    using namespace GirlsSpire;
    const auto& state=Combat->Snapshot();
    auto next=Control.Orders[state.ActiveId]; next.TargetId.reset(); next.Point.reset();
    const auto unit=std::find_if(state.Units.begin(),state.Units.end(),[&](const BattleUnit& value) { return value.Hp>0 && value.At==cell; });
    if(unit==state.Units.end()) next.Point=cell;
    else next.TargetId=unit->Id;
    if(RankIntentActions(*Combat,next).empty()) { Message=TEXT("その作戦では指定できない対象です"); return; }
    Control.Orders[state.ActiveId]=next;
    Message=TEXT("作戦の対象・地点を更新しました");
}

void SCombatScreen::ExecuteIntent(const GirlsSpire::IntentForecast& forecast)
{
    if(!forecast.Execute(*Combat,Control.ChoiceRandom)) { Message=TEXT("盤面が変わったため作戦を再確認してください"); Refresh(); return; }
    Message=TEXT("作戦の予告候補から行動しました");
    AdvanceEnemies();
    Refresh();
}
