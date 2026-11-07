#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/RunScreen.h"
#include "Domain/RelationshipText.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include <algorithm>

namespace {
class NarrativePreferenceFiles : public GirlsSpire::PreferencesFiles {
public:
    std::optional<std::string> Bytes;
    GirlsSpire::SlotBytes ReadPreferences()override{return {false,Bytes};}
    bool ReplacePreferences(std::string_view bytes)override{Bytes=bytes;return true;}
    bool PreservePreferences()override{return true;}
};
std::shared_ptr<GirlsSpire::Run> PrepareNarrative(int actor,bool ending)
{
    using namespace GirlsSpire;
    for(int seed=0;seed<1000;++seed){
        Profile profile;profile.Bonds.fill(80);profile.SpecialConversations.fill(ending);
        auto run=std::make_shared<Run>(seed,Difficulty::Beginner,HeroRole::Sword,profile);
        for(int step=0;step<24 && run->Snapshot().Phase==RunPhase::Route;++step){
            int node=0;
            if(!ending)for(int i=0;i<static_cast<int>(run->Snapshot().Choices.size());++i){
                const auto& c=run->Snapshot().Choices[i];
                if(c.Kind==NodeKind::Social || (c.Kind==NodeKind::Rescue && c.Candidate==actor-1))node=i;
            }
            run->ChooseNode(run->Snapshot().Revision,node);
            if(!ending && run->Snapshot().Active.Kind==NodeKind::Social && run->MaxHp(actor)>0)return run;
            run->CompleteEvent(run->Snapshot().Revision);
        }
        if(ending){const auto choices=run->EpilogueChoices();if(std::find(choices.begin(),choices.end(),actor)!=choices.end())return run;}
    }
    return nullptr;
}
}

BEGIN_DEFINE_SPEC(FGirlsSpireNarrativeResolutionSpec,"GirlsSpire.UI.NarrativeResolution",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
    std::shared_ptr<NarrativePreferenceFiles> Files;
    std::shared_ptr<GirlsSpire::PreferencesStore> Store;
    bool Open(const std::shared_ptr<GirlsSpire::Run>& run,int width,int size);
    bool Click(const char* id);
    void Capture(const char* page,int width,int size);
END_DEFINE_SPEC(FGirlsSpireNarrativeResolutionSpec)

bool FGirlsSpireNarrativeResolutionSpec::Open(const std::shared_ptr<GirlsSpire::Run>& run,int width,int size)
{
    using namespace GirlsSpire;
    if(!TestTrue(TEXT("対象人物の状態を準備"),bool(run)))return false;
    if(!Store){
        Files=std::make_shared<NarrativePreferenceFiles>();Store=std::make_shared<PreferencesStore>(*Files);
        Preferences prefs;prefs.Text=static_cast<TextSize>(size);prefs.ReducedEffects=true;prefs.SuppressIntimacy=false;prefs.BgmVolume=0;prefs.SfxVolume=0;
        if(!TestTrue(TEXT("文字と併用設定を保存・再読込"),Store->Save(prefs)==PreferencesIssue::None && Store->Load().Value==prefs))return false;
    }
    bool dimensions=false;
    Async(EAsyncExecution::TaskGraphMainThread,[this,run,width,&dimensions]{
        if(!Window){
            const int height=width==1920?1080:1440;
            Window=SNew(SWindow).ClientSize(FVector2D(width,height)).AutoCenter(EAutoCenter::None).ScreenPosition(FVector2D::ZeroVector).SaneWindowPlacement(false).AdjustInitialSizeAndPositionForDPIScale(false);
            FSlateApplication::Get().AddWindow(Window.ToSharedRef());
        }
        Window->SetContent(SNew(SRunScreen).InitialRun(run).PreferencesService(Store).DevelopmentTools(false));
        const auto actual=Window->GetClientSizeInScreen();
        dimensions=FMath::IsNearlyEqual(double(actual.X),double(width),.5) && FMath::IsNearlyEqual(double(actual.Y),double(width==1920?1080:1440),.5);
    }).Wait();DrainSlateLayout();
    return TestTrue(TEXT("実クライアント寸法が指定値に一致"),dimensions);
}
bool FGirlsSpireNarrativeResolutionSpec::Click(const char* id)
{
    const auto target=Driver->FindElement(By::Id(id));
    if(!target->IsVisible()){
        const auto scroll=Driver->FindElement(By::Id("run.scroll"));
        scroll->ScrollToBeginning();DrainSlateLayout();
        if(!target->IsVisible()){scroll->ScrollToEndUntil(By::Id(id));DrainSlateLayout();}
    }
    return TestTrue(*FString::Printf(TEXT("本文画面を実マウス操作 %hs"),id),ClickAndDrain(target));
}
void FGirlsSpireNarrativeResolutionSpec::Capture(const char* page,int width,int size)
{
    Async(EAsyncExecution::TaskGraphMainThread,[this,page,width,size]{
        TArray<FColor> pixels;FIntVector dimensions;
        if(!TestTrue(TEXT("本文画面の実描画を撮影"),FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),pixels,dimensions)))return;
        TestTrue(TEXT("撮影寸法が実クライアント以上"),dimensions.X>=width && dimensions.Y>=(width==1920?1080:1440));
        TArray64<uint8> png;FImageUtils::PNGCompressImageArray(dimensions.X,dimensions.Y,pixels,png);
        const auto folder=FPaths::ProjectSavedDir()/TEXT("Screenshots");IFileManager::Get().MakeDirectory(*folder,true);
        TestTrue(TEXT("本文の実寸画像を保存"),FFileHelper::SaveArrayToFile(png,*(folder/FString::Printf(TEXT("Narrative-%hs-%d-%d.png"),page,width,size))));
    }).Wait();
}
void FGirlsSpireNarrativeResolutionSpec::Define()
{
    BeforeEach([this]{FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();Driver=IAutomationDriverModule::Get().CreateDriver();});
    LatentBeforeEach(WaitForSlateLayout);
    for(int width:{1920,2560})for(int size=0;size<3;++size){
        It(*FString::Printf(TEXT("交流8人の実寸%d・文字%dで本文と下端操作"),width,size),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(600),[this,width,size]{
            using namespace GirlsSpire;
            for(int actor=1;actor<=8;++actor){
                auto run=PrepareNarrative(actor,false);if(!Open(run,width,size))return;
                const auto before=run->Checkpoint();const auto profile=run->ProfileState();const auto preferences=Files->Bytes;
                const auto id=FString::Printf(TEXT("social.actor.%d"),actor);if(!Click(TCHAR_TO_UTF8(*id)))return;
                const auto* text=FindSocialDialogue(actor);
                TestTrue(TEXT("全人物の導入本文"),Driver->FindElement(By::Id("social.dialogue"))->GetText().ToString().Contains(UTF8_TO_TCHAR(text->Opening.data())));
                for(int choice=0;choice<3;++choice){
                    const auto key=FString::Printf(TEXT("social.choice.%d"),choice);if(!Click(TCHAR_TO_UTF8(*key)))return;
                    TestTrue(TEXT("返答が予告に全文ある"),Driver->FindElement(By::Id("social.preview"))->GetText().ToString().Contains(UTF8_TO_TCHAR(text->Replies[choice].data())));
                    if(!Click("social.cancel"))return;
                }
                if(actor==8)Capture("social",width,size);
                if(!Click("social.special"))return;
                TestTrue(TEXT("親密表現の事前警告"),Driver->FindElement(By::Id("social.special.warning"))->Exists());
                TestFalse(TEXT("警告では本文を表示しない"),Driver->FindElement(By::Id("social.special.preview"))->Exists());
                if(actor==8)Capture("warning",width,size);
                if(!Click("social.special.read"))return;
                TestTrue(TEXT("専用交流の全文"),Driver->FindElement(By::Id("social.special.preview"))->GetText().ToString().Contains(UTF8_TO_TCHAR(text->Special.data())));
                if(actor==8)Capture("special",width,size);
                if(!Click("social.special.cancel"))return;
                TestTrue(TEXT("全予告と取消で状態・乱数・恒久記録不変"),run->Checkpoint()==before && run->ProfileState()==profile);
                if(!Click("social.special") || !Click("social.special.skip"))return;
                TestTrue(TEXT("警告から要約を表示"),Driver->FindElement(By::Id("social.special.summary"))->Exists());
                TestFalse(TEXT("要約に全文を出さない"),Driver->FindElement(By::Id("social.special.preview"))->GetText().ToString().Contains(UTF8_TO_TCHAR(text->Special.data())));
                TestTrue(TEXT("要約へ切替後も確定前の状態は同じ"),run->Checkpoint()==before && run->ProfileState()==profile);
                if(!Click("social.special.confirm"))return;
                auto expected=*Run::Restore(before,profile);expected.SpecialConverse(before.State.Revision,actor);
                TestTrue(TEXT("専用交流確定は既存規則と一致"),run->Checkpoint()==expected.Checkpoint() && run->ProfileState()==expected.ProfileState());
                TestTrue(TEXT("本文操作で独立設定不変"),Files->Bytes==preferences);
            }
        });
        It(*FString::Printf(TEXT("後日談9編の実寸%d・文字%dで本文と再読"),width,size),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(600),[this,width,size]{
            using namespace GirlsSpire;
            for(int actor=0;actor<=8;++actor){
                auto run=PrepareNarrative(actor,true);if(!Open(run,width,size))return;
                const auto before=run->Checkpoint();const auto profile=run->ProfileState();const auto preferences=Files->Bytes;
                const auto key=FString::Printf(TEXT("epilogue.select.%d"),actor);
                if(!Click("result.continue") || !Click(TCHAR_TO_UTF8(*key)))return;
                const auto* text=FindEndingText(actor);
                TestEqual(TEXT("後日談の題名"),Driver->FindElement(By::Id("epilogue.title"))->GetText().ToString(),FString(UTF8_TO_TCHAR(text->Title.data())));
                TestEqual(TEXT("後日談の全段落"),Driver->FindElement(By::Id("epilogue.page"))->GetText().ToString(),FString(UTF8_TO_TCHAR(text->Page.data())));
                if(actor==8)Capture("ending",width,size);
                if(!Click("epilogue.cancel"))return;
                TestTrue(TEXT("後日談予告・取消で全状態不変"),run->Checkpoint()==before && run->ProfileState()==profile);
                if(!Click(TCHAR_TO_UTF8(*key)) || !Click("epilogue.confirm") || !Click("epilogue.library"))return;
                const auto record=FString::Printf(TEXT("epilogue.record.%d"),actor);if(!Click(TCHAR_TO_UTF8(*record)))return;
                TestEqual(TEXT("実寸画面から同じ本文を再読"),Driver->FindElement(By::Id("epilogue.page"))->GetText().ToString(),FString(UTF8_TO_TCHAR(text->Page.data())));
                TestFalse(TEXT("再読で二重確定なし"),Driver->FindElement(By::Id("epilogue.confirm"))->Exists());
                TestTrue(TEXT("後日談操作で独立設定不変"),Files->Bytes==preferences);
            }
        });
    }
    AfterEach([this]{Driver.Reset();if(Window){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}Store.reset();Files.reset();IAutomationDriverModule::Get().Disable();});
}
