#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MetaData/DriverIdMetaData.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/CombatScreen.h"
#include "Presentation/CharacterPortraits.h"
#include "Misc/Paths.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
namespace {
TSharedPtr<SWidget> MotionFindTag(const TSharedRef<SWidget>& widget,FName id)
{
    const auto tag=widget->GetMetaData<FDriverIdMetaData>();if(tag && tag->Id==id)return widget;
    auto* children=widget->GetChildren();for(int i=0;i<children->Num();++i)if(auto found=MotionFindTag(children->GetChildAt(i),id))return found;
    return nullptr;
}
void MotionWait(float seconds)
{
    auto done=MakeShared<TPromise<void>,ESPMode::ThreadSafe>();auto ready=done->GetFuture();
    Async(EAsyncExecution::TaskGraphMainThread,[done,seconds]{FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([done](float){done->SetValue();return false;}),seconds);}).Wait();ready.Wait();
}
}
BEGIN_DEFINE_SPEC(FGirlsSpireMotionUiSpec,"GirlsSpire.UI.UnitMotion",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    TSharedPtr<SWindow> Window;TSharedPtr<SCombatScreen> Screen;FAutomationDriverPtr Driver;
    void CaptureMotion(const TCHAR* name){Async(EAsyncExecution::TaskGraphMainThread,[this,name]{TArray<FColor> pixels;FIntVector size;if(!TestTrue(TEXT("2D画面の撮影"),FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),pixels,size)))return;TArray64<uint8> png;FImageUtils::PNGCompressImageArray(size.X,size.Y,pixels,png);const auto folder=FPaths::ProjectSavedDir()/TEXT("Screenshots");IFileManager::Get().MakeDirectory(*folder,true);TestTrue(TEXT("2D表示の証跡を保持"),FFileHelper::SaveArrayToFile(png,*(folder/name)));}).Wait();}
    FVector2D Translation(){FVector2D result=FVector2D::ZeroVector;Async(EAsyncExecution::TaskGraphMainThread,[this,&result]{const auto widget=MotionFindTag(Window.ToSharedRef(),"battle.unit.art.0");if(widget && widget->GetRenderTransform().IsSet())result=FVector2D(widget->GetRenderTransform()->GetTranslation());}).Wait();return result;}
END_DEFINE_SPEC(FGirlsSpireMotionUiSpec)
void FGirlsSpireMotionUiSpec::Define()
{
    BeforeEach([this]{FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();Driver=IAutomationDriverModule::Get().CreateDriver();Window=SNew(SWindow).ClientSize(FVector2D(1200,900));FSlateApplication::Get().AddWindow(Window.ToSharedRef());});
    LatentBeforeEach(WaitForSlateLayout);
    for(int mode=0;mode<3;++mode)It(*FString::Printf(TEXT("2D移動と待機・軽減・画像欠落%dで状態と再開を維持する"),mode),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this,mode]{
        using namespace GirlsSpire;BattleUnit hero;hero.At={1,1};hero.Speed=99;hero.Movement=6;hero.Hp=hero.MaxHp=40;
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={6,6};enemy.Speed=0;
        auto battle=MakeShared<Battle>(8,8,std::vector<Tile>(64),std::vector<BattleUnit>{hero,enemy},42);
        Preferences prefs;prefs.ReducedEffects=mode==1;auto portraits=mode==2?MakeShared<FCharacterPortraits>(FPaths::ProjectSavedDir()/TEXT("Tests/missing-motion-images")):MakeShared<FCharacterPortraits>();
        Async(EAsyncExecution::TaskGraphMainThread,[this,battle,prefs,portraits]{Window->SetContent(SAssignNew(Screen,SCombatScreen).Combat(battle).DisplayPreferences(prefs).Portraits(portraits));}).Wait();DrainSlateLayout();
        const auto initial=battle->Checkpoint();const auto a=Translation();MotionWait(.21f);const auto b=Translation();MotionWait(.21f);const auto c=Translation();
        if(mode==0)TestTrue(TEXT("実描画の人物が時間で揺れる"),!a.Equals(b,.0001) || !a.Equals(c,.0001));
        if(mode==1)TestTrue(TEXT("演出軽減では完全静止"),a.IsZero() && b.IsZero() && c.IsZero());
        TestTrue(TEXT("待機演出で状態/RNG不変"),battle->Checkpoint()==initial);
        auto expected=*battle;TestTrue(TEXT("同じ規則の移動を準備"),expected.Move(expected.Snapshot().Revision,0,{4,3}));
        if(!TestTrue(TEXT("移動先を実クリック"),ClickAndDrain(Driver->FindElement(By::Id("battle.cell.4.3")))))return;
        if(!TestTrue(TEXT("移動を実確定"),ClickAndDrain(Driver->FindElement(By::Id("battle.move.confirm")))))return;
        TestTrue(TEXT("確定結果と乱数は既存規則と一致"),battle->Checkpoint()==expected.Checkpoint());
        TestEqual(TEXT("通常は移動用2Dレイヤー、軽減は即時表示"),Driver->FindElement(By::Id("battle.motion.unit.0"))->Exists(),mode!=1);
        if(mode==0)CaptureMotion(TEXT("UnitMotion-Moving.png"));
        const auto control=Screen->ControlCheckpoint();const auto settled=battle->Checkpoint();MotionWait(.8f);DrainSlateLayout();
        Async(EAsyncExecution::TaskGraphMainThread,[this]{const auto ghost=MotionFindTag(Window.ToSharedRef(),"battle.motion.unit.0");TestTrue(TEXT("移動演出は有限時間で終了"),!ghost || !ghost->GetVisibility().IsVisible());}).Wait();
        TestTrue(TEXT("演出完了でも戦闘/作戦乱数不変"),battle->Checkpoint()==settled && Screen->ControlCheckpoint()==control);
        if(mode==0)CaptureMotion(TEXT("UnitMotion-Settled.png"));
        Async(EAsyncExecution::TaskGraphMainThread,[this,battle,prefs,portraits,control]{Window->SetContent(SAssignNew(Screen,SCombatScreen).Combat(battle).DisplayPreferences(prefs).Portraits(portraits).SavedControl(control));}).Wait();DrainSlateLayout();
        TestFalse(TEXT("画面再生成で移動を再生し直さない"),Driver->FindElement(By::Id("battle.motion.unit.0"))->Exists());
        TestTrue(TEXT("再開で確定位置/全状態不変"),battle->Checkpoint()==settled);
    });
    AfterEach([this]{Driver.Reset();Screen.Reset();if(Window){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}IAutomationDriverModule::Get().Disable();});
}
