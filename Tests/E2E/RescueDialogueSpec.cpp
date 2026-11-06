#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/RunScreen.h"
#include "Application/Encounter.h"
#include "Domain/RescueDialogue.h"
#include "Domain/CharacterCatalog.h"
#include "Presentation/GameAudio.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
namespace {
class RescueTextPreferenceFiles : public GirlsSpire::PreferencesFiles {
public:
    std::optional<std::string> Bytes;
    GirlsSpire::SlotBytes ReadPreferences()override{return {false,Bytes};}
    bool ReplacePreferences(std::string_view bytes)override{Bytes=bytes;return true;}
    bool PreservePreferences()override{return true;}
};
}
BEGIN_DEFINE_SPEC(FGirlsSpireRescueDialogueSpec,"GirlsSpire.UI.RescueDialogue",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;TSharedPtr<SWindow> Window;
    std::shared_ptr<RescueTextPreferenceFiles> Files;
    std::shared_ptr<GirlsSpire::PreferencesStore> Store;
    bool Click(const char* id){auto target=Driver->FindElement(By::Id(id));if(!target->IsVisible()){Driver->FindElement(By::Id("run.scroll"))->ScrollToEndUntil(By::Id(id));DrainSlateLayout();}return TestTrue(UTF8_TO_TCHAR(id),ClickAndDrain(target));}
    std::shared_ptr<GirlsSpire::Run> Prepare(int actor){
        using namespace GirlsSpire;Profile profile;profile.Bonds.fill(50);
        for(int seed=0;seed<1000;++seed){
            auto run=std::make_shared<Run>(seed,Difficulty::Beginner,HeroRole::Sword,profile);
            for(int node=0;node<static_cast<int>(run->Snapshot().Choices.size());++node){
                const auto candidate=run->Snapshot().Choices[node];
                if(candidate.Kind!=NodeKind::Rescue || candidate.Candidate!=actor-1)continue;
                run->ChooseNode(0,node);auto battle=Encounter::Create(run->Snapshot(),run->RandomState(),profile).Snapshot();battle.Outcome=BattleOutcome::Won;
                if(run->CompleteBattle(run->Snapshot().Revision,battle))return run;
            }
        }
        return nullptr;
    }
END_DEFINE_SPEC(FGirlsSpireRescueDialogueSpec)
void FGirlsSpireRescueDialogueSpec::Define()
{
    BeforeEach([this]{FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();Window=SNew(SWindow).ClientSize(FVector2D(1200,1000));FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();});
    LatentBeforeEach(WaitForSlateLayout);
    for(int actor=1;actor<=8;++actor)for(int choice=0;choice<3;++choice)
    It(*FString::Printf(TEXT("人物%dの正式返答%dを予告・取消・確定する"),actor,choice),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this,actor,choice]{
        using namespace GirlsSpire;auto run=Prepare(actor);if(!TestTrue(TEXT("救出勝利後の状態を用意"),bool(run)))return;
        const auto before=run->Checkpoint();const auto profile=run->ProfileState();auto expected=*run;
        const auto* dialogue=FindRescueDialogue(actor);if(!TestNotNull(TEXT("承認済みカタログ"),dialogue))return;
        Async(EAsyncExecution::TaskGraphMainThread,[this,run]{Window->SetContent(SNew(SRunScreen).InitialRun(run));}).Wait();DrainSlateLayout();
        const FString opening=FString(UTF8_TO_TCHAR(FindCharacter(actor)->Name.data()))+TEXT("「")+UTF8_TO_TCHAR(dialogue->Opening.data())+TEXT("」");
        TestEqual(TEXT("人物名と本人の台詞"),Driver->FindElement(By::Id("rescue.dialogue"))->GetText().ToString(),opening);
        TestTrue(TEXT("救出人物の衣装画像を表示"),Driver->FindElement(By::Id("rescue.image"))->Exists());
        const TCHAR* effects[]={TEXT("（＋10）"),TEXT("（0）"),TEXT("（−5）")};
        for(int index=0;index<3;++index){const auto id=FString::Printf(TEXT("rescue.choice.%d"),index);TestEqual(TEXT("承認済みの本文と効果数値"),Driver->FindElement(By::Id(*id))->GetText().ToString(),FString(UTF8_TO_TCHAR(dialogue->Replies[index].data()))+effects[index]);}
        const auto id=FString::Printf(TEXT("rescue.choice.%d"),choice);
        if(!Click(TCHAR_TO_UTF8(*id)))return;
        TestTrue(TEXT("選んだ本文を予告にも表示"),Driver->FindElement(By::Id("rescue.preview"))->GetText().ToString().Contains(UTF8_TO_TCHAR(dialogue->Replies[choice].data())));
        TestTrue(TEXT("予告では状態・乱数・絆を変更しない"),run->Checkpoint()==before && run->ProfileState()==profile);
        if(!Click("rescue.cancel"))return;
        TestTrue(TEXT("取消でも不変"),run->Checkpoint()==before && run->ProfileState()==profile);
        if(!Click(TCHAR_TO_UTF8(*id)) || !Click("rescue.confirm"))return;
        expected.ConverseRescue(before.State.Revision,static_cast<ConversationChoice>(choice));
        TestTrue(TEXT("既存の確定規則と全状態・絆・乱数が一致"),run->Checkpoint()==expected.Checkpoint() && run->ProfileState()==expected.ProfileState());
        TestFalse(TEXT("同じ救出の返答を繰り返せない"),Driver->FindElement(By::Id("rescue.help"))->Exists());
    });
    for(int width:{1920,2560})for(int size=0;size<3;++size)
    It(*FString::Printf(TEXT("実寸%dと文字%dで救出本文と通知を併用する"),width,size),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this,width,size]{
        using namespace GirlsSpire;const int height=width==1920?1080:1440;auto run=Prepare(6);if(!TestTrue(TEXT("救出後の状態"),bool(run)))return;
        Files=std::make_shared<RescueTextPreferenceFiles>();Store=std::make_shared<PreferencesStore>(*Files);
        Preferences prefs;prefs.Text=static_cast<TextSize>(size);prefs.ReducedEffects=true;prefs.BgmVolume=0;prefs.SfxVolume=0;
        if(!TestTrue(TEXT("併用設定を保存して再読込"),Store->Save(prefs)==PreferencesIssue::None && Store->Load().Value==prefs))return;
        const auto before=run->Checkpoint();const auto profile=run->ProfileState();
        Async(EAsyncExecution::TaskGraphMainThread,[this,width,height,run]{
            Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());
            Window=SNew(SWindow).ClientSize(FVector2D(width,height)).AutoCenter(EAutoCenter::None).ScreenPosition(FVector2D::ZeroVector).SaneWindowPlacement(false).AdjustInitialSizeAndPositionForDPIScale(false);
            Window->SetContent(SNew(SRunScreen).InitialRun(run).PreferencesService(Store).Audio(std::make_shared<FGameAudio>(false)).DevelopmentTools(false));
            FSlateApplication::Get().AddWindow(Window.ToSharedRef());
        }).Wait();DrainSlateLayout();
        Async(EAsyncExecution::TaskGraphMainThread,[this,width,height]{const auto actual=Window->GetClientSizeInScreen();TestTrue(TEXT("実クライアント寸法"),FMath::IsNearlyEqual(double(actual.X),double(width),.5) && FMath::IsNearlyEqual(double(actual.Y),double(height),.5));}).Wait();
        TestTrue(TEXT("実際に素材を持たない音声層の通知"),Driver->FindElement(By::Id("audio.unavailable"))->IsVisible());
        for(const char* id:{"rescue.choice.0","rescue.choice.1","rescue.choice.2"})TestTrue(TEXT("3返答すべて表示"),Driver->FindElement(By::Id(id))->IsVisible());
        if(!Click("rescue.choice.2") || !Click("rescue.cancel"))return;
        TestTrue(TEXT("予告と取消は全状態/乱数/絆を変更しない"),run->Checkpoint()==before && run->ProfileState()==profile);
        Async(EAsyncExecution::TaskGraphMainThread,[this,width,height,size]{
            TArray<FColor> pixels;FIntVector dimensions;if(!TestTrue(TEXT("実画面を撮影"),FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),pixels,dimensions)))return;
            TestTrue(TEXT("撮影寸法も実クライアント以上"),dimensions.X>=width && dimensions.Y>=height);
            TArray64<uint8> png;FImageUtils::PNGCompressImageArray(dimensions.X,dimensions.Y,pixels,png);const auto folder=FPaths::ProjectSavedDir()/TEXT("Screenshots");IFileManager::Get().MakeDirectory(*folder,true);
            TestTrue(TEXT("画像を保存"),FFileHelper::SaveArrayToFile(png,*(folder/FString::Printf(TEXT("RescueDialogue-%d-%d.png"),width,size))));
        }).Wait();
    });
    AfterEach([this]{Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}Store.reset();Files.reset();IAutomationDriverModule::Get().Disable();});
}
