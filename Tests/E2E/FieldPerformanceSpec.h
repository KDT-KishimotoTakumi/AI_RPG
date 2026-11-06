#pragma once
#include "Rendering/SlateRenderer.h"
#include "HAL/CriticalSection.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeLock.h"
#include "Domain/CharacterCatalog.h"
#include "Domain/EnemyCatalog.h"
#include <memory>
#include <vector>

namespace {
struct FieldFrameSample {double Interval,Measured;};
class FieldFrameCollector {
public:
    explicit FieldFrameCollector(double now):Begin(now+5),End(now+35){}
    const double Begin,End;
    void Record(double now){
        FScopeLock lock(&Guard);if(Complete)return;
        if(Previous>0 && now>Begin){const double part=FMath::Min(now,End)-FMath::Max(Previous,Begin);if(part>0)Samples.push_back({now-Previous,part});}
        Previous=now;if(now>=End)Complete=true;
    }
    bool Done()const{FScopeLock lock(&Guard);return Complete;}
    std::vector<FieldFrameSample> Copy()const{FScopeLock lock(&Guard);return Samples;}
private:
    mutable FCriticalSection Guard;
    double Previous=0;
    bool Complete=false;
    std::vector<FieldFrameSample> Samples;
};
}
BEGIN_DEFINE_SPEC(FGirlsSpireFieldPerformanceSpec,"GirlsSpire.Performance.Field",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    TSharedPtr<SWindow> Window;
    FDelegateHandle RenderHandle;
    void StopSampling(){if(RenderHandle.IsValid()){FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().Remove(RenderHandle);RenderHandle.Reset();}}
END_DEFINE_SPEC(FGirlsSpireFieldPerformanceSpec)
void FGirlsSpireFieldPerformanceSpec::Define()
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("GirlsSpirePerformanceTest")))return;
    BeforeEach([this]{Window=SNew(SWindow).ClientSize(FVector2D(1920,1080)).AutoCenter(EAutoCenter::None).ScreenPosition(FVector2D::ZeroVector).SaneWindowPlacement(false).AdjustInitialSizeAndPositionForDPIScale(false);FSlateApplication::Get().AddWindow(Window.ToSharedRef());});
    LatentBeforeEach(WaitForSlateLayout);
    for(int enemies:{3,10})It(*FString::Printf(TEXT("5対%dの実描画提出間隔を30秒測定する"),enemies),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(90),[this,enemies]{
#if defined(GIRLS_SPIRE_COVERAGE) && GIRLS_SPIRE_COVERAGE
        AddError(TEXT("性能検証は計装なしの通常MSVCビルドで実行してください。"));return;
#endif
        FieldFrameCollector boundary(0);boundary.Record(4.9);
        if(!TestTrue(TEXT("暖機中の描画間隔を集計しない"),boundary.Copy().empty()))return;
        boundary.Record(5.1);boundary.Record(35.1);const auto clipped=boundary.Copy();
        if(!TestTrue(TEXT("暖機/終了をまたぐ遅いフレームも30秒全体へ正しく配分"),boundary.Done() && clipped.size()==2 && FMath::Abs(clipped[0].Measured-.1)<1.e-8 && FMath::Abs(clipped[1].Measured-29.9)<1.e-8))return;
        boundary.Record(100);if(!TestTrue(TEXT("計測終了後の描画を加算しない"),boundary.Copy().size()==clipped.size()))return;
        using namespace GirlsSpire;const int size=enemies==10?12:8;std::vector<BattleUnit> units;
        for(int id=0;id<5;++id)units.push_back(CreateCharacter(id,{1,id+1}));
        for(int id=0;id<enemies;++id)units.push_back(CreateEnemy(static_cast<EnemyKind>(id+1),100+id,{size-3+id/5,id%5+1}));
        auto battle=MakeShared<Battle>(size,size,std::vector<Tile>(size*size),units,42);const auto original=battle->Checkpoint();
        std::shared_ptr<FieldFrameCollector> collector;
        Async(EAsyncExecution::TaskGraphMainThread,[this,battle,&collector]{
            Window->SetContent(SNew(SCombatScreen).Combat(battle).FullField(true));
            const auto actual=Window->GetClientSizeInScreen();TestTrue(TEXT("性能計測の実クライアントは1920×1080"),FMath::IsNearlyEqual(double(actual.X),1920.,.5) && FMath::IsNearlyEqual(double(actual.Y),1080.,.5));
            collector=std::make_shared<FieldFrameCollector>(FPlatformTime::Seconds());const auto target=Window.Get();
            RenderHandle=FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().AddLambda([collector,target](SWindow& rendered,const FTextureRHIRef&){if(&rendered==target)collector->Record(FPlatformTime::Seconds());});
        }).Wait();
        while(!collector->Done() && FPlatformTime::Seconds()<collector->End+10)FPlatformProcess::Sleep(.05f);
        Async(EAsyncExecution::TaskGraphMainThread,[this]{StopSampling();}).Wait();
        if(!TestTrue(TEXT("対象ウィンドウの実描画通知を計測終了まで受信"),collector->Done()))return;
        const auto samples=collector->Copy();double elapsed=0,slow=0,frames=0,worst=0;FString csv=TEXT("frame,interval_ms,measured_ms\n");int index=0;
        for(const auto sample:samples){elapsed+=sample.Measured;frames+=sample.Measured/sample.Interval;worst=FMath::Max(worst,sample.Interval);if(sample.Interval>1.0/30)slow+=sample.Measured;csv+=FString::Printf(TEXT("%d,%.6f,%.6f\n"),++index,sample.Interval*1000,sample.Measured*1000);}
        if(!TestTrue(TEXT("30秒全体を欠落なく測定"),samples.size()>1 && FMath::Abs(elapsed-30)<.01))return;
        const double fps=frames/elapsed,ratio=slow/elapsed;
        const auto dir=FPaths::ProjectSavedDir()/TEXT("Tests/FieldPerformance")/FGuid::NewGuid().ToString(EGuidFormats::Digits);IFileManager::Get().MakeDirectory(*dir,true);
        const auto summary=FString::Printf(TEXT("{\"scope\":\"editor-offscreen-backbuffer-ready\",\"width\":1920,\"height\":1080,\"allies\":5,\"enemies\":%d,\"warmupSeconds\":5,\"measuredSeconds\":%.6f,\"samples\":%d,\"averageFps\":%.6f,\"below30TimeRatio\":%.9f,\"worstMs\":%.6f,\"normal60TargetMet\":%s}\n"),enemies,elapsed,index,fps,ratio,worst*1000,fps>=60?TEXT("true"):TEXT("false"));
        TestTrue(TEXT("全フレームCSVを保全"),FFileHelper::SaveStringToFile(csv,*(dir/TEXT("frames.csv"))));
        TestTrue(TEXT("性能集計を保全"),FFileHelper::SaveStringToFile(summary,*(dir/TEXT("summary.json"))));
        AddInfo(FString::Printf(TEXT("5対%d: 平均%.2ffps / 30fps未満の時間%.4f%% / 最大%.2fms / %s"),enemies,fps,ratio*100,worst*1000,*dir));
        TestTrue(TEXT("描画計測で戦闘/乱数を変更しない"),battle->Checkpoint()==original);
        if(enemies==10)TestTrue(TEXT("最大編成で30fps未満の時間が1%未満"),ratio<.01);
    });
    AfterEach([this]{StopSampling();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}});
}
