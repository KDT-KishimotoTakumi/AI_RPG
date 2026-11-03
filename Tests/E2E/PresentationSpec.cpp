#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/RunScreen.h"
#include "Presentation/CombatScreen.h"
#include "Presentation/SpireStyle.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Misc/Paths.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Framework/MetaData/DriverIdMetaData.h"
namespace {
bool PaintedWidget(const TSharedRef<SWidget>& widget,const FName id)
{
    if(!widget->GetVisibility().IsVisible())return false;
    const auto tag=widget->GetMetaData<FDriverIdMetaData>();
    if(tag && tag->Id==id)return widget->GetCachedGeometry().GetLocalSize().GetMin()>0;
    auto* children=widget->GetChildren();
    for(int i=0;i<children->Num();++i)if(PaintedWidget(children->GetChildAt(i),id))return true;
    return false;
}
}

BEGIN_DEFINE_SPEC(FGirlsSpirePresentationSpec,"GirlsSpire.UI.Presentation",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;TSharedPtr<SWindow> Window;
    bool Painted(const FName id){bool result=false;Async(EAsyncExecution::TaskGraphMainThread,[this,id,&result]{result=PaintedWidget(Window.ToSharedRef(),id);}).Wait();return result;}
    void Capture(const TCHAR* name){Async(EAsyncExecution::TaskGraphMainThread,[this,name]{TArray<FColor> pixels;FIntVector size;if(!TestTrue(TEXT("実画面の撮影"),FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),pixels,size)))return;TArray64<uint8> png;FImageUtils::PNGCompressImageArray(size.X,size.Y,pixels,png);const auto folder=FPaths::ProjectSavedDir()/TEXT("Screenshots");IFileManager::Get().MakeDirectory(*folder,true);TestTrue(TEXT("確認用画像を保持"),FFileHelper::SaveArrayToFile(png,*(folder/name)));}).Wait();}
END_DEFINE_SPEC(FGirlsSpirePresentationSpec)
void FGirlsSpirePresentationSpec::Define()
{
    BeforeEach([this]{FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();Driver=IAutomationDriverModule::Get().CreateDriver();Window=SNew(SWindow).ClientSize(FVector2D(1920,1080)).AutoCenter(EAutoCenter::None).ScreenPosition(FVector2D::ZeroVector).SaneWindowPlacement(false).AdjustInitialSizeAndPositionForDPIScale(false);FSlateApplication::Get().AddWindow(Window.ToSharedRef());});
    LatentBeforeEach(WaitForSlateLayout);
    It(TEXT("独自の城背景とタイトル・開始操作を表示する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this]{
        Async(EAsyncExecution::TaskGraphMainThread,[this]{
            const auto path=FPaths::ProjectContentDir()/TEXT("Art/Backgrounds/title-castle-v1.png");const auto image=FSpireStyle::Backdrop(path);
            TestTrue(TEXT("実画像を読み込み共有する"),image.IsValid() && image==FSpireStyle::Backdrop(path));
            Window->SetContent(SNew(SRunScreen).DevelopmentTools(false));
        }).Wait();DrainSlateLayout();
        TestTrue(TEXT("背景の実表示"),Painted("title.art"));
        TestEqual(TEXT("正式タイトルを表示"),Driver->FindElement(By::Id("title.heading"))->GetText().ToString(),FString(TEXT("GIRLS SPIRE")));
        Capture(TEXT("Presentation-Title.png"));
        TestTrue(TEXT("開始操作を維持"),ClickAndDrain(Driver->FindElement(By::Id("title.start"))));
        TestTrue(TEXT("従来の編成へ進む"),Driver->FindElement(By::Id("role.confirm"))->Exists());
    });
    It(TEXT("未配置の背景は安全に退避する"),[this]{TestFalse(TEXT("背景なしで不正なブラシを返さない"),FSpireStyle::Backdrop(FPaths::ProjectSavedDir()/TEXT("Tests/missing-title-art.png")).IsValid());});
    It(TEXT("盤面の人物駒とHP表示を追加して状態を変えない"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this]{
        using namespace GirlsSpire;BattleUnit hero;hero.At={1,1};hero.Hp=20;hero.MaxHp=40;hero.Speed=99;BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={3,1};enemy.Hp=enemy.MaxHp=40;
        auto battle=MakeShared<Battle>(8,8,std::vector<Tile>(64),std::vector<BattleUnit>{hero,enemy},42);const auto before=battle->Checkpoint();
        Async(EAsyncExecution::TaskGraphMainThread,[this,battle]{Window->SetContent(SNew(SCombatScreen).Combat(battle));}).Wait();DrainSlateLayout();
        TestTrue(TEXT("既存人物の画像を駒に使用"),Painted("battle.unit.art.0"));
        TestTrue(TEXT("駒のHPバー"),Painted("battle.unit.hpbar.0"));
        TestTrue(TEXT("HPの文字情報を維持"),Driver->FindElement(By::Id("battle.celllabel.1.1"))->GetText().ToString().Contains(TEXT("20")));
        Capture(TEXT("Presentation-Battle.png"));
        const auto toggle=Driver->FindElement(By::Id("battle.cues.monochrome"));if(!toggle->IsVisible()){Driver->FindElement(By::Id("battle.scroll"))->ScrollToEndUntil(By::Id("battle.cues.monochrome"));DrainSlateLayout();}
        if(!TestTrue(TEXT("単色切替を実操作"),ClickAndDrain(toggle)))return;
        TestFalse(TEXT("単色ではカラー画像に依存しない"),Painted("battle.unit.art.0"));
        TestTrue(TEXT("描画・切替で全状態と乱数不変"),battle->Checkpoint()==before);
    });
    AfterEach([this]{Driver.Reset();if(Window){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}IAutomationDriverModule::Get().Disable();});
}
