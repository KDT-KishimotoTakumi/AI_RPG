#include "GameEntry.h"
#include "GameAudio.h"
#include "RunScreen.h"
#include "Infrastructure/DiskSlotFiles.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

AGirlsSpireGameMode::AGirlsSpireGameMode()
{
    PlayerControllerClass=AGirlsSpireController::StaticClass();
    DefaultPawnClass=nullptr;HUDClass=nullptr;
}
void AGirlsSpireController::BeginPlay()
{
    Super::BeginPlay();
    if(!IsLocalController()) return;
    const auto player=GetLocalPlayer();
    auto* viewport=player?player->ViewportClient.Get():nullptr;
    if(!viewport) return;
    FString root=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/(GetWorld()->WorldType==EWorldType::PIE?TEXT("DevelopmentSaves"):TEXT("SaveGames")));
#if !UE_BUILD_SHIPPING
    if(FParse::Param(FCommandLine::Get(),TEXT("GirlsSpireRuntimeTest"))) {
        FString testRoot;
        const auto prefix=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("Saved/Tests/Runtime"));
        if(!FParse::Value(FCommandLine::Get(),TEXT("GirlsSpireRuntimeRoot="),testRoot)) return;
        testRoot=FPaths::ConvertRelativePathToFull(testRoot);
        if(testRoot==prefix || !FPaths::IsUnderDirectory(testRoot,prefix)) return;
        root=testRoot;
    }
#endif
    Files=std::make_shared<GirlsSpire::DiskSlotFiles>(std::filesystem::path(*root));
    Service=std::make_shared<GirlsSpire::SaveTransaction>(*Files,*Files);
    Screen=SNew(SRunScreen).SaveService(Service).PreferencesService(std::make_shared<GirlsSpire::PreferencesStore>(*Files)).Audio(std::make_shared<FGameAudio>()).DevelopmentTools(false);
    viewport->AddViewportWidgetContent(Screen.ToSharedRef(),100);
    viewport->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
    viewport->SetMouseLockMode(EMouseLockMode::DoNotLock);
    OwnerViewport=viewport;bShowMouseCursor=true;
    SetInputMode(FInputModeUIOnly().SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));
}
void AGirlsSpireController::EndPlay(const EEndPlayReason::Type reason)
{
    if(OwnerViewport.IsValid() && Screen.IsValid()) OwnerViewport->RemoveViewportWidgetContent(Screen.ToSharedRef());
    Screen.Reset();Service.reset();Files.reset();OwnerViewport.Reset();
    Super::EndPlay(reason);
}
