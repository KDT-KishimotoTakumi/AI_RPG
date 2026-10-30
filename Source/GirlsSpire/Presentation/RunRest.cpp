#include "RunScreen.h"
void SRunScreen::RefreshRest()
{
    const auto& state=CurrentRun->Snapshot();
    ShowGrowth();
    AddText(TEXT("通貨不要・1イベント。全員回復か、1人の負傷を1段階治療します。"),"rest.help",16);
    AddAction(TEXT("全員のHPを最大HPの50%回復"),"rest.heal",[this]{SelectedRest=-1;RefreshRun();});
    bool injured=false;
    for(int id=0;id<=8;++id) if(CurrentRun->MaxHp(id)>0 && state.Health[id].Wounds>0) {
        injured=true;
        AddAction(ActorName(id)+TEXT("の負傷を1段階治療"),*FString::Printf(TEXT("rest.treat.%d"),id),[this,id]{SelectedRest=id;RefreshRun();});
    }
    if(!injured) AddText(TEXT("治療が必要な負傷者はいません。"),"rest.no-injury",15);
    if(SelectedRest==-2) return;
    // コピーへ同じルールを適用して予告する。取消は本体の状態を変えない。
    auto preview=*CurrentRun;
    if(!preview.Rest(state.Revision,SelectedRest)) return;
    FString label;
    for(int id=0;id<=8;++id) if(CurrentRun->MaxHp(id)>0 && (SelectedRest==-1 || SelectedRest==id)) {
        label+=FString::Printf(TEXT("%s：HP%d→%d / 最大HP%d→%d / 負傷段階%d→%d\n"),*ActorName(id),
            CurrentRun->CurrentHp(id),preview.CurrentHp(id),CurrentRun->MaxHp(id),preview.MaxHp(id),
            state.Health[id].Wounds,preview.Snapshot().Health[id].Wounds);
    }
    AddText(label+TEXT("段階4＝重傷、3～1＝通常負傷数、0＝負傷なし。治療だけでは現在HPは増えません。"),"rest.preview",16);
    AddAction(TEXT("この休息を確定"),"rest.confirm",[this,revision=state.Revision,patient=SelectedRest] {
        CurrentRun->Rest(revision,patient);SelectedRest=-2;RefreshRun();
    });
    AddAction(TEXT("取消（イベントは消費しない）"),"rest.cancel",[this]{SelectedRest=-2;RefreshRun();});
}
