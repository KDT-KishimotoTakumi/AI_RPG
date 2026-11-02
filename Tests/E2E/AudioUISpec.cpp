#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/RunScreen.h"
#include "Presentation/CombatScreen.h"
#include "Presentation/GameAudio.h"
namespace {
class AudioPreferencesMemory : public GirlsSpire::PreferencesFiles {
public:
    std::optional<std::string> Bytes;bool Fail=false;
    GirlsSpire::SlotBytes ReadPreferences()override{return {false,Bytes};}
    bool ReplacePreferences(std::string_view bytes)override{if(Fail)return false;Bytes=bytes;return true;}
    bool PreservePreferences()override{return true;}
};
class RecordingGameAudio : public IGameAudio {
public:
    GirlsSpire::Preferences Applied;int Applies=0,Starts=0,Confirms=0,Impacts=0;
    bool Available=true;
    bool Ready()const override{return Available;}
    bool Apply(const GirlsSpire::Preferences& value)override{if(!GirlsSpire::ResolveAudioLevels(value))return false;Applied=value;++Applies;return true;}
    void Start()override{++Starts;}
    bool Cue(EGameCue cue)override{if(cue==EGameCue::Confirm)++Confirms;else ++Impacts;return true;}
};
}
BEGIN_DEFINE_SPEC(FGirlsSpireAudioUISpec,"GirlsSpire.UI.Audio",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;TSharedPtr<SWindow> Window;
    std::shared_ptr<RecordingGameAudio> Audio;
    std::shared_ptr<AudioPreferencesMemory> Files;
    std::shared_ptr<GirlsSpire::PreferencesStore> Store;
    bool Click(const char* key){const auto target=Driver->FindElement(By::Id(key));if(!target->IsVisible()){const auto scroll=Driver->FindElement(By::Id("run.scroll"))->Exists()?Driver->FindElement(By::Id("run.scroll")):Driver->FindElement(By::Id("battle.scroll"));scroll->ScrollToBeginning();DrainSlateLayout();if(!target->IsVisible()){scroll->ScrollToEndUntil(By::Id(key));DrainSlateLayout();}}return TestTrue(UTF8_TO_TCHAR(key),ClickAndDrain(target));}
END_DEFINE_SPEC(FGirlsSpireAudioUISpec)
void FGirlsSpireAudioUISpec::Define()
{
    BeforeEach([this]{FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();Window=SNew(SWindow).ClientSize(FVector2D(1200,1000));FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();Audio=std::make_shared<RecordingGameAudio>();Files=std::make_shared<AudioPreferencesMemory>();Store=std::make_shared<GirlsSpire::PreferencesStore>(*Files);});
    LatentBeforeEach(WaitForSlateLayout);
    It(TEXT("保存成功時だけ独立音量を再生層へ反映する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this]{
        Async(EAsyncExecution::TaskGraphMainThread,[this]{Window->SetContent(SNew(SRunScreen).PreferencesService(Store).Audio(Audio).DevelopmentTools(false));}).Wait();DrainSlateLayout();
        TestTrue(TEXT("起動時に既定音量とBGMを開始"),Audio->Applies==1 && Audio->Starts==1 && Audio->Applied.BgmVolume==100 && Audio->Applied.SfxVolume==100);
        if(!Click("title.settings") || !Click("settings.bgm.mute") || !Click("settings.sfx.down") || !Click("settings.cancel"))return;
        TestTrue(TEXT("取消で再生音量を変えない"),Audio->Applies==1 && Audio->Applied.BgmVolume==100);
        if(!Click("title.settings") || !Click("settings.bgm.mute") || !Click("settings.sfx.down"))return;Files->Fail=true;if(!Click("settings.save"))return;
        TestTrue(TEXT("保存失敗中は音量未適用"),Audio->Applies==1 && Audio->Applied.SfxVolume==100);Files->Fail=false;if(!Click("settings.save"))return;
        TestTrue(TEXT("成功後だけBGM0/SFX90を反映・入力効果音を接続"),Audio->Applies==2 && Audio->Applied.BgmVolume==0 && Audio->Applied.SfxVolume==90 && Audio->Confirms>0);
    });
    It(TEXT("確定した打撃の効果音は戦闘と乱数を変更しない"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this]{
        using namespace GirlsSpire;BattleUnit hero;hero.At={1,1};hero.Accuracy=200;hero.Attack=20;BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={2,1};enemy.Hp=enemy.MaxHp=100;enemy.Speed=1;
        auto battle=MakeShared<Battle>(8,8,std::vector<Tile>(64),std::vector<BattleUnit>{hero,enemy},42);auto expected=*battle;
        Async(EAsyncExecution::TaskGraphMainThread,[this,battle]{Window->SetContent(SNew(SCombatScreen).Combat(battle).Audio(Audio));}).Wait();DrainSlateLayout();
        if(!Click("battle.cell.2.1") || !Click("battle.confirm"))return;expected.Attack(expected.Snapshot().Revision,0,{2,1});
        TestTrue(TEXT("入力音/打撃音を通知し状態は非音声経路と一致"),Audio->Confirms==1 && Audio->Impacts==1 && battle->Checkpoint()==expected.Checkpoint());
    });
    for(bool unavailable:{false,true})It(*FString::Printf(TEXT("音声利用不可%dを通知し設定と実戦を継続できる"),unavailable),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this,unavailable]{
        Audio->Available=!unavailable;
        Async(EAsyncExecution::TaskGraphMainThread,[this]{Window->SetContent(SNew(SRunScreen).PreferencesService(Store).Audio(Audio).DevelopmentTools(false));}).Wait();DrainSlateLayout();
        TestEqual(TEXT("利用不可のときだけ通知"),Driver->FindElement(By::Id("audio.unavailable"))->IsVisible(),unavailable);
        if(!Click("title.settings") || !Click("settings.bgm.mute") || !Click("settings.sfx.mute") || !Click("settings.save"))return;
        TestTrue(TEXT("音量0を正常に保存"),Store->Load().Value.BgmVolume==0 && Store->Load().Value.SfxVolume==0);
        TestEqual(TEXT("消音を利用不可と混同しない"),Driver->FindElement(By::Id("audio.unavailable"))->IsVisible(),unavailable);
        if(!Click("title.start") || !Click("role.confirm") || !Click("route.node.0") || !Click("battle.start"))return;
        TestTrue(TEXT("通知中も実戦へ進める"),Driver->FindElement(By::Id("battle.round"))->Exists());
        TestEqual(TEXT("通知は戦闘でも維持"),Driver->FindElement(By::Id("audio.unavailable"))->IsVisible(),unavailable);
    });
    AfterEach([this]{Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}Store.reset();Files.reset();Audio.reset();IAutomationDriverModule::Get().Disable();});
}
