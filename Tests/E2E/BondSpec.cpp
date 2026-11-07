#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Widgets/SWindow.h"
#include "Presentation/RunScreen.h"
#include "Domain/RelationshipText.h"
#include "Domain/CharacterCatalog.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

BEGIN_DEFINE_SPEC(FGirlsSpireBondUiSpec,"GirlsSpire.UI.Bond",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
    int CaptureIndex=0;
    bool Click(const char* id);
    std::shared_ptr<GirlsSpire::Run> Prepare(int actor,int rank);
END_DEFINE_SPEC(FGirlsSpireBondUiSpec)
bool FGirlsSpireBondUiSpec::Click(const char* id)
{
    auto target=Driver->FindElement(By::Id(id));
    if(!target->IsVisible()) {
        auto scroll=Driver->FindElement(By::Id("run.scroll"));
        scroll->ScrollToBeginning();Async(EAsyncExecution::TaskGraphMainThread,[]{}).Wait();
        if(!target->IsVisible()) {scroll->ScrollToEndUntil(By::Id(id));Async(EAsyncExecution::TaskGraphMainThread,[]{}).Wait();}
    }
    return TestTrue(*FString::Printf(TEXT("絆操作 %hs"),id),ClickAndDrain(target));
}
std::shared_ptr<GirlsSpire::Run> FGirlsSpireBondUiSpec::Prepare(int actor,int rank)
{
    using namespace GirlsSpire;
    for(int seed=0;seed<1000;++seed) {
        Profile profile;profile.Bonds[actor-1]=rank;
        auto run=std::make_shared<Run>(seed,Difficulty::Beginner,HeroRole::Sword,profile);
        for(int step=0;step<24 && run->Snapshot().Phase==RunPhase::Route;++step) {
            int node=0;
            for(int i=0;i<static_cast<int>(run->Snapshot().Choices.size());++i) {
                const auto& c=run->Snapshot().Choices[i];
                if(c.Kind==NodeKind::Social || (c.Kind==NodeKind::Rescue && c.Candidate==actor-1)) node=i;
            }
            run->ChooseNode(run->Snapshot().Revision,node);
            const auto& s=run->Snapshot();
            if(s.Active.Kind==NodeKind::Social && run->MaxHp(actor)>0) return run;
            run->CompleteEvent(s.Revision);
        }
    }
    return nullptr;
}
void FGirlsSpireBondUiSpec::Define()
{
    BeforeEach([this] {
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        IConsoleManager::Get().ProcessUserConsoleInput(TEXT("GirlsSpire.Preview"),*GLog,nullptr);
        for(const auto& w:FSlateApplication::Get().GetTopLevelWindows()) if(w->GetTitle().ToString()==TEXT("Girls Spire — 周回プレビュー")) Window=w;
        Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    for(int actor=1;actor<=8;++actor) It(*FString::Printf(TEXT("人物%dの交流予告・取消・確定"),actor),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this,actor] {
        const int rank=actor==1?0:actor==8?99:25;
        const auto run=Prepare(actor,rank);if(!TestTrue(TEXT("交流準備"),run!=nullptr)) return;
        Async(EAsyncExecution::TaskGraphMainThread,[this,run]{Window->SetContent(SNew(SRunScreen).InitialRun(run));}).Wait();
        const auto actorId=FString::Printf(TEXT("social.actor.%d"),actor);
        if(!TestTrue(TEXT("加入済み相手ボタン"),Driver->FindElement(By::Id(*actorId))->Exists())) return;
        TestFalse(TEXT("主人公は相手でない"),Driver->FindElement(By::Id("social.actor.0"))->Exists());
        for(int id:run->Unrecruited()) TestFalse(TEXT("未加入者は非表示"),Driver->FindElement(By::Id(*FString::Printf(TEXT("social.actor.%d"),id+1)))->Exists());
        if(!Click(TCHAR_TO_UTF8(*actorId))) return;
        const auto* text=GirlsSpire::FindSocialDialogue(actor);if(!TestNotNull(TEXT("承認済み通常交流"),text))return;
        TestTrue(TEXT("交流相手の衣装画像を表示"),Driver->FindElement(By::Id("social.image"))->Exists());
        TestEqual(TEXT("人物別の台詞"),Driver->FindElement(By::Id("social.dialogue"))->GetText().ToString(),FString(UTF8_TO_TCHAR(GirlsSpire::FindCharacter(actor)->Name.data()))+TEXT("「")+UTF8_TO_TCHAR(text->Opening.data())+TEXT("」"));
        const TCHAR* effects[]={TEXT("（＋10）"),TEXT("（0）"),TEXT("（−5）")};
        for(int index=0;index<3;++index){const auto key=FString::Printf(TEXT("social.choice.%d"),index);TestEqual(TEXT("人物別3返答と既存効果"),Driver->FindElement(By::Id(*key))->GetText().ToString(),FString(UTF8_TO_TCHAR(text->Replies[index].data()))+effects[index]);}
        const int choice=actor%3;
        const auto choiceId=FString::Printf(TEXT("social.choice.%d"),choice);
        if(!Click(TCHAR_TO_UTF8(*choiceId))) return;
        const int expected=FMath::Clamp(rank+(choice==0?10:choice==2?-5:0),0,100);
        TestTrue(TEXT("絆前後値予告"),Driver->FindElement(By::Id("social.preview"))->GetText().ToString().Contains(FString::Printf(TEXT("%d→%d"),rank,expected)));
        TestTrue(TEXT("選択した本文の予告"),Driver->FindElement(By::Id("social.preview"))->GetText().ToString().Contains(UTF8_TO_TCHAR(text->Replies[choice].data())));
        if(!Click("social.cancel")) return;
        TestFalse(TEXT("取消で確定欄消失"),Driver->FindElement(By::Id("social.confirm"))->Exists());
        TestTrue(TEXT("取消後も同じ相手の選択"),Driver->FindElement(By::Id(*choiceId))->Exists());
        if(!Click("social.back") || !Click(TCHAR_TO_UTF8(*actorId)) || !Click(TCHAR_TO_UTF8(*choiceId)) || !Click("social.confirm")) return;
        TestTrue(TEXT("結果と前後値"),Driver->FindElement(By::Id("social.result"))->GetText().ToString().Contains(FString::Printf(TEXT("%d→%d"),rank,expected)));
        TestTrue(TEXT("経路へ戻る"),Driver->FindElement(By::Id("route.heading"))->Exists());
        TestFalse(TEXT("二重確定できない"),Driver->FindElement(By::Id("social.confirm"))->Exists());
        TestTrue(TEXT("加入者の絆表示"),Driver->FindElement(By::Id("run.growth"))->GetText().ToString().Contains(FString::Printf(TEXT("絆%d"),expected)));
    });
    It(TEXT("交流から敗北・次周へ絆を引き継ぐ"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this] {
        const auto run=Prepare(1,65);if(!TestTrue(TEXT("持越し準備"),run!=nullptr)) return;
        Async(EAsyncExecution::TaskGraphMainThread,[this,run]{Window->SetContent(SNew(SRunScreen).InitialRun(run));}).Wait();
        if(!TestTrue(TEXT("交流ボタン"),Driver->FindElement(By::Id("social.actor.1"))->Exists())) return;
        for(const char* id:{"social.actor.1","social.choice.0","social.confirm"}) if(!Click(id)) return;
        // ここからは画面上のノード名だけで戦闘を選び、進行検証の敗北入力を使う。
        bool found=false;
        for(int i=0;i<3;++i) {
            const auto id=FString::Printf(TEXT("route.node.%d"),i);const auto element=Driver->FindElement(By::Id(*id));
            if(!element->Exists()) continue;const auto label=element->GetText().ToString();
            if(label.Contains(TEXT("合戦")) || label.Contains(TEXT("強敵")) || label.Contains(TEXT("救出")) || label.Contains(TEXT("ボス"))) {if(!Click(TCHAR_TO_UTF8(*id))) return;found=true;break;}
        }
        if(!TestTrue(TEXT("敗北確認用戦闘経路"),found)) return;
        for(const char* id:{"preview.defeat","result.continue"}) if(!Click(id)) return;
        TestTrue(TEXT("タイトルの保持値"),Driver->FindElement(By::Id("title.bonds"))->GetText().ToString().Contains(TEXT("75")));
        for(const char* id:{"title.start","role.confirm","route.node.0","preview.complete"}) if(!Click(id)) return;
        TestTrue(TEXT("次周でも保持"),Driver->FindElement(By::Id("run.profile"))->GetText().ToString().Contains(TEXT("75")));
    });
    for(int rank:{0,24,25,49,50,74,75,99,100}) It(*FString::Printf(TEXT("周回の絆%dを実戦の信頼へ反映する"),rank),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this,rank] {
        const auto run=Prepare(1,rank);if(!TestTrue(TEXT("信頼準備"),run!=nullptr)) return;
        run->CompleteEvent(run->Snapshot().Revision);
        // 初期状態のみ戦闘入口へ進め、戦闘開始と手番送りはマウスで行う。
        for(int step=0;step<24 && run->Snapshot().Phase==GirlsSpire::RunPhase::Route;++step) {
            run->ChooseNode(run->Snapshot().Revision,0);const auto kind=run->Snapshot().Active.Kind;
            if(kind==GirlsSpire::NodeKind::Battle || kind==GirlsSpire::NodeKind::Elite || kind==GirlsSpire::NodeKind::Rescue || kind==GirlsSpire::NodeKind::MidBoss || kind==GirlsSpire::NodeKind::FinalBoss) break;
            run->CompleteEvent(run->Snapshot().Revision);
        }
        Async(EAsyncExecution::TaskGraphMainThread,[this,run]{Window->SetContent(SNew(SRunScreen).InitialRun(run));}).Wait();
        if(!Click("preview.battle") || !Click("battle.wait.north")) return;
        TestTrue(TEXT("実際の信頼"),Driver->FindElement(By::Id("intent.trust"))->GetText().ToString().Contains(FString::Printf(TEXT("絆（信頼）: %d"),rank)));
        const auto label=Driver->FindElement(By::Id("intent.forecast"))->GetText().ToString();
        TestTrue(TEXT("確定予告の境界"),label.Contains(TEXT("確定予定"))==(rank>=75));
        TestTrue(TEXT("確率表示の境界"),label.Contains(TEXT("%"))==(rank>=25 && rank<75));
        TestFalse(TEXT("実戦では検証用信頼変更なし"),Driver->FindElement(By::Id("intent.trust.0"))->Exists());
    });
    for(int actor=1;actor<=8;++actor) It(*FString::Printf(TEXT("人物%dの専用交流を取消・確定し次周も保持する"),actor),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this,actor] {
        const auto run=Prepare(actor,80);if(!TestTrue(TEXT("交流準備"),run!=nullptr))return;
        const auto before=run->Checkpoint();
        Async(EAsyncExecution::TaskGraphMainThread,[this,run]{Window->SetContent(SNew(SRunScreen).InitialRun(run));}).Wait();
        const auto actorId=FString::Printf(TEXT("social.actor.%d"),actor);
        if(!Click(TCHAR_TO_UTF8(*actorId)))return;
        if(!TestTrue(TEXT("絆80で専用交流を選択可能"),Driver->FindElement(By::Id("social.special"))->Exists()))return;
        if(!Click("social.special"))return;
        TestTrue(TEXT("完了の恒久保持とイベント消費を予告"),Driver->FindElement(By::Id("social.special.preview"))->Exists());
        TestTrue(TEXT("抑制時は要約を表示"),Driver->FindElement(By::Id("social.special.summary"))->Exists());
        TestFalse(TEXT("抑制時は本文を表示しない"),Driver->FindElement(By::Id("social.special.preview"))->GetText().ToString().Contains(UTF8_TO_TCHAR(GirlsSpire::FindSocialDialogue(actor)->Special.data())));
        TestTrue(TEXT("予告は記録しない"),run->Checkpoint()==before && !run->ProfileState().SpecialConversations[actor-1]);
        if(!Click("social.special.cancel"))return;
        TestTrue(TEXT("取消は時間も記録も変えない"),run->Checkpoint()==before && !run->ProfileState().SpecialConversations[actor-1]);
        if(!Click("social.special") || !Click("social.special.confirm"))return;
        auto elapsed=GirlsSpire::Run::Restore(before,{});elapsed->CompleteEvent(before.State.Revision);
        TestTrue(TEXT("1イベントだけ消費"),run->Checkpoint()==elapsed->Checkpoint());
        TestTrue(TEXT("対象だけ完了"),run->ProfileState().SpecialConversations[actor-1]);
        TestEqual(TEXT("専用交流で絆を重ねて加算しない"),run->ProfileState().Bonds[actor-1],80);
        auto revisited=GirlsSpire::Run::Restore(before,run->ProfileState());
        const auto next=std::make_shared<GirlsSpire::Run>(*revisited);
        Async(EAsyncExecution::TaskGraphMainThread,[this,next]{Window->SetContent(SNew(SRunScreen).InitialRun(next));}).Wait();
        if(!Click(TCHAR_TO_UTF8(*actorId)))return;
        TestFalse(TEXT("完了済みの専用交流を再消費しない"),Driver->FindElement(By::Id("social.special"))->Exists());
        TestTrue(TEXT("通常交流は継続可能"),Driver->FindElement(By::Id("social.choice.0"))->Exists());
    });
    It(TEXT("絆79では専用交流を表示しない"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this] {
        const auto run=Prepare(1,79);if(!TestTrue(TEXT("交流準備"),run!=nullptr))return;
        Async(EAsyncExecution::TaskGraphMainThread,[this,run]{Window->SetContent(SNew(SRunScreen).InitialRun(run));}).Wait();
        if(!Click("social.actor.1"))return;
        TestFalse(TEXT("絆79は対象外"),Driver->FindElement(By::Id("social.special"))->Exists());
    });
    AfterEach([this] {
        if(Window.IsValid()) {
            TArray<FColor> pixels;FIntVector size;
            if(TestTrue(TEXT("絆画面撮影"),FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),pixels,size))) {
                TArray64<uint8> png;FImageUtils::PNGCompressImageArray(size.X,size.Y,pixels,png);
                const auto folder=FPaths::ProjectSavedDir()/TEXT("Screenshots");IFileManager::Get().MakeDirectory(*folder,true);
                TestTrue(TEXT("絆画像保存"),FFileHelper::SaveArrayToFile(png,*(folder/FString::Printf(TEXT("Bond-%d.png"),++CaptureIndex))));
            }
        }
        Driver.Reset();IAutomationDriverModule::Get().Disable();
        if(Window.IsValid()){FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}
#if defined(GIRLS_SPIRE_COVERAGE)
        TestEqual(TEXT("絆UI計測"),__llvm_profile_write_file(),0);
#endif
    });
}
