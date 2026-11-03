#include "GameAudio.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Engine/Engine.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
namespace {
USoundBase* Clip(const TCHAR* path)
{
    if(!FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(FString(path))))return nullptr;
    return LoadObject<USoundBase>(nullptr,path,nullptr,LOAD_NoWarn | LOAD_Quiet);
}
UAudioComponent* Component()
{
    auto* value=NewObject<UAudioComponent>(GetTransientPackage());
    value->bAutoDestroy=false;value->bIsUISound=true;value->bAllowSpatialization=false;
    return value;
}
}
FGameAudio::FGameAudio(bool loadAssets)
{
    MusicComponent=Component();EffectComponent=Component();
    if(loadAssets && GEngine && GEngine->UseSound() && GEngine->GetMainAudioDeviceRaw()){
        MusicClip=Clip(TEXT("/Game/Audio/BGM_Castle_V2.BGM_Castle_V2"));
        ConfirmClip=Clip(TEXT("/Game/Audio/SFX_Confirm_V2.SFX_Confirm_V2"));
        ImpactClip=Clip(TEXT("/Game/Audio/SFX_Impact_V2.SFX_Impact_V2"));
    }
    MusicComponent->SetSound(MusicClip);
}
FGameAudio::~FGameAudio(){Stop();}
bool FGameAudio::Ready() const {return MusicClip && ConfirmClip && ImpactClip;}
bool FGameAudio::Apply(const GirlsSpire::Preferences& value)
{
    const auto levels=GirlsSpire::ResolveAudioLevels(value);if(!levels)return false;
    Levels=*levels;MusicComponent->SetVolumeMultiplier(Levels.Bgm);EffectComponent->SetVolumeMultiplier(Levels.Sfx);
    if(Levels.Sfx==0)EffectComponent->Stop();
    RefreshMusic();return true;
}
void FGameAudio::Start(){MusicRequested=true;RefreshMusic();}
void FGameAudio::RefreshMusic()
{
    if(!MusicRequested || !MusicClip || Levels.Bgm==0){MusicComponent->Stop();return;}
    if(!MusicComponent->IsPlaying())MusicComponent->Play();
}
bool FGameAudio::Cue(EGameCue cue)
{
    auto* sound=cue==EGameCue::Confirm?ConfirmClip.Get():cue==EGameCue::Impact?ImpactClip.Get():nullptr;
    if(!sound || Levels.Sfx==0)return false;
    EffectComponent->Stop();EffectComponent->SetSound(sound);EffectComponent->Play();return EffectComponent->IsPlaying();
}
void FGameAudio::Stop(){MusicRequested=false;MusicComponent->Stop();EffectComponent->Stop();}
#if defined(__clang__)
// GS-054: 増分GCに非対応の参照APIへ戻した場合はビルドを失敗させる。
#pragma clang diagnostic push
#pragma clang diagnostic error "-Wdeprecated-declarations"
#endif
void FGameAudio::AddReferencedObjects(FReferenceCollector& collector)
{
    collector.AddReferencedObject(MusicComponent);collector.AddReferencedObject(EffectComponent);
    collector.AddReferencedObject(MusicClip);collector.AddReferencedObject(ConfirmClip);collector.AddReferencedObject(ImpactClip);
}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
