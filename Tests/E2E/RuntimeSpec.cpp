#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Infrastructure/DiskSlotFiles.h"
#include "Infrastructure/PreferencesStore.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/SWindow.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "GenericPlatform/GenericApplication.h"
#include <algorithm>
#if defined(GIRLS_SPIRE_COVERAGE)
extern "C" int __llvm_profile_write_file();
#endif

BEGIN_DEFINE_SPEC(FGirlsSpireRuntimeSpec,"GirlsSpire.Runtime.GameEntry",EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    bool CheckFullscreen(){
        if(!FParse::Param(FCommandLine::Get(),TEXT("GirlsSpireRuntimeFullscreenTest")))return true;
        bool valid=false;
        Async(EAsyncExecution::TaskGraphMainThread,[this,&valid]{
            const auto window=GEngine && GEngine->GameViewport?GEngine->GameViewport->GetWindow():nullptr;
            if(!TestTrue(TEXT("実ゲームのウィンドウ"),window.IsValid()))return;
            const auto mode=window->GetWindowMode();const auto size=window->GetClientSizeInScreen();
            FDisplayMetrics display;FSlateApplication::Get().GetInitialDisplayMetrics(display);
            valid=TestTrue(TEXT("実フルスクリーンモード"),mode==EWindowMode::Fullscreen || mode==EWindowMode::WindowedFullscreen) &&
                TestTrue(TEXT("デスクトップを満たす実クライアント寸法"),display.PrimaryDisplayWidth>0 && display.PrimaryDisplayHeight>0 && size.X>=display.PrimaryDisplayWidth && size.Y>=display.PrimaryDisplayHeight);
            AddInfo(FString::Printf(TEXT("フルスクリーン実寸 %.0fx%.0f / デスクトップ %dx%d / モード%d"),double(size.X),double(size.Y),display.PrimaryDisplayWidth,display.PrimaryDisplayHeight,int(mode)));
        }).Wait();return valid;
    }
    bool CheckAudio(float bgm,float sfx,bool effects){
        bool valid=false;
        Async(EAsyncExecution::TaskGraphMainThread,[this,bgm,sfx,effects,&valid]{
            if(!TestTrue(TEXT("音声を有効にした通常ゲーム"),GEngine && GEngine->UseSound() && GEngine->GetMainAudioDeviceRaw()))return;
            UAudioComponent* music=nullptr;UAudioComponent* effect=nullptr;int musicCount=0;
            for(TObjectIterator<UAudioComponent> it;it;++it){
                if(!it->GetSound())continue;const auto path=it->GetSound()->GetPathName();
                if(path==TEXT("/Game/Audio/BGM_Castle_V2.BGM_Castle_V2")){music=*it;++musicCount;}
                if(path==TEXT("/Game/Audio/SFX_Confirm_V2.SFX_Confirm_V2"))effect=*it;
            }
            if(!TestTrue(TEXT("通常ゲームにBGM所有者が1つ"),musicCount==1 && music))return;
            valid=TestTrue(TEXT("BGM独立音量と再生/消音"),FMath::IsNearlyEqual(music->VolumeMultiplier,bgm) && music->IsPlaying()==(bgm>0));
            if(effects)valid=TestTrue(TEXT("実際の入力音源と独立効果音量"),effect && FMath::IsNearlyEqual(effect->VolumeMultiplier,sfx)) && valid;
        }).Wait();return valid;
    }
END_DEFINE_SPEC(FGirlsSpireRuntimeSpec)
void FGirlsSpireRuntimeSpec::Define()
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("GirlsSpireRuntimeTest"))) return;
    BeforeEach([this]{
        FSlateApplication::Get().CloseToolTip();
        if(FParse::Param(FCommandLine::Get(),TEXT("GirlsSpireRuntimeFullscreenTest")) && GEngine && GEngine->GameViewport){
            const auto window=GEngine->GameViewport->GetWindow();
            if(window){
                AddInfo(FString::Printf(TEXT("試験前ウィンドウ Visible=%d Minimized=%d Active=%d"),int(window->IsVisible()),int(window->IsWindowMinimized()),int(FSlateApplication::Get().GetActiveTopLevelWindow()==window)));
                if(window->IsWindowMinimized())window->Restore();
                window->ShowWindow();window->BringToFront(true);
            }
        }
        IAutomationDriverModule::Get().Enable();Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    if(!FParse::Param(FCommandLine::Get(),TEXT("GirlsSpireRuntimeAudioResume")))It(TEXT("通常ゲームの画面から出発・実戦・保存・中断できる"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(180),[this] {
        using namespace GirlsSpire;
        if(!TestTrue(TEXT("ゲーム用ビューポート"),GEngine && GEngine->GameViewport)) return;
        if(!TestTrue(TEXT("通常起動でタイトルを表示"),Driver->FindElement(By::Id("title.start"))->Exists())) {
            for(const char* id:{"save.load.error","save.version.error","save.error","title.settings.error"})
                AddInfo(FString::Printf(TEXT("起動診断 %hs=%d"),id,int(Driver->FindElement(By::Id(id))->Exists())));
            Async(EAsyncExecution::TaskGraphMainThread,[this]{
                const auto window=GEngine->GameViewport->GetWindow();int screens=0;
                TFunction<void(TSharedRef<SWidget>)> inspect=[&](TSharedRef<SWidget> widget){
                    if(widget->GetTypeAsString()==TEXT("SRunScreen"))++screens;
                    const auto children=widget->GetChildren();for(int i=0;i<children->Num();++i)inspect(children->GetChildAt(i));
                };
                if(window)inspect(window->GetContent());
                AddInfo(FString::Printf(TEXT("起動診断 RunScreen=%d WorldType=%d"),screens,int(GEngine->GameViewport->GetWorld()->WorldType)));
            }).Wait();return;
        }
        if(!CheckFullscreen())return;
        TestTrue(TEXT("実ゲームのマウス終了操作"),Driver->FindElement(By::Id("title.quit"))->Exists());
        const bool audio=FParse::Param(FCommandLine::Get(),TEXT("GirlsSpireRuntimeAudioTest"));
        if(audio && !CheckAudio(1.f,1.f,false))return;
        if(!audio){
            bool quiet=false;Async(EAsyncExecution::TaskGraphMainThread,[&quiet]{quiet=GEngine && !GEngine->UseSound() && !GEngine->GetMainAudioDeviceRaw();}).Wait();
            if(!TestTrue(TEXT("既定経路は実際に無音で音声デバイスを作らない"),quiet))return;
        }
        const auto click=[&](const char* id) {
            const auto target=Driver->FindElement(By::Id(id));
            if(!target->IsVisible()) {
                const auto outer=Driver->FindElement(By::Id("run.scroll"));
                if(outer->Exists()){outer->ScrollToBeginning();DrainSlateLayout();}
                const auto panel=Driver->FindElement(By::Id("battle.scroll"));
                if(!target->IsVisible()) {
                    if(panel->Exists()){panel->ScrollToBeginning();DrainSlateLayout();}
                    if(!target->IsVisible()){(panel->Exists()?panel:outer)->ScrollToEndUntil(By::Id(id));DrainSlateLayout();}
                }
            }
            const bool clicked=ClickAndDrain(target);
            if(!clicked)Async(EAsyncExecution::TaskGraphMainThread,[this]{const auto window=GEngine->GameViewport->GetWindow();TArray<FColor> pixels;FIntVector extent;if(!window || !FSlateApplication::Get().TakeScreenshot(window.ToSharedRef(),pixels,extent))return;TArray64<uint8> png;FImageUtils::PNGCompressImageArray(extent.X,extent.Y,pixels,png);const auto directory=FPaths::ProjectSavedDir()/TEXT("Screenshots");IFileManager::Get().MakeDirectory(*directory,true);const auto path=directory/FString::Printf(TEXT("RuntimeClickFailure-%s.png"),*FGuid::NewGuid().ToString(EGuidFormats::Digits));if(FFileHelper::SaveArrayToFile(png,*path))AddInfo(path);}).Wait();
            if(!TestTrue(*FString::Printf(TEXT("ゲーム操作 %hs"),id),clicked))return false;
            // 実IOが失敗した場合も、結果を作り直さず製品の再試行操作を検証する。
            if(Driver->FindElement(By::Id("save.error"))->Exists()) {
                AddWarning(FString::Printf(TEXT("%hs 後に保存エラーを表示。同じ内容の再試行を1回検証する。"),id));
                if(!TestTrue(TEXT("保存を画面から再試行"),ClickAndDrain(Driver->FindElement(By::Id("save.retry")))))return false;
                if(!TestFalse(TEXT("再試行後に保存停止を解消"),Driver->FindElement(By::Id("save.error"))->Exists()))return false;
            }
            return true;
        };
        if(audio){
            TestFalse(TEXT("正常音声で利用不可通知を表示しない"),Driver->FindElement(By::Id("audio.unavailable"))->IsVisible());
            if(!click("title.settings") || !click("settings.bgm.mute") || !click("settings.sfx.down") || !click("settings.save"))return;
            if(!CheckAudio(0.f,.9f,true))return;
            TestFalse(TEXT("消音でも利用不可通知を表示しない"),Driver->FindElement(By::Id("audio.unavailable"))->IsVisible());
            if(!click("title.settings") || !click("settings.bgm.up") || !click("settings.save") || !CheckAudio(.1f,.9f,true))return;
        }
        if(!click("title.start") || !click("role.confirm") || !click("route.node.0")) return;
        TestFalse(TEXT("進行検証の勝利入力を表示しない"),Driver->FindElement(By::Id("preview.complete"))->Exists());
        if(!click("battle.start")) return;
        FString root;FParse::Value(FCommandLine::Get(),TEXT("GirlsSpireRuntimeRoot="),root);root=FPaths::ConvertRelativePathToFull(root);
        const auto prefix=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("Saved/Tests/Runtime"));
        if(!TestTrue(TEXT("専用の保存先"),root!=prefix && FPaths::IsUnderDirectory(root,prefix))) return;
        DiskSlotFiles files{std::filesystem::path(*root)};SaveTransaction service(files,files);
        const auto entered=service.Load();
        if(!TestTrue(TEXT("実戦の開始を保存"),entered.Run && entered.Run->Combat)) return;
        if(audio && !CheckAudio(.1f,.9f,true))return;
        const auto battle=Battle::Restore(entered.Run->Combat->Battle);
        if(!TestTrue(TEXT("実戦復元"),battle.has_value())) return;
        const auto reachable=battle->Reachable(battle->Snapshot().ActiveId);
        const auto target=std::find_if(reachable.begin(),reachable.end(),[&](Position at){const auto preview=battle->PreviewMove(battle->Snapshot().ActiveId,at);return preview.Legal && preview.Damage==0;});
        if(!TestTrue(TEXT("安全な移動先"),target!=reachable.end())) return;
        const auto cell=FString::Printf(TEXT("battle.cell.%d.%d"),target->X,target->Y);
        if(!click(TCHAR_TO_UTF8(*cell)) || !click("battle.move.confirm")) return;
        const auto moved=service.Load();
        TestTrue(TEXT("通常起動の行動も保存される"),moved.Run && moved.Run->Combat && moved.Run->Combat->Battle.State.Revision>entered.Run->Combat->Battle.State.Revision);
        if(!click("save.suspend")) return;
        TestTrue(TEXT("通常起動の中断と再開操作"),Driver->FindElement(By::Id("title.resume"))->Exists());
        if(audio){PreferencesStore prefs(files);TestTrue(TEXT("独立音量を実ファイルへ保存"),prefs.Load().Issue==PreferencesIssue::None && prefs.Load().Value.BgmVolume==10 && prefs.Load().Value.SfxVolume==90);CheckAudio(.1f,.9f,true);}
    });
    if(FParse::Param(FCommandLine::Get(),TEXT("GirlsSpireRuntimeAudioResume")))It(TEXT("音声有効の別プロセスで音量と中断実戦を再開する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(180),[this]{
        using namespace GirlsSpire;FString root;FParse::Value(FCommandLine::Get(),TEXT("GirlsSpireRuntimeRoot="),root);root=FPaths::ConvertRelativePathToFull(root);
        if(!CheckFullscreen())return;
        const auto prefix=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("Saved/Tests/Runtime"));
        if(!TestTrue(TEXT("既存の専用保存先"),root!=prefix && FPaths::IsUnderDirectory(root,prefix)))return;
        DiskSlotFiles files{std::filesystem::path(*root)};SaveTransaction saves(files,files);PreferencesStore prefs(files);
        const auto before=saves.Load();const auto preferences=files.ReadPreferences();
        if(!TestTrue(TEXT("前プロセスの中断を検出"),before.Run && before.Run->Combat && before.Run->ResumeAvailable))return;
        TestTrue(TEXT("音量10/90を再読込"),prefs.Load().Issue==PreferencesIssue::None && prefs.Load().Value.BgmVolume==10 && prefs.Load().Value.SfxVolume==90);
        if(!CheckAudio(.1f,.9f,false))return;
        TestFalse(TEXT("再起動後も正常音声"),Driver->FindElement(By::Id("audio.unavailable"))->IsVisible());
        if(!TestTrue(TEXT("通常画面から中断再開"),ClickAndDrain(Driver->FindElement(By::Id("title.resume")))))return;
        TestTrue(TEXT("再開した実戦を表示"),Driver->FindElement(By::Id("battle.round"))->Exists());
        const auto after=saves.Load();
        auto expected=*before.Run;expected.ResumeAvailable=false;
        TestTrue(TEXT("実戦全状態・乱数・恒久記録を維持"),after.Run && *after.Run==expected && after.Permanent==before.Permanent);
        TestTrue(TEXT("再開で独立設定を書換えない"),files.ReadPreferences().Bytes==preferences.Bytes);
        CheckAudio(.1f,.9f,true);
    });
    AfterEach([this]{Driver.Reset();IAutomationDriverModule::Get().Disable();
#if defined(GIRLS_SPIRE_COVERAGE)
        TestEqual(TEXT("通常ゲーム経路の計測を書出し"),__llvm_profile_write_file(),0);
#endif
    });
}
