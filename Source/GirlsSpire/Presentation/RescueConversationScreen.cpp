#include "RunScreen.h"
#include "Domain/RescueDialogue.h"
#include "Widgets/SBoxPanel.h"
void SRunScreen::RefreshRescueConversation()
{
    using namespace GirlsSpire;
    const auto& state=CurrentRun->Snapshot();const int actor=state.PendingRescue+1;
    AddText(ActorName(actor)+TEXT("を救出しました。"),"rescue.heading",26);
    AddText(TEXT("救出後の返答を一度だけ選びます。追加の日数・イベントは消費しません。確定後はやり直せません。"),"rescue.help",17);
    const auto* dialogue=FindRescueDialogue(actor);
    check(dialogue); // 救出人物IDは周回状態の生成・復元で検証済み。
    const auto content=AddDialoguePortrait(actor,CurrentRun->ProfileState().SelectedAppearances[actor],"rescue.image");
    TGuardValue<TSharedPtr<SVerticalBox>> target(Body,content);
    AddText(ActorName(actor)+TEXT("「")+UTF8_TO_TCHAR(dialogue->Opening.data())+TEXT("」"),"rescue.dialogue",18);
    const TCHAR* effects[]={TEXT("（＋10）"),TEXT("（0）"),TEXT("（−5）")};
    std::array<FString,3> replies;
    for(int i=0;i<3;++i)replies[i]=FString(UTF8_TO_TCHAR(dialogue->Replies[i].data()))+effects[i];
    if(SocialChoice>=0 && SocialChoice<3) {
        auto preview=*CurrentRun;const auto choice=static_cast<ConversationChoice>(SocialChoice);
        if(preview.ConverseRescue(state.Revision,choice)) {
            const int before=CurrentRun->ProfileState().Bonds[actor-1],after=preview.ProfileState().Bonds[actor-1];
            AddText(FString::Printf(TEXT("%s / 絆%d→%d（実変動%+d）\n追加の時間消費なし。確定後は取り消せません。"),*replies[SocialChoice],before,after,after-before),"rescue.preview",17);
            AddAction(TEXT("この返答を確定"),"rescue.confirm",[this,revision=state.Revision,choice,actor,before,after] {
                if(CurrentRun->ConverseRescue(revision,choice))SocialMessage=FString::Printf(TEXT("救出後の返答：%s / 絆%d→%d"),*ActorName(actor),before,after);
                SocialChoice=-1;RefreshRun();
            });
        }
        AddAction(TEXT("まだ返答せず戻る"),"rescue.cancel",[this]{SocialChoice=-1;RefreshRun();});return;
    }
    TSharedPtr<SHorizontalBox> row;Body->AddSlot().AutoHeight()[SAssignNew(row,SHorizontalBox)];
    for(int i=0;i<3;++i)AddAction(replies[i],*FString::Printf(TEXT("rescue.choice.%d"),i),[this,i]{SocialChoice=i;RefreshRun();},row);
}
