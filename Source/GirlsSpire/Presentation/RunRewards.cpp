#include "RunScreen.h"
#include "CharacterPortraits.h"
#include <algorithm>
FString SRunScreen::ActorName(int id)
{
    return FCharacterPortraits::Name(id);
}
namespace {
FString RewardLabel(GirlsSpire::Reward reward)
{
    const TCHAR* names[]={TEXT("最大HP＋10（現在HPも＋10、最大まで）"),TEXT("攻撃＋2"),TEXT("防御＋1"),TEXT("素早さ＋1"),
        TEXT("薙ぎ払い：通常射程・敵中心と上下左右に通常攻撃力 / 再使用待ち3 / 転・結"),
        TEXT("遠当て：射程＋2・敵1体に通常攻撃力 / 再使用待ち2 / 承・結"),
        TEXT("癒しの術：射程3・生存味方を最大40回復 / 再使用待ち3 / 承・転")};
    return names[static_cast<int>(reward)];
}
}
void SRunScreen::ShowGrowth()
{
    FString label=TEXT("周回内の成長（次の戦闘へ反映）\n");
    const auto& state=CurrentRun->Snapshot();
    for(int id=0;id<9;++id) {
        if(id!=0 && std::find(state.Recruited.begin(),state.Recruited.end(),id-1)==state.Recruited.end()) continue;
        const auto& g=state.Growth[id];
        const auto& level=state.Levels[id];
        const FString xp=level.Level==10?TEXT("MAX"):FString::Printf(TEXT("%d/200"),level.Experience);
        const FString bond=id>0?FString::Printf(TEXT(" 絆%d"),CurrentRun->ProfileState().Bonds[id-1]):TEXT("");
        label+=FString::Printf(TEXT("%s%s：Lv%d 経験値%s / HP%d/%d %s\n  攻撃＋%d 防御＋%d 素早さ＋%d 追加技%d\n"),*ActorName(id),*bond,level.Level,*xp,
            CurrentRun->CurrentHp(id),CurrentRun->MaxHp(id),
            state.Health[id].Wounds==4?TEXT("重傷"):*FString::Printf(TEXT("負傷%d"),state.Health[id].Wounds),
            g.Bonuses[1]+level.Level-1+(state.Accessories[id]==GirlsSpire::Accessory::Power?3:0),g.Bonuses[2]+level.Level-1+(state.Accessories[id]==GirlsSpire::Accessory::Guard?2:0),g.Bonuses[3],static_cast<int>(std::count(g.Learned.begin(),g.Learned.end(),true)));
    }
    AddText(label,"run.growth",14);
}
void SRunScreen::RefreshRewards()
{
    const auto& state=CurrentRun->Snapshot();
    AddText(TEXT("勝利報酬：育てる人物を1人選び、三択から1つ取得"),"reward.heading",23);
    if(state.RewardRecipient<0) {
        AddText(TEXT("人物を確定すると候補が表示されます。以後、人物変更・引き直しはできません。"),"reward.notice",16);
        for(int id=0;id<9;++id) {
            if(id!=0 && std::find(state.Recruited.begin(),state.Recruited.end(),id-1)==state.Recruited.end()) continue;
            AddAction(ActorName(id)+TEXT("を受取人に確定"),*FString::Printf(TEXT("reward.recipient.%d"),id),[this,id,revision=state.Revision] {
                CurrentRun->ChooseRecipient(revision,id);SelectedReward=-1;RefreshRun();
            });
        }
        return;
    }
    AddText(ActorName(state.RewardRecipient)+TEXT("の三択（受取人確定済み）"),"reward.locked",19);
    for(int i=0;i<static_cast<int>(state.Rewards.size());++i)
        AddAction(RewardLabel(state.Rewards[i]),*FString::Printf(TEXT("reward.choice.%d"),i),[this,i] {SelectedReward=i;RefreshRun();});
    if(SelectedReward<0) return;
    AddText(TEXT("取得効果：")+RewardLabel(state.Rewards[SelectedReward]),"reward.preview",17);
    AddAction(TEXT("この報酬を取得"),"reward.confirm",[this,revision=state.Revision,index=SelectedReward] {
        CurrentRun->ClaimReward(revision,index);SelectedReward=-1;RefreshRun();
    });
    AddAction(TEXT("効果確認を閉じる（三択は変わりません）"),"reward.cancel",[this] {SelectedReward=-1;RefreshRun();});
}
