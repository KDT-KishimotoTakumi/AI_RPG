#include "RunScreen.h"
#include "Widgets/SBoxPanel.h"

namespace {
const TCHAR* const EndingTitles[]={TEXT("名のない明日"),TEXT("茜さす旗"),TEXT("雪解けの水"),TEXT("名を呼べる朝"),
    TEXT("余白の祈り"),TEXT("風を通す窓"),TEXT("消えない火"),TEXT("二つの湯呑み"),TEXT("夜明けの鈴")};
const TCHAR* const EndingPages[]={
    TEXT("死者の城が沈んだ朝、黒鉄蓮は誰の名も書かれていない道標を見つけた。歴史に残る勝者になることより、隣を歩いた者たちを忘れないことを選んだ。\n\n別れは約束を消さなかった。旅の荷には仲間から受け取った手紙がある。蓮は自分の足で、まだ誰も奪っていない明日へ歩き出した。"),
    TEXT("城の外で、茜は戦のために掲げていた旗を畳んだ。「今度は、帰る場所を一緒に作りたい」。蓮がうなずくまで、彼女は静かに待っていた。\n\n二人が選んだのは、名を競う合戦ではなく荒れた畑を耕す日々だった。夕暮れに交わす言葉は少ない。それでも、翌朝も隣にいるという約束は毎日新しくなった。"),
    TEXT("澪は雪解けの水を手ですくい、これが城の見せる幻でないことを確かめた。蓮も同じ冷たさに触れ、二人で笑った。\n\n傷ついた旅人が訪れる小さな宿に、二つの灯りがともる。澪は誰かを救うために自分をすり減らす夜を少しずつ手放した。蓮は、彼女が休む時間を一緒に守ると決めた。"),
    TEXT("朧は最後の影が消えるのを見届けてから、自分の名を蓮に告げ直した。命令でも合図でもなく、ただ呼んでほしい名だった。\n\n朝市で隣を歩く二人を、追う者はいない。朧は時折、遠回りを選ぶ。帰り道がいくつあっても同じ家へ戻れることを、そのたびに確かめるように。"),
    TEXT("紫苑は歴史を書き換えていた帳面の最後に、何も書かなかった。「これからのことは、決めつけずにおきましょう」。蓮はその余白を閉じずに残した。\n\n二人の暮らしには、失敗も迷いも訪れる。けれど誰かの運命として押しつけられた日々ではない。相談し、選び直し、互いの意思で同じ明日を迎えた。"),
    TEXT("楓は古い屋敷の窓をすべて開けた。城へ向かった夜には言えなかった望みを、今は自分の声で蓮へ伝えられた。\n\n二人は互いの居場所を狭めないと約束した。離れて過ごす日にも、帰ってから聞かせたい話が増えていく。風の通る部屋で、次の旅の支度を一緒に整えた。"),
    TEXT("火乃香は工房の炉へ、新しい炭を足した。何かを壊すためではなく、明日の暮らしに使う道具を作るための火だった。\n\n蓮が手伝うと、火乃香は遠慮なく失敗を笑い、うまくできた時には真っ先に見せた。城の夜を越えた二人の間で、その火は競うものから分け合うものへ変わっていった。"),
    TEXT("千景は旅支度の横へ、二つの湯呑みを並べた。戦いのない朝にどう振る舞えばよいか、二人ともまだ慣れていなかった。\n\n言葉にできない日は、急いで答えを出さない。蓮と千景は、黙って隣に座る時間も約束の一部にした。湯気が消えるまでの静けさを、今度は誰にも奪わせなかった。"),
    TEXT("小夜の鈴が、城の崩れる音を追い払うように鳴った。蓮はその音に合わせて、彼女が差し出した手を取った。互いの意思を確かめる言葉は、もう戦の号令にかき消されない。\n\n次の祭りで小夜は、帰らぬ者への祈りと、生きている者の明日を舞った。見守る蓮へ笑いかけると、夜明けの風が二人の間を通り抜けた。")};
}
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
        AddAction(EndingTitles[id],*FString::Printf(TEXT("epilogue.record.%d"),id),[this,id]{ShowEpiloguePage(id,true);});
    AddAction(TEXT("タイトルへ戻る"),"epilogue.back",[this]{ShowTitle();});
}
void SRunScreen::ShowEpiloguePage(int actorId,bool archived)
{
    if(actorId<0 || actorId>8)return;
    if(archived?!RetainedProfile.Epilogues[actorId]:(!CurrentRun || !CurrentRun->ProfileForEpilogue(actorId)))return;
    Body->ClearChildren();AddText(EndingTitles[actorId],"epilogue.title",28);
    AddText(EndingPages[actorId],"epilogue.page",21);
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
