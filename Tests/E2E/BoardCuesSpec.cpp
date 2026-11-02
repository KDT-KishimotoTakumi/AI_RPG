#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Presentation/CombatScreen.h"
BEGIN_DEFINE_SPEC(FGirlsSpireBoardCuesSpec,"GirlsSpire.UI.BoardCues",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
END_DEFINE_SPEC(FGirlsSpireBoardCuesSpec)
void FGirlsSpireBoardCuesSpec::Define()
{
    BeforeEach([this]{
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        Window=SNew(SWindow).ClientSize(FVector2D(1200,1000));
        FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    for(int size=0;size<3;++size)It(*FString::Printf(TEXT("文字%dでも記号・単色・危険予測と移動操作を維持する"),size),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this,size]{
        using namespace GirlsSpire;
        BattleUnit hero;hero.At={1,1};hero.Speed=30;hero.Range=3;hero.Hp=hero.MaxHp=100;
        BattleUnit enemy;enemy.Id=100;enemy.At={3,1};enemy.Side=Faction::Enemy;enemy.Speed=10;enemy.Movement=0;
        auto battle=MakeShared<Battle>(8,8,std::vector<Tile>(64),std::vector<BattleUnit>{hero,enemy},42);
        const auto original=battle->Checkpoint();Preferences preferences;preferences.Text=static_cast<TextSize>(size);auto writes=std::make_shared<int>(0);
        Async(EAsyncExecution::TaskGraphMainThread,[this,battle,preferences,writes]{Window->SetContent(SNew(SCombatScreen).Combat(battle).DisplayPreferences(preferences).OnCheckpoint([writes](const SavedCombat&){++*writes;return true;}));}).Wait();DrainSlateLayout();
        const int initialWrites=*writes;
        const auto text=[this](const char* id){return Driver->FindElement(By::Id(id))->GetText().ToString();};
        TestTrue(TEXT("味方形状"),text("battle.cues.1.1").Contains(TEXT("○")));
        TestTrue(TEXT("敵形状と合法攻撃"),text("battle.cues.3.1").Contains(TEXT("△×")));
        TestTrue(TEXT("移動先記号"),text("battle.cues.1.0").Contains(TEXT("＋")));
        const auto click=[this](const char* id){auto target=Driver->FindElement(By::Id(id));if(!target->IsVisible()){Driver->FindElement(By::Id("battle.scroll"))->ScrollToEndUntil(By::Id(id));DrainSlateLayout();}return TestTrue(UTF8_TO_TCHAR(id),ClickAndDrain(target));};
        if(!click("battle.cues.monochrome") || !click("battle.cues.threats"))return;
        TestTrue(TEXT("単色でも陣営と攻撃対象を識別"),text("battle.cues.3.1").Contains(TEXT("△×")));
        TestTrue(TEXT("予測範囲の説明"),Driver->FindElement(By::Id("battle.cues.scope"))->Exists());
        TestTrue(TEXT("表示の切替は状態と乱数を変えない"),battle->Checkpoint()==original);
        TestEqual(TEXT("表示切替は保存を発生させない"),*writes,initialWrites);
        Async(EAsyncExecution::TaskGraphMainThread,[this,size]{
            TArray<FColor> pixels;FIntVector dimensions;
            if(!TestTrue(TEXT("実ウィンドウを撮影"),FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),pixels,dimensions)))return;
            TArray64<uint8> png;FImageUtils::PNGCompressImageArray(dimensions.X,dimensions.Y,pixels,png);
            const auto folder=FPaths::ProjectSavedDir()/TEXT("Screenshots");IFileManager::Get().MakeDirectory(*folder,true);
            TestTrue(TEXT("表示証跡を保存"),FFileHelper::SaveArrayToFile(png,*(folder/FString::Printf(TEXT("BoardCues-%d.png"),size))));
        }).Wait();
        if(!click("battle.cues.threats") || !click("battle.cues.monochrome"))return;
        Driver->FindElement(By::Id("battle.scroll"))->ScrollToBeginning();DrainSlateLayout();
        TestTrue(TEXT("拡大文字でもマスを操作可能"),ClickAndDrain(Driver->FindElement(By::Id("battle.cell.1.0"))));
        TestTrue(TEXT("予告段階では状態不変"),battle->Checkpoint()==original);
        const auto boardScroll=Driver->FindElement(By::Id("battle.board.scroll"));
        if(!TestTrue(TEXT("盤面の下端へ到達するスクロールがある"),boardScroll->Exists()))return;
        boardScroll->ScrollToEndUntil(By::Id("battle.cell.7.7"));DrainSlateLayout();
        TestTrue(TEXT("最下段を表示できる"),Driver->FindElement(By::Id("battle.cell.7.7"))->IsVisible());
        TestTrue(TEXT("最下段もクリックできる"),ClickAndDrain(Driver->FindElement(By::Id("battle.cell.7.7"))));
        TestTrue(TEXT("範囲外マスの選択は状態不変"),battle->Checkpoint()==original);
        Async(EAsyncExecution::TaskGraphMainThread,[this]{Window->SetContent(SNullWidget::NullWidget);}).Wait();
    });
    AfterEach([this]{Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}IAutomationDriverModule::Get().Disable();});
}
