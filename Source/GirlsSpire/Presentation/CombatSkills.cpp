#include "CombatScreen.h"
#include "Domain/CharacterCatalog.h"
#include "Domain/AbilityCatalog.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SWrapBox.h"
#include <algorithm>

const TCHAR* SCombatScreen::SkillLabel(GirlsSpire::Skill skill)
{
    static const std::array<FString,GirlsSpire::SkillCount> names=[] {
        std::array<FString,GirlsSpire::SkillCount> result;
        for(int i=0;i<GirlsSpire::SkillCount;++i)result[i]=UTF8_TO_TCHAR(GirlsSpire::CommonSkillName(static_cast<GirlsSpire::Skill>(i)).data());
        return result;
    }();
    return *names[static_cast<int>(skill)];
}
FString SCombatScreen::OwnedSkillLabel(int actorId,GirlsSpire::Skill skill) const
{
    if(GirlsSpire::UsesCharacterSkills(Combat->Snapshot().AbilityRules))
        if(const auto ability=GirlsSpire::FormalSkill(actorId,skill))return UTF8_TO_TCHAR(ability->Name.data());
    const auto name=GirlsSpire::CharacterSkillName(actorId,skill);
    if(name.empty())return SkillLabel(skill);
    return UTF8_TO_TCHAR(name.data());
}

void SCombatScreen::RefreshSkills()
{
    using namespace GirlsSpire;
    const auto& state=Combat->Snapshot();
    const auto actor=std::find_if(state.Units.begin(),state.Units.end(),[&](const BattleUnit& unit) {return unit.Id==state.ActiveId;});
    if(std::none_of(actor->Skills.begin(),actor->Skills.end(),[](bool owned){return owned;})) return;
    FString status=TEXT("技 / 再使用待ち ");
    for(int i=0;i<SkillCount;++i) if(actor->Skills[i]) status+=FString::Printf(TEXT("%s:%d "),*OwnedSkillLabel(actor->Id,static_cast<Skill>(i)),actor->Cooldowns[i]);
    Text(status,"battle.skill.status",13);
    const auto row=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6,7));
    Panel->AddSlot().AutoHeight()[row];
    for(int index=0;index<SkillCount;++index) {
        if(index>=2 && !actor->Skills[index]) continue;
        Action(OwnedSkillLabel(actor->Id,static_cast<Skill>(index)),*FString::Printf(TEXT("battle.skill.%d"),index),[this,index] {
            SelectedSkill=static_cast<Skill>(index); Selected.reset(); MoveTarget.reset(); Refresh();
        },actor->Skills[index] && actor->Cooldowns[index]==0 && !state.Acted,nullptr,row);
    }
    if(!SelectedSkill) return;
    Action(TEXT("通常操作へ戻る"),"battle.skill.cancel",[this] {SelectedSkill.reset();Selected.reset();Refresh();});
    if(!Selected) return;
    const auto preview=Combat->PreviewSkill(state.ActiveId,*SelectedSkill,*Selected);
    FString label=TEXT("この対象には使用できません");
    if(preview.Legal) {
        if(preview.Support)label=FString::Printf(TEXT("回復%d / 攻撃+%d / 防御+%d / 命中+%d%s"),preview.Healing,preview.AttackBuff,preview.DefenseBuff,preview.AccuracyBuff,preview.Cleanse?TEXT(" / 攻撃低下・移動低下を解除"):TEXT(""));
        else if(preview.Healing>0) label=FString::Printf(TEXT("回復%d"),preview.Healing);
        else if(!preview.Attack.Hits.empty()) {
            label.Empty();
            for(const auto& hit:preview.Attack.Hits) label+=FString::Printf(TEXT("敵%d / 命中%d%% / ダメージ%d\n"),hit.TargetId,hit.Chance,hit.Damage);
        }
        else label=FString::Printf(TEXT("障害物にダメージ%d（確定命中）"),preview.Attack.TerrainDamage);
    }
    Text(label,"battle.skill.preview",15);
    Action(TEXT("技を確定"),"battle.skill.confirm",[this,revision=state.Revision,actorId=state.ActiveId,skill=*SelectedSkill,target=*Selected,preview] {
        if(Combat->UseSkill(revision,actorId,skill,target)) {
            RecordOffensiveStrike(actorId,target,preview.Attack);
            if(preview.Support)Message=FString::Printf(TEXT("対象%dへの支援を確定しました"),preview.TargetId);
            else if(preview.Healing>0) Message=FString::Printf(TEXT("対象%dを%d回復しました"),preview.TargetId,preview.Healing);
            else if(preview.Attack.TerrainDamage>0) Message=TEXT("障害物へ")+OwnedSkillLabel(actorId,skill)+TEXT("しました");
            else DescribeHits();
        }
        SelectedSkill.reset(); Selected.reset(); Refresh();
    },preview.Legal);
}
