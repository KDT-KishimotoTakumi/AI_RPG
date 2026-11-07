#include "RunScreen.h"
#include "Domain/RelationshipText.h"

namespace {
constexpr int SpecialWarning=-2;
constexpr int SpecialSummary=-3;
constexpr int SpecialFullText=-4;
}
FString SRunScreen::ProfileLabel() const
{
    const auto& profile=CurrentRun?CurrentRun->ProfileState():RetainedProfile;
    FString label=SaveService?TEXT("絆・専用交流（保存済み／次周へ保持）: "):TEXT("絆・専用交流（このプレビュー内で保持）: ");
    for(int id=1;id<=8;++id) label+=FString::Printf(TEXT("%s %d%s　"),*ActorName(id),profile.Bonds[id-1],
        profile.SpecialConversations[id-1]?TEXT("・専用交流済"):TEXT(""));
    return label;
}
void SRunScreen::RefreshSocial()
{
    using namespace GirlsSpire;
    const auto& state=CurrentRun->Snapshot();
    AddText(TEXT("加入済みの仲間1人と交流します。確定すると1イベントが終わり、やり直せません。"),"social.help",15);
    if(SocialActor<0) {
        for(int candidate:state.Recruited) {
            const int id=candidate+1;
            AddAction(FString::Printf(TEXT("%sと話す（絆%d）"),*ActorName(id),CurrentRun->ProfileState().Bonds[candidate]),
                *FString::Printf(TEXT("social.actor.%d"),id),[this,id]{SocialActor=id;SocialChoice=-1;RefreshRun();});
        }
        return;
    }
    const auto* text=FindSocialDialogue(SocialActor);
    check(text); // 交流相手は検証済みの加入者IDだけ。
    const auto content=AddDialoguePortrait(SocialActor,CurrentRun->ProfileState().SelectedAppearances[SocialActor],"social.image");
    TGuardValue<TSharedPtr<SVerticalBox>> target(Body,content);
    AddText(ActorName(SocialActor)+TEXT("「")+UTF8_TO_TCHAR(text->Opening.data())+TEXT("」"),"social.dialogue",18);
    if(CurrentRun->CanSpecialConverse(SocialActor)) {
        if(SocialChoice==SpecialWarning) {
            AddText(TEXT("この先には、成人同士の親密な会話が含まれます。要約を選んでも交流の結果は同じです。"),"social.special.warning",18);
            AddAction(TEXT("会話を読む"),"social.special.read",[this]{SocialChoice=SpecialFullText;RefreshRun();});
            AddAction(TEXT("要約を読む"),"social.special.skip",[this]{SocialChoice=SpecialSummary;RefreshRun();});
            AddAction(TEXT("交流に戻る"),"social.special.cancel",[this]{SocialChoice=-1;RefreshRun();});
            return;
        }
        if(SocialChoice==SpecialSummary || SocialChoice==SpecialFullText) {
            auto preview=*CurrentRun;
            if(preview.SpecialConverse(state.Revision,SocialActor)) {
                if(SocialChoice==SpecialSummary) {
                    AddText(TEXT("要約：二人は互いの意思を確かめ、城を出た後も話し合いながら過ごす約束を交わします。"),"social.special.summary",18);
                }
                const FString dialogue=SocialChoice==SpecialFullText ? FString(UTF8_TO_TCHAR(text->Special.data()))+TEXT("\n\n") : FString();
                AddText(dialogue+TEXT("1イベントを消費し、専用交流の完了を次周以降も保持します。絆の追加変動はありません。確定後は取り消せません。"),"social.special.preview",18);
                AddAction(TEXT("互いの約束を確定"),"social.special.confirm",[this,revision=state.Revision,actor=SocialActor] {
                    if(CurrentRun->SpecialConverse(revision,actor))SocialMessage=ActorName(actor)+TEXT("との約束を記録しました。専用交流の完了は次周へ引き継ぎます。");
                    SocialActor=-1;SocialChoice=-1;RefreshRun();
                });
            }
            AddAction(TEXT("まだ約束せず戻る"),"social.special.cancel",[this]{SocialChoice=-1;RefreshRun();});
            return;
        }
        AddAction(TEXT("専用交流：二人の約束"),"social.special",[this]{SocialChoice=DisplayPreferences.SuppressIntimacy ? SpecialSummary : SpecialWarning;RefreshRun();});
    }
    const TCHAR* effects[]={TEXT("（＋10）"),TEXT("（0）"),TEXT("（−5）")};
    std::array<FString,3> choices;
    for(int i=0;i<3;++i)choices[i]=FString(UTF8_TO_TCHAR(text->Replies[i].data()))+effects[i];
    if(SocialChoice>=0) {
        const auto choice=static_cast<ConversationChoice>(SocialChoice);
        auto preview=*CurrentRun;
        const bool legal=preview.Converse(state.Revision,SocialActor,choice);
        if(legal) {
            const int before=CurrentRun->ProfileState().Bonds[SocialActor-1],after=preview.ProfileState().Bonds[SocialActor-1];
            AddText(FString::Printf(TEXT("%s / 絆%d→%d（実変動%+d）\n会話1回でイベント終了。確定後は取り消せません。"),*choices[SocialChoice],before,after,after-before),"social.preview",17);
            AddAction(TEXT("この返答を確定"),"social.confirm",[this,revision=state.Revision,actor=SocialActor,choice,before,after] {
                if(CurrentRun->Converse(revision,actor,choice)) {
                    const TCHAR* reason=choice==ConversationChoice::Warm?TEXT("支えを感じた"):choice==ConversationChoice::Cold?TEXT("突き放されたと感じた"):TEXT("予定を確認した");
                    SocialMessage=FString::Printf(TEXT("%s：%s / 絆%d→%d"),*ActorName(actor),reason,before,after);
                }
                SocialActor=-1;SocialChoice=-1;RefreshRun();
            });
        }
        AddAction(TEXT("返答を取り消す（絆・時間は変化しない）"),"social.cancel",[this]{SocialChoice=-1;RefreshRun();});
    }
    for(int index=0;index<3;++index) AddAction(choices[index],*FString::Printf(TEXT("social.choice.%d"),index),[this,index]{SocialChoice=index;RefreshRun();});
    AddAction(TEXT("相手を選び直す"),"social.back",[this]{SocialActor=-1;SocialChoice=-1;RefreshRun();});
}
