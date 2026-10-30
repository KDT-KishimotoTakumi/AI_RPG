#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Widgets/SWindow.h"
#include "Presentation/RunScreen.h"
#include "Application/Encounter.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

BEGIN_DEFINE_SPEC(FGirlsSpireShopUiSpec,"GirlsSpire.UI.Shop",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
    int CaptureIndex=0;
    bool Click(const char* id);
    std::shared_ptr<GirlsSpire::Run> Prepare(int coinsMode);
END_DEFINE_SPEC(FGirlsSpireShopUiSpec)
bool FGirlsSpireShopUiSpec::Click(const char* id)
{
    auto target=Driver->FindElement(By::Id(id));
    if(!target->IsVisible()) {
        auto scroll=Driver->FindElement(By::Id("run.scroll"));
        scroll->ScrollToBeginning();Async(EAsyncExecution::TaskGraphMainThread,[]{}).Wait();
        if(!target->IsVisible()) {scroll->ScrollToEndUntil(By::Id(id));Async(EAsyncExecution::TaskGraphMainThread,[]{}).Wait();}
    }
    return TestTrue(*FString::Printf(TEXT("商店操作 %hs"),id),ClickAndDrain(target));
}
std::shared_ptr<GirlsSpire::Run> FGirlsSpireShopUiSpec::Prepare(int mode)
{
    using namespace GirlsSpire;
    for(int seed=0;seed<100;++seed) {
        auto run=std::make_shared<Run>(seed,Difficulty::Beginner,HeroRole::Sword);
        for(int step=0;step<24 && run->Snapshot().Phase==RunPhase::Route;++step) {
            int index=0;
            for(int i=0;i<static_cast<int>(run->Snapshot().Choices.size());++i)
                if(run->Snapshot().Choices[i].Kind==NodeKind::Shop) index=i;
            if(!run->ChooseNode(run->Snapshot().Revision,index))break;
            const auto kind=run->Snapshot().Active.Kind;
            if(kind==NodeKind::Shop && (mode==0 || run->Snapshot().Wallet.Coins>=250)) return run;
            const bool battle=kind==NodeKind::Battle || kind==NodeKind::Elite || kind==NodeKind::Rescue || kind==NodeKind::MidBoss || kind==NodeKind::FinalBoss;
            if(mode!=0 && battle) {
                auto end=Encounter::Create(run->Snapshot(),0).Snapshot();end.Outcome=BattleOutcome::Won;
                if(!run->Snapshot().Recruited.empty()) end.Units[0].Hp=0;
                run->CompleteBattle(run->Snapshot().Revision,end);
                run->ChooseRecipient(run->Snapshot().Revision,0);run->ClaimReward(run->Snapshot().Revision,0);
                if(run->Snapshot().PendingRescue>=0)run->ConverseRescue(run->Snapshot().Revision,ConversationChoice::Neutral);
            } else run->CompleteEvent(run->Snapshot().Revision);
        }
    }
    return nullptr;
}
void FGirlsSpireShopUiSpec::Define()
{
    BeforeEach([this] {
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        IConsoleManager::Get().ProcessUserConsoleInput(TEXT("GirlsSpire.Preview"),*GLog,nullptr);
        for(const auto& w:FSlateApplication::Get().GetTopLevelWindows()) if(w->GetTitle().ToString()==TEXT("Girls Spire — 周回プレビュー")) Window=w;
        Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    It(TEXT("護符の予告取消・購入・残高不足・退出"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this] {
        const auto run=Prepare(0);if(!TestTrue(TEXT("商店準備"),run!=nullptr)) return;
        Async(EAsyncExecution::TaskGraphMainThread,[this,run]{Window->SetContent(SNew(SRunScreen).InitialRun(run));}).Wait();
        if(!TestTrue(TEXT("商品選択肢"),Driver->FindElement(By::Id("shop.offer.0"))->Exists())) return;
        for(const char* id:{"shop.offer.0","shop.actor.0"}) if(!Click(id)) return;
        TestTrue(TEXT("費用予告"),Driver->FindElement(By::Id("shop.preview"))->GetText().ToString().Contains(TEXT("100銭")));
        if(!Click("shop.cancel")) return;
        TestTrue(TEXT("取消は残高維持"),Driver->FindElement(By::Id("run.wallet"))->GetText().ToString().Contains(TEXT("100銭")));
        for(const char* id:{"shop.offer.0","shop.actor.0","shop.confirm"}) if(!Click(id)) return;
        TestTrue(TEXT("購入結果"),Driver->FindElement(By::Id("shop.result"))->GetText().ToString().Contains(TEXT("購入しました")));
        for(const char* id:{"shop.offer.1","shop.actor.0"}) if(!Click(id)) return;
        TestFalse(TEXT("不足で確定不可"),Driver->FindElement(By::Id("shop.confirm"))->Exists());
        TestTrue(TEXT("不足理由"),Driver->FindElement(By::Id("shop.preview"))->GetText().ToString().Contains(TEXT("所持金不足")));
        if(!Click("shop.exit")) return;
        TestFalse(TEXT("退出済み"),Driver->FindElement(By::Id("shop.exit"))->Exists());
    });
    It(TEXT("回復・治療・傷薬在庫と売切れ"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this] {
        const auto run=Prepare(1);if(!TestTrue(TEXT("負傷商店準備"),run!=nullptr)) return;
        Async(EAsyncExecution::TaskGraphMainThread,[this,run]{Window->SetContent(SNew(SRunScreen).InitialRun(run));}).Wait();
        if(!TestTrue(TEXT("治療選択肢"),Driver->FindElement(By::Id("shop.offer.4"))->Exists())) return;
        for(const char* id:{"shop.offer.4","shop.actor.0","shop.confirm","shop.offer.3","shop.actor.0","shop.confirm"}) if(!Click(id)) return;
        for(int i=0;i<3;++i) if(!Click("shop.offer.2") || !Click("shop.confirm")) return;
        if(!Click("shop.offer.2")) return;
        TestTrue(TEXT("売切れ理由"),Driver->FindElement(By::Id("shop.preview"))->GetText().ToString().Contains(TEXT("売切れ")));
        TestFalse(TEXT("売切れ確定不可"),Driver->FindElement(By::Id("shop.confirm"))->Exists());
        TestTrue(TEXT("薬の共通所持表示"),Driver->FindElement(By::Id("run.wallet"))->GetText().ToString().Contains(TEXT("傷薬3/5")));
    });
    It(TEXT("何も買わず退出"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(90),[this] {
        const auto run=Prepare(0);if(!TestTrue(TEXT("商店準備"),run!=nullptr)) return;
        Async(EAsyncExecution::TaskGraphMainThread,[this,run]{Window->SetContent(SNew(SRunScreen).InitialRun(run));}).Wait();
        if(!TestTrue(TEXT("退出選択肢"),Driver->FindElement(By::Id("shop.exit"))->Exists()) || !Click("shop.exit")) return;
        TestTrue(TEXT("未購入の財布"),Driver->FindElement(By::Id("run.wallet"))->GetText().ToString().Contains(TEXT("100銭")));
        TestFalse(TEXT("店舗終了"),Driver->FindElement(By::Id("shop.offer.0"))->Exists());
    });
    It(TEXT("護符交換で旧装備の消失を確認する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this] {
        const auto run=Prepare(1);if(!TestTrue(TEXT("交換商店準備"),run!=nullptr)) return;
        Async(EAsyncExecution::TaskGraphMainThread,[this,run]{Window->SetContent(SNew(SRunScreen).InitialRun(run));}).Wait();
        for(const char* id:{"shop.offer.0","shop.actor.0","shop.confirm","shop.offer.1","shop.actor.0"}) if(!Click(id)) return;
        TestTrue(TEXT("交換内容の予告"),Driver->FindElement(By::Id("shop.preview"))->GetText().ToString().Contains(TEXT("力の護符→守りの護符。旧装備は消失")));
        if(!Click("shop.confirm") || !Click("shop.offer.1") || !Click("shop.actor.0")) return;
        TestTrue(TEXT("購入済み護符は売切れ"),Driver->FindElement(By::Id("shop.preview"))->GetText().ToString().Contains(TEXT("売切れ")));
        TestFalse(TEXT("再購入できない"),Driver->FindElement(By::Id("shop.confirm"))->Exists());
    });
    AfterEach([this] {
        if(Window.IsValid()) {
            TArray<FColor> pixels;FIntVector size;
            if(TestTrue(TEXT("商店画面撮影"),FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),pixels,size))) {
                TArray64<uint8> png;FImageUtils::PNGCompressImageArray(size.X,size.Y,pixels,png);
                const auto folder=FPaths::ProjectSavedDir()/TEXT("Screenshots");IFileManager::Get().MakeDirectory(*folder,true);
                TestTrue(TEXT("商店画像保存"),FFileHelper::SaveArrayToFile(png,*(folder/FString::Printf(TEXT("Shop-%d.png"),++CaptureIndex))));
            }
        }
        Driver.Reset();IAutomationDriverModule::Get().Disable();
        if(Window.IsValid()){FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}
#if defined(GIRLS_SPIRE_COVERAGE)
        TestEqual(TEXT("商店UI計測"),__llvm_profile_write_file(),0);
#endif
    });
}
