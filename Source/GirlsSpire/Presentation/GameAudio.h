#pragma once
#include "Application/AudioLevels.h"
#include "UObject/GCObject.h"
class UAudioComponent;
class USoundBase;
enum class EGameCue {Confirm,Impact};
class IGameAudio {
public:
    virtual ~IGameAudio()=default;
    virtual bool Apply(const GirlsSpire::Preferences& value)=0;
    virtual void Start()=0;
    virtual bool Cue(EGameCue cue)=0;
};
class FGameAudio final : public IGameAudio,public FGCObject {
public:
    explicit FGameAudio(bool loadAssets=true);
    ~FGameAudio() override;
    bool Apply(const GirlsSpire::Preferences& value) override;
    void Start() override;
    bool Cue(EGameCue cue) override;
    void Stop();
    UAudioComponent* Music() const {return MusicComponent;}
    UAudioComponent* Effects() const {return EffectComponent;}
    bool Ready() const;
    void AddReferencedObjects(FReferenceCollector& collector) override;
    FString GetReferencerName() const override {return TEXT("GirlsSpire.GameAudio");}
private:
    void RefreshMusic();
    UAudioComponent* MusicComponent=nullptr;
    UAudioComponent* EffectComponent=nullptr;
    USoundBase* MusicClip=nullptr;
    USoundBase* ConfirmClip=nullptr;
    USoundBase* ImpactClip=nullptr;
    GirlsSpire::AudioLevels Levels;
    bool MusicRequested=false;
};
