#include "RunScreen.h"
#include "Domain/RelationshipText.h"
#include "Widgets/SBoxPanel.h"

void SRunScreen::ShowEpilogueChoices()
{
    if(!CurrentRun)return;
    const auto choices=CurrentRun->EpilogueChoices();if(choices.empty())return;
    Body->ClearChildren();AddText(TEXT("この旅の後日談"),"epilogue.heading",28);
    AddText(TEXT("条件を満たした相手全員と、一人で歩む結末から一つを選べます。確定するとこの周回が終了し、選んだ後日談を本へ記録します。"),"epilogue.help",17);
    for(int id:choices)AddAction(id==0?TEXT("一人で歩む"):ActorName(id)+TEXT("との明日"),*FString::Printf(TEXT("epilogue.select.%d"),id),[this,id]{ShowEpiloguePage(id,false);});
}
void SRunScreen::ShowEpilogueLibrary()
{
    Body->ClearChildren();AddText(TEXT("後日談の本 ― 目録"),"epilogue.library.heading",28);
    for(int id=0;id<=8;++id)if(RetainedProfile.Epilogues[id])
        AddAction(UTF8_TO_TCHAR(GirlsSpire::FindEndingText(id)->Title.data()),*FString::Printf(TEXT("epilogue.record.%d"),id),[this,id]{ShowEpiloguePage(id,true);});
    AddAction(TEXT("タイトルへ戻る"),"epilogue.back",[this]{ShowTitle();});
}
void SRunScreen::ShowEpiloguePage(int actorId,bool archived)
{
    if(actorId<0 || actorId>8)return;
    if(archived?!RetainedProfile.Epilogues[actorId]:(!CurrentRun || !CurrentRun->ProfileForEpilogue(actorId)))return;
    const auto* text=GirlsSpire::FindEndingText(actorId);check(text);
    Body->ClearChildren();AddText(UTF8_TO_TCHAR(text->Title.data()),"epilogue.title",28);
    AddText(UTF8_TO_TCHAR(text->Page.data()),"epilogue.page",21);
    AddText(TEXT("―― 一頁 / 一頁 ――"),"epilogue.page.number",15);
    if(archived) {
        AddAction(TEXT("目録へ戻る"),"epilogue.library",[this]{ShowEpilogueLibrary();});
        AddAction(TEXT("タイトルへ戻る"),"epilogue.back",[this]{ShowTitle();});
    } else {
        AddAction(TEXT("この後日談を記録して周回を終える"),"epilogue.confirm",[this,actorId]{ConfirmEpilogue(actorId);});
        AddAction(TEXT("まだ確定せず選び直す"),"epilogue.cancel",[this]{ShowEpilogueChoices();});
    }
}
void SRunScreen::ConfirmEpilogue(int actorId)
{
    if(!CurrentRun || (PendingEpilogue && *PendingEpilogue!=actorId))return;
    const auto profile=CurrentRun->ProfileForEpilogue(actorId);if(!profile)return;
    PendingEpilogue=actorId;
    // 周回消去と記録を同じ取引へ渡す。失敗中は元の周回と選択を保持する。
    if(!PersistState({},*profile)){ShowSaveError([this,actorId]{ConfirmEpilogue(actorId);});return;}
    RetainedProfile=*profile;CurrentRun.reset();PendingEpilogue.reset();
    ShowEpiloguePage(actorId,true);
}
