#pragma once
#include "Application/AudioLevels.h"
#include "UObject/GCObject.h"
#include "UObject/ObjectPtr.h"
class UAudioComponent;
class USoundBase;
enum class EGameCue {Confirm,Impact};
class IGameAudio {
public:
    virtual ~IGameAudio()=default;
    virtual bool Apply(const GirlsSpire::Preferences& value)=0;
    virtual void Start()=0;
    virtual bool Cue(EGameCue cue)=0;
    virtual bool Ready() const=0;
};
class FGameAudio final : public IGameAudio,public FGCObject {
public:
    explicit FGameAudio(bool loadAssets=true);
    ~FGameAudio() override;
    bool Apply(const GirlsSpire::Preferences& value) override;
    void Start() override;
    bool Cue(EGameCue cue) override;
    void Stop();
    UAudioComponent* Music() const {return MusicComponent.Get();}
    UAudioComponent* Effects() const {return EffectComponent.Get();}
    bool Ready() const override;
    void AddReferencedObjects(FReferenceCollector& collector) override;
    FString GetReferencerName() const override {return TEXT("GirlsSpire.GameAudio");}
private:
    void RefreshMusic();
    TObjectPtr<UAudioComponent> MusicComponent;
    TObjectPtr<UAudioComponent> EffectComponent;
    TObjectPtr<USoundBase> MusicClip;
    TObjectPtr<USoundBase> ConfirmClip;
    TObjectPtr<USoundBase> ImpactClip;
    GirlsSpire::AudioLevels Levels;
    bool MusicRequested=false;
};
