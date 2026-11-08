#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/RunScreen.h"
#include "Presentation/CombatScreen.h"
#include "Presentation/GameAudio.h"
// FR-071～075 / SC-012 を一画面で見渡すための薄い束ね。
// 個別の深い検証は PreferencesSpec(文字・保存)、BoardCuesSpec(記号・単色)、
// AudioUISpec(音量・音声不可)、BondSpec(親密表現の抑制表示)、
// CampaignSpec の併用設定12日通し(-GirlsSpireCampaignAccessibility)が持つ。
namespace {
class AccPreferencesMemory : public GirlsSpire::PreferencesFiles {
public:
    std::optional<std::string> Bytes;
    GirlsSpire::SlotBytes ReadPreferences()override{return {false,Bytes};}
    bool ReplacePreferences(std::string_view bytes)override{Bytes=bytes;return true;}
    bool PreservePreferences()override{return true;}
};
class AccRecordingAudio : public IGameAudio {
public:
    GirlsSpire::Preferences Applied;int Applies=0,Impacts=0;
    bool Ready()const override{return true;}
    bool Apply(const GirlsSpire::Preferences& value)override{if(!GirlsSpire::ResolveAudioLevels(value))return false;Applied=value;++Applies;return true;}
    void Start()override{}
    bool Cue(EGameCue cue)override{if(cue==EGameCue::Impact)++Impacts;return true;}
};
}
BEGIN_DEFINE_SPEC(FGirlsSpireAccessibilitySpec,"GirlsSpire.UI.Accessibility",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;TSharedPtr<SWindow> Window;
    std::shared_ptr<AccRecordingAudio> Audio;
    std::shared_ptr<AccPreferencesMemory> Files;
    std::shared_ptr<GirlsSpire::PreferencesStore> Store;
    void OpenTitle(){Async(EAsyncExecution::TaskGraphMainThread,[this]{Window->SetContent(SNew(SRunScreen).PreferencesService(Store).Audio(Audio).DevelopmentTools(false));}).Wait();DrainSlateLayout();}
    bool Click(const char* key){const auto target=Driver->FindElement(By::Id(key));if(!target->IsVisible()){const auto scroll=Driver->FindElement(By::Id("run.scroll"))->Exists()?Driver->FindElement(By::Id("run.scroll")):Driver->FindElement(By::Id("battle.scroll"));scroll->ScrollToBeginning();DrainSlateLayout();if(!target->IsVisible()){scroll->ScrollToEndUntil(By::Id(key));DrainSlateLayout();}}return TestTrue(UTF8_TO_TCHAR(key),ClickAndDrain(target));}
    FString Text(const char* key){return Driver->FindElement(By::Id(key))->GetText().ToString();}
END_DEFINE_SPEC(FGirlsSpireAccessibilitySpec)
void FGirlsSpireAccessibilitySpec::Define()
{
    BeforeEach([this]{
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        Window=SNew(SWindow).ClientSize(FVector2D(1200,1000));FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();
        Audio=std::make_shared<AccRecordingAudio>();Files=std::make_shared<AccPreferencesMemory>();Store=std::make_shared<GirlsSpire::PreferencesStore>(*Files);
    });
    LatentBeforeEach(WaitForSlateLayout);
    It(TEXT("FR-071/073 文字3段階をマウスだけで保存し、画面の実文字が段階順に大きくなる"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this]{
        using namespace GirlsSpire;
        TestTrue(TEXT("基準の文字は段階順に拡大"),ScaledFontSize(TextSize::Standard,20)<ScaledFontSize(TextSize::Large,20) && ScaledFontSize(TextSize::Large,20)<ScaledFontSize(TextSize::Largest,20));
        double heights[3]={};
        for(int size=0;size<3;++size){
            OpenTitle();
            if(!Click("title.settings") || !Click(TCHAR_TO_UTF8(*FString::Printf(TEXT("settings.text.%d"),size))) || !Click("settings.save"))return;
            if(!TestTrue(TEXT("選んだ段階を保存"),Store->Load().Issue==PreferencesIssue::None && Store->Load().Value.Text==static_cast<TextSize>(size)))return;
            OpenTitle();heights[size]=Driver->FindElement(By::Id("title.heading"))->GetSize().Y;
        }
        TestTrue(TEXT("再生成後のタイトル見出しが標準<大<最大"),heights[0]<heights[1] && heights[1]<heights[2]);
    });
    It(TEXT("FR-073/075 字幕・演出軽減・親密表現抑制・BGM/効果音の個別消音を設定画面で切替え実保存し音声層へ反映する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this]{
        using namespace GirlsSpire;
        OpenTitle();
        if(!Click("title.settings"))return;
        TestTrue(TEXT("既定は字幕入/演出軽減切/抑制入"),Text("settings.subtitles").Contains(TEXT("入")) && Text("settings.effects").Contains(TEXT("切")) && Text("settings.intimacy").Contains(TEXT("入")));
        // 色や音でなく文言でも状態を取得できることを確認しつつ個別に切替える。
        if(!Click("settings.subtitles") || !Click("settings.effects") || !Click("settings.intimacy") || !Click("settings.bgm.mute"))return;
        TestTrue(TEXT("切替後の状態を文言で取得"),Text("settings.subtitles").Contains(TEXT("切")) && Text("settings.effects").Contains(TEXT("入")) && Text("settings.intimacy").Contains(TEXT("切")));
        if(!Click("settings.save"))return;
        TestTrue(TEXT("BGMだけ消音し効果音は維持して保存"),Store->Load().Value==Preferences{TextSize::Standard,0,100,false,true,false} && Audio->Applied.BgmVolume==0 && Audio->Applied.SfxVolume==100);
        if(!Click("title.settings") || !Click("settings.sfx.mute") || !Click("settings.save"))return;
        TestTrue(TEXT("効果音も消音しても他の設定を維持"),Store->Load().Value==Preferences{TextSize::Standard,0,0,false,true,false} && Audio->Applied.SfxVolume==0);
        if(!Click("title.settings") || !Click("settings.defaults") || !Click("settings.save"))return;
        TestTrue(TEXT("標準へ戻すと全値が既定"),Store->Load().Value==Preferences{} && Audio->Applied==Preferences{});
    });
    It(TEXT("SC-012 最大文字・字幕・演出軽減・抑制・消音を併用し、色と音に頼らず盤面と結果を取得できる"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this]{
        using namespace GirlsSpire;
        const Preferences combined{TextSize::Largest,0,0,true,true,true};
        OpenTitle();
        if(!Click("title.settings") || !Click("settings.text.2") || !Click("settings.bgm.mute") || !Click("settings.sfx.mute") || !Click("settings.effects") || !Click("settings.save"))return;
        if(!TestTrue(TEXT("併用設定を実保存し音声層へ消音を反映"),Store->Load().Issue==PreferencesIssue::None && Store->Load().Value==combined && Audio->Applied==combined))return;
        BattleUnit hero;hero.At={1,1};hero.Accuracy=200;hero.Attack=50;hero.Speed=30;hero.Range=3;BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={2,1};enemy.Hp=enemy.MaxHp=10;enemy.Speed=1;
        auto battle=MakeShared<Battle>(8,8,std::vector<Tile>(64),std::vector<BattleUnit>{hero,enemy},42);auto expected=*battle;
        Async(EAsyncExecution::TaskGraphMainThread,[this,battle,combined]{Window->SetContent(SNew(SCombatScreen).Combat(battle).DisplayPreferences(combined).Audio(Audio));}).Wait();DrainSlateLayout();
        TestTrue(TEXT("色なしでも味方○・敵△×を記号で区別"),Text("battle.cues.1.1").Contains(TEXT("○")) && Text("battle.cues.2.1").Contains(TEXT("△×")));
        if(!Click("battle.cues.monochrome") || !Click("battle.cues.threats"))return;
        TestTrue(TEXT("単色と危険表示でも記号と範囲の文字説明を取得"),Text("battle.cues.2.1").Contains(TEXT("△×")) && Driver->FindElement(By::Id("battle.cues.scope"))->Exists());
        TestTrue(TEXT("最大文字でも戦闘の見出しが標準より大きい"),Driver->FindElement(By::Id("battle.round"))->GetSize().Y>=ScaledFontSize(combined.Text,23));
        if(!Click("battle.cell.2.1") || !Click("battle.confirm"))return;expected.Attack(expected.Snapshot().Revision,0,{2,1});
        TestTrue(TEXT("音量0でも結果は文字で取得でき、演出軽減は戦闘状態と乱数を変えない"),Driver->FindElement(By::Id("battle.won"))->Exists() && battle->Checkpoint()==expected.Checkpoint());
        TestTrue(TEXT("打撃は音層へ通知されるが、結果の伝達は音に依存しない"),Audio->Impacts==1);
    });
    AfterEach([this]{Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}Store.reset();Files.reset();Audio.reset();IAutomationDriverModule::Get().Disable();});
}
