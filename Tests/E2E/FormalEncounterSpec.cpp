#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/RunScreen.h"
#include "Application/Encounter.h"
namespace {
class FormalEncounterMemory : public GirlsSpire::SlotFiles,public GirlsSpire::JournalFiles {
public:
    std::array<std::array<std::optional<std::string>,2>,2> Slots{};std::optional<std::string> Journal;
    GirlsSpire::SlotBytes Read(GirlsSpire::SaveKind kind,bool backup)override{return {false,Slots[static_cast<int>(kind)-1][backup]};}
    GirlsSpire::SlotBytes ReadJournal()override{return {false,Journal};}
    bool Replace(GirlsSpire::SaveKind kind,bool backup,std::string_view bytes)override{Slots[static_cast<int>(kind)-1][backup]=std::string(bytes);return true;}
    bool ReplaceJournal(std::string_view bytes)override{Journal=std::string(bytes);return true;}
};
}
BEGIN_DEFINE_SPEC(FGirlsSpireFormalEncounterSpec,"GirlsSpire.UI.FormalEncounter",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
    std::shared_ptr<FormalEncounterMemory> Files;
    std::shared_ptr<GirlsSpire::SaveTransaction> Service;
END_DEFINE_SPEC(FGirlsSpireFormalEncounterSpec)
void FGirlsSpireFormalEncounterSpec::Define()
{
    BeforeEach([this]{FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();Window=SNew(SWindow).ClientSize(FVector2D(1200,1000));FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();});
    LatentBeforeEach(WaitForSlateLayout);
    for(int role=0;role<4;++role)for(bool fallback:{false,true})It(*FString::Printf(TEXT("武器%dの新規戦闘/固定戦場%dで正式技能を使い保存再開する"),role,fallback),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this,role,fallback]{
        using namespace GirlsSpire;Profile profile;profile.Bonds={0,24,49,50,74,75,99,100};
        Run initial(42,Difficulty::Beginner,static_cast<HeroRole>(role),profile);initial.ChooseNode(0,0);auto progress=initial.Checkpoint();progress.State.Health[0].Hp=30;
        const auto restored=Run::Restore(progress,profile);if(!TestTrue(TEXT("負傷状態の通常周回を用意"),restored.has_value()))return;auto run=std::make_shared<Run>(*restored);
        GenerationResult generation;auto expected=Encounter::Create(run->Snapshot(),run->RandomState(),profile,&generation,fallback);
        Files=std::make_shared<FormalEncounterMemory>();Service=std::make_shared<SaveTransaction>(*Files,*Files);
        Async(EAsyncExecution::TaskGraphMainThread,[this,run,fallback]{Window->SetContent(SNew(SRunScreen).InitialRun(run).SaveService(Service).DevelopmentTools(fallback));}).Wait();DrainSlateLayout();
        const auto click=[&](const char* key){const auto target=Driver->FindElement(By::Id(key));if(!target->IsVisible()){const auto scroll=Driver->FindElement(By::Id("battle.scroll"))->Exists()?Driver->FindElement(By::Id("battle.scroll")):Driver->FindElement(By::Id("run.scroll"));scroll->ScrollToBeginning();DrainSlateLayout();if(!target->IsVisible()){scroll->ScrollToEndUntil(By::Id(key));DrainSlateLayout();}}if(std::string_view(key)=="battle.skill.confirm"){Driver->FindElement(By::Id("battle.cell.1.1"))->ScrollBy(-20);DrainSlateLayout();Driver->FindElement(By::Id("battle.scroll"))->ScrollToEnd();DrainSlateLayout();}return TestTrue(UTF8_TO_TCHAR(key),ClickAndDrain(target));};
        if(!click(fallback?"preview.fallback":"battle.start"))return;
        auto saved=Service->Load();
        if(!TestTrue(TEXT("通常開始から正式識別/固定絆/信頼/地形/RNGを同時保存"),saved.Issue==SlotIssue::None && saved.Run && saved.Run->Combat && saved.Run->Combat->Battle==expected.Checkpoint() && saved.Run->Combat->Battle.State.AbilityRules==AbilityRuleSet::CompleteCharacters && saved.Run->Combat->Battle.State.StartingBonds==profile.Bonds && saved.Run->Combat->Control.ProfileDriven))return;
        for(int id=1;id<=8;++id)TestEqual(TEXT("開始時信頼と固定絆"),saved.Run->Combat->Control.Trust[id],profile.Bonds[id-1]);
        const auto hero=expected.Snapshot().Units.front();const auto cell=FString::Printf(TEXT("battle.cell.%d.%d"),hero.At.X,hero.At.Y);
        if(!click("battle.skill.1") || !click(TCHAR_TO_UTF8(*cell)))return;
        TestTrue(TEXT("予告で状態を変えない"),Service->Load().Run==saved.Run);
        if(!TestTrue(TEXT("生成した正式技能を実行できる"),expected.UseSkill(expected.Snapshot().Revision,0,Skill::FirstAid,hero.At)) || !click("battle.skill.confirm"))return;
        saved=Service->Load();
        TestTrue(TEXT("効果/CD/承属性/RNGを通常生成の正式解釈で保存"),saved.Run && saved.Run->Combat && saved.Run->Combat->Battle==expected.Checkpoint() && !expected.Snapshot().Combos.Pending.empty() && expected.Snapshot().Combos.Pending.back().Attributes==Continuation);
        if(!click("save.suspend"))return;
        Async(EAsyncExecution::TaskGraphMainThread,[this]{Window->SetContent(SNew(SRunScreen).SaveService(Service).DevelopmentTools(false));}).Wait();DrainSlateLayout();if(!click("title.resume"))return;
        const auto resumed=Service->Load();TestTrue(TEXT("再開は新規生成せず固定絆/効果/CD/構文/乱数を維持"),resumed.Run && saved.Run && resumed.Run->Combat==saved.Run->Combat && resumed.Permanent==saved.Permanent);
    });
    AfterEach([this]{Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}Service.reset();Files.reset();IAutomationDriverModule::Get().Disable();});
}
