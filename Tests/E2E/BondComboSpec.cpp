#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/RunScreen.h"
#include "Domain/ComboCatalog.h"
namespace {
class BondComboMemory : public GirlsSpire::SlotFiles,public GirlsSpire::JournalFiles,public GirlsSpire::PreferencesFiles {
public:
    std::optional<std::string> PreferencesBytes;
    GirlsSpire::SlotBytes ReadPreferences()override{return {false,PreferencesBytes};}
    bool ReplacePreferences(std::string_view bytes)override{PreferencesBytes=std::string(bytes);return true;}
    bool PreservePreferences()override{return true;}
    std::array<std::array<std::optional<std::string>,2>,2> Slots{};std::optional<std::string> Journal;
    GirlsSpire::SlotBytes Read(GirlsSpire::SaveKind kind,bool backup)override{return {false,Slots[static_cast<int>(kind)-1][backup]};}
    GirlsSpire::SlotBytes ReadJournal()override{return {false,Journal};}
    bool Replace(GirlsSpire::SaveKind kind,bool backup,std::string_view bytes)override{Slots[static_cast<int>(kind)-1][backup]=std::string(bytes);return true;}
    bool ReplaceJournal(std::string_view bytes)override{Journal=std::string(bytes);return true;}
};
}
BEGIN_DEFINE_SPEC(FGirlsSpireBondComboSpec,"GirlsSpire.UI.BondCombos",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
    std::shared_ptr<BondComboMemory> Files;
    std::shared_ptr<GirlsSpire::SaveTransaction> Service;
END_DEFINE_SPEC(FGirlsSpireBondComboSpec)
void FGirlsSpireBondComboSpec::Define()
{
    BeforeEach([this]{FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();Window=SNew(SWindow).ClientSize(FVector2D(1200,1000));FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();});
    LatentBeforeEach(WaitForSlateLayout);
    for(int id=1;id<=8;++id)It(*FString::Printf(TEXT("専用連携%dを実操作で発見し中断再開する"),200+id),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this,id]{
        using namespace GirlsSpire;Profile profile;profile.Bonds[id-1]=100;
        Run run(42,Difficulty::Beginner,HeroRole::Sword,profile);auto progress=run.Checkpoint();progress.State.Day=5;progress.State.Phase=RunPhase::Event;progress.State.Active={NodeKind::Battle};progress.State.Choices.clear();progress.State.Recruited={id-1};
        BattleUnit hero;hero.At={0,1};hero.Hp=hero.MaxHp=80;hero.Attack=10;hero.Accuracy=200;hero.Speed=30;hero.Range=3;
        auto ally=hero;ally.Id=id;ally.At={2,1};ally.Hp=ally.MaxHp=40;ally.Speed=20;
        auto enemy=hero;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={1,2};enemy.Hp=enemy.MaxHp=1000;enemy.Speed=10;enemy.Attack=0;enemy.Movement=0;enemy.Evasion=0;
        Battle initial(8,8,std::vector<Tile>(64),{hero,ally,enemy},42);auto checkpoint=initial.Checkpoint();checkpoint.State.AbilityRules=AbilityRuleSet::CompleteCharacters;checkpoint.State.StartingBonds=profile.Bonds;
        CombatControl control;control.ProfileDriven=true;control.Trust[id]=100;
        Files=std::make_shared<BondComboMemory>();Service=std::make_shared<SaveTransaction>(*Files,*Files);
        if(!TestTrue(TEXT("初期状態を取引保存"),Service->Prepare(RunSaveState{progress,SavedCombat{checkpoint,control.Checkpoint()}},profile)==SlotIssue::None && Service->Retry()==SlotIssue::None))return;
        Async(EAsyncExecution::TaskGraphMainThread,[this]{Window->SetContent(SNew(SRunScreen).SaveService(Service));}).Wait();DrainSlateLayout();
        const auto click=[&](const char* key){const auto target=Driver->FindElement(By::Id(key));if(!target->IsVisible()){const auto scroll=Driver->FindElement(By::Id("battle.scroll"))->Exists()?Driver->FindElement(By::Id("battle.scroll")):Driver->FindElement(By::Id("run.scroll"));scroll->ScrollToBeginning();DrainSlateLayout();if(!target->IsVisible()){scroll->ScrollToEndUntil(By::Id(key));DrainSlateLayout();}}return TestTrue(UTF8_TO_TCHAR(key),ClickAndDrain(target));};
        if(!click("title.resume") || !click("battle.cell.1.1") || !click("battle.move.confirm") || !click("battle.cell.1.2") || !click("battle.confirm") || !click("battle.wait.south") || !click("battle.cell.1.2") || !click("intent.execute"))return;
        const auto result=Service->Load();const auto recipe=FindComboRecipe(200+id);
        TestTrue(TEXT("専用だけゲージ20・発見を同時保存"),result.Issue==SlotIssue::None && result.Permanent.DiscoveredCombos==std::vector<int>{200+id} && result.Run && result.Run->Combat && result.Run->Combat->Battle.State.LastCombo && result.Run->Combat->Battle.State.LastCombo->RecipeId==200+id && result.Run->Combat->Battle.State.LastCombo->GaugeGain==20 && result.Run->Combat->Battle.State.Combos.Gauge==20);
        const auto text=Driver->FindElement(By::Id("battle.combo.result"))->GetText().ToString();TestTrue(TEXT("初発見/名称/承→結の理由を表示"),recipe && text.Contains(UTF8_TO_TCHAR(recipe->Name.data())) && text.Contains(TEXT("初発見")) && text.Contains(TEXT("承→")));
        if(!click("save.suspend"))return;TestTrue(TEXT("タイトルの恒久一覧に表示"),Driver->FindElement(By::Id("title.combos"))->GetText().ToString().Contains(UTF8_TO_TCHAR(recipe->Name.data())));
        Async(EAsyncExecution::TaskGraphMainThread,[this]{Window->SetContent(SNew(SRunScreen).SaveService(Service));}).Wait();DrainSlateLayout();if(!click("title.resume"))return;
        const auto resumed=Service->Load();TestTrue(TEXT("既知表示・乱数/状態を保持し再発見しない"),Driver->FindElement(By::Id("battle.combo.result"))->GetText().ToString().Contains(TEXT("発見済み")) && resumed.Permanent==result.Permanent && resumed.Run && result.Run && resumed.Run->Combat==result.Run->Combat);
    });
    for(int text=0;text<3;++text)for(int populated=0;populated<3;++populated)It(*FString::Printf(TEXT("連携図鑑の文字%d・発見%dを読み返し保存を変えない"),text,populated),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(150),[this,text,populated]{
        using namespace GirlsSpire;Profile profile;
        if(populated==1)DiscoverCombo(profile,2);
        if(populated==2){for(const auto& recipe:ComboCatalog())DiscoverCombo(profile,recipe.Id);for(const auto& recipe:BondComboCatalog())DiscoverCombo(profile,recipe.Id);DiscoverCombo(profile,9999);}
        Run run(42,Difficulty::Beginner,HeroRole::Sword,profile);
        Files=std::make_shared<BondComboMemory>();Service=std::make_shared<SaveTransaction>(*Files,*Files);
        if(!TestTrue(TEXT("図鑑の元データを保存"),Service->Prepare(RunSaveState{run.Checkpoint(),{}},profile)==SlotIssue::None && Service->Retry()==SlotIssue::None))return;
        auto preferences=std::make_shared<PreferencesStore>(*Files);Preferences settings;settings.Text=static_cast<TextSize>(text);
        if(!TestTrue(TEXT("文字サイズを実際に適用"),preferences->Save(settings)==PreferencesIssue::None))return;
        const auto original=Service->Load();
        Async(EAsyncExecution::TaskGraphMainThread,[this,preferences]{Window->SetContent(SNew(SRunScreen).SaveService(Service).PreferencesService(preferences));}).Wait();DrainSlateLayout();
        const auto click=[&](const char* key){const auto target=Driver->FindElement(By::Id(key));if(!target->IsVisible()){const auto scroll=Driver->FindElement(By::Id("run.scroll"));scroll->ScrollToBeginning();DrainSlateLayout();if(!target->IsVisible()){scroll->ScrollToEndUntil(By::Id(key));DrainSlateLayout();}}return TestTrue(UTF8_TO_TCHAR(key),ClickAndDrain(target));};
        if(!TestTrue(TEXT("タイトルに図鑑入口"),Driver->FindElement(By::Id("title.combo.library"))->Exists()) || !click("title.combo.library"))return;
        TestTrue(TEXT("未発見の個別条件を公開しない"),Driver->FindElement(By::Id("combo.library.entry.3"))->Exists()==(populated==2));
        if(!populated){TestTrue(TEXT("発見0件の説明"),Driver->FindElement(By::Id("combo.library.empty"))->Exists());TestFalse(TEXT("未発見の条件を公開しない"),Driver->FindElement(By::Id("combo.library.entry.2"))->Exists());}
        else for(const int id:profile.DiscoveredCombos){
            const auto key=FString::Printf(TEXT("combo.library.entry.%d"),id);if(!click(TCHAR_TO_UTF8(*key)))return;
            const auto recipe=FindComboRecipe(id);const auto reason=Driver->FindElement(By::Id("combo.library.reason"))->GetText().ToString();
            TestTrue(TEXT("既存条件と代替文言を使用"),reason==(recipe?FString(UTF8_TO_TCHAR(recipe->Reason.data())):FString(TEXT("成立条件は未登録"))));
            if(!click("combo.library.list"))return;
        }
        if(!click("combo.library.back"))return;
        TestTrue(TEXT("タイトルも先頭から表示する"),Driver->FindElement(By::Id("title.heading"))->GetAbsolutePosition().Y>=Driver->FindElement(By::Id("run.scroll"))->GetAbsolutePosition().Y-1);
        const auto after=Service->Load();TestTrue(TEXT("閲覧で周回/乱数/恒久記録/世代を変えない"),after.Run==original.Run && after.Permanent==original.Permanent && after.Generation==original.Generation && preferences->Load().Value==settings);
        Async(EAsyncExecution::TaskGraphMainThread,[this,preferences]{Window->SetContent(SNew(SRunScreen).SaveService(Service).PreferencesService(preferences));}).Wait();DrainSlateLayout();if(!click("title.combo.library"))return;
        TestTrue(TEXT("画面再生成でも発見一覧を維持"),Service->Load().Permanent==profile);
    });
    AfterEach([this]{Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}Service.reset();Files.reset();IAutomationDriverModule::Get().Disable();});
}
