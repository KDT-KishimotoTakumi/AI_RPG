#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "Presentation/GameAudio.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "Engine/Engine.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/GarbageCollection.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ScopeExit.h"
#if defined(GIRLS_SPIRE_COVERAGE)
extern "C" int __llvm_profile_write_file();
#endif
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGirlsSpireAudioQuietTest,"GirlsSpire.Audio.Quiet",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FGirlsSpireAudioQuietTest::RunTest(const FString&)
{
    FGameAudio audio(false);audio.Start();TestFalse(TEXT("素材欠落で停止せず無音"),audio.Ready());TestFalse(TEXT("欠落効果音を再生しない"),audio.Cue(EGameCue::Confirm));TestFalse(TEXT("不明な効果音を拒否"),audio.Cue(static_cast<EGameCue>(99)));
    GirlsSpire::Preferences prefs;prefs.BgmVolume=0;prefs.SfxVolume=0;TestTrue(TEXT("消音を適用できる"),audio.Apply(prefs));prefs.BgmVolume=-1;TestFalse(TEXT("不正音量を拒否"),audio.Apply(prefs));audio.Stop();return true;
}
BEGIN_DEFINE_SPEC(FGirlsSpireAudioEngineSpec,"GirlsSpire.Audio.Engine",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    std::shared_ptr<FGameAudio> Audio;
END_DEFINE_SPEC(FGirlsSpireAudioEngineSpec)
void FGirlsSpireAudioEngineSpec::Define()
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("GirlsSpireAudioTest")))return;
    It(TEXT("UE音声デバイスで再生・独立消音・再開・GC・停止する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(90),[this]{
        Async(EAsyncExecution::TaskGraphMainThread,[this]{
            if(!TestNotNull(TEXT("実音声デバイス"),GEngine->GetMainAudioDeviceRaw()))return;
            Audio=std::make_shared<FGameAudio>();if(!TestTrue(TEXT("3素材を読込"),Audio->Ready()))return;
            const auto wave=Cast<USoundWave>(Audio->Music()->GetSound());TestTrue(TEXT("BGM原本長さ20秒とループ設定"),wave && wave->bLooping && FMath::IsNearlyEqual(wave->Duration,20.f));
            Audio->Start();Audio->Start();TestTrue(TEXT("BGM再生要求を受理"),Audio->Music()->IsPlaying());TestTrue(TEXT("確認音を再生"),Audio->Cue(EGameCue::Confirm));
        }).Wait();if(!Audio || !Audio->Ready())return;
        auto done=MakeShared<TPromise<void>,ESPMode::ThreadSafe>();auto ready=done->GetFuture();Async(EAsyncExecution::TaskGraphMainThread,[done]{FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([done](float){done->SetValue();return false;}),.5f);}).Wait();ready.Wait();
        Async(EAsyncExecution::TaskGraphMainThread,[this]{
            TestTrue(TEXT("BGMは継続"),Audio->Music()->IsPlaying());TestFalse(TEXT("0.16秒の確認音は終了"),Audio->Effects()->IsPlaying());
            GirlsSpire::Preferences prefs;prefs.BgmVolume=37;prefs.SfxVolume=50;Audio->Apply(prefs);TestTrue(TEXT("実コンポーネントの独立音量"),FMath::IsNearlyEqual(Audio->Music()->VolumeMultiplier,.37f) && FMath::IsNearlyEqual(Audio->Effects()->VolumeMultiplier,.5f));
            TestTrue(TEXT("打撃音を再生"),Audio->Cue(EGameCue::Impact));CollectGarbage(RF_NoFlags);TestTrue(TEXT("GC後も所有する音声コンポーネントを保持"),IsValid(Audio->Music()) && IsValid(Audio->Effects()));
            prefs.BgmVolume=0;Audio->Apply(prefs);TestFalse(TEXT("BGMだけ消音停止"),Audio->Music()->IsPlaying());prefs.SfxVolume=0;Audio->Apply(prefs);TestFalse(TEXT("効果音も即時消音停止"),Audio->Effects()->IsPlaying());TestFalse(TEXT("消音中は再発音しない"),Audio->Cue(EGameCue::Impact));
            prefs.BgmVolume=100;Audio->Apply(prefs);TestTrue(TEXT("BGMを再開"),Audio->Music()->IsPlaying());Audio->Stop();TestFalse(TEXT("明示停止"),Audio->Music()->IsPlaying());
        }).Wait();
    });
    It(TEXT("増分GCの途中に作成した音声参照も完了後に保持する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(90),[this]{
        Async(EAsyncExecution::TaskGraphMainThread,[this]{
            auto* allow=IConsoleManager::Get().FindConsoleVariable(TEXT("gc.AllowIncrementalReachability"));
            auto* limit=IConsoleManager::Get().FindConsoleVariable(TEXT("gc.IncrementalReachabilityTimeLimit"));
            auto* delay=IConsoleManager::Get().FindConsoleVariable(TEXT("gc.DelayReachabilityIterations"));
            if(!TestNotNull(TEXT("増分GC設定"),allow) || !TestNotNull(TEXT("時間制限"),limit) || !TestNotNull(TEXT("反復遅延"),delay))return;
            const int previousAllow=allow->GetInt(),previousDelay=delay->GetInt();const float previousLimit=limit->GetFloat();
            ON_SCOPE_EXIT {
                delay->SetWithCurrentPriority(0);FinalizeIncrementalReachabilityAnalysis();
                allow->SetWithCurrentPriority(previousAllow);limit->SetWithCurrentPriority(previousLimit);delay->SetWithCurrentPriority(previousDelay);
            };
            Audio=std::make_shared<FGameAudio>();if(!TestTrue(TEXT("GC前に音素材を読込"),Audio->Ready()))return;
            Audio->Start();const TWeakObjectPtr<UAudioComponent> original=Audio->Music();
            allow->SetWithCurrentPriority(1);limit->SetWithCurrentPriority(0.000001f);delay->SetWithCurrentPriority(2);
            CollectGarbage(RF_NoFlags,false);
            if(!TestTrue(TEXT("実際に増分到達性解析が進行中"),IsIncrementalReachabilityAnalysisPending()))return;
            auto during=std::make_shared<FGameAudio>();during->Start();
            const TWeakObjectPtr<UAudioComponent> created=during->Music();
            TestTrue(TEXT("解析中も効果音を操作"),Audio->Cue(EGameCue::Impact));
            delay->SetWithCurrentPriority(0);FinalizeIncrementalReachabilityAnalysis();
            TestFalse(TEXT("到達性解析が完了"),IsIncrementalReachabilityAnalysisPending());
            TestTrue(TEXT("既存と途中作成の所有物を保持"),original.IsValid() && created.IsValid() && original.Get()==Audio->Music() && created.Get()==during->Music());
            TestTrue(TEXT("両方の3素材と再生を維持"),Audio->Ready() && during->Ready() && Audio->Music()->IsPlaying() && during->Music()->IsPlaying());
            during->Stop();Audio->Stop();
        }).Wait();
    });
    AfterEach([this]{Audio.reset();
#if defined(GIRLS_SPIRE_COVERAGE)
        // 短い専用試験も強制終了前に記録し、0バイトの原本を成功扱いにしない。
        TestEqual(TEXT("音声経路のカバレッジ記録"),__llvm_profile_write_file(),0);
#endif
    });
}
