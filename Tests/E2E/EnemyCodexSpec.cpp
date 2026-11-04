#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Presentation/RunScreen.h"
#include "Presentation/BoardSprites.h"
#include "Domain/EnemyCatalog.h"
namespace {
class EnemyCodexMemory:public GirlsSpire::SlotFiles,public GirlsSpire::JournalFiles,public GirlsSpire::PreferencesFiles {
public:
    std::array<std::array<std::optional<std::string>,2>,2> Slots{};
    std::optional<std::string> Journal,Settings;bool Fail=false;std::vector<std::string> JournalRequests;
    GirlsSpire::SlotBytes Read(GirlsSpire::SaveKind kind,bool backup)override{return {false,Slots[static_cast<int>(kind)-1][backup]};}
    GirlsSpire::SlotBytes ReadJournal()override{return {false,Journal};}
    bool Replace(GirlsSpire::SaveKind kind,bool backup,std::string_view bytes)override{if(Fail)return false;Slots[static_cast<int>(kind)-1][backup]=std::string(bytes);return true;}
    bool ReplaceJournal(std::string_view bytes)override{JournalRequests.emplace_back(bytes);if(Fail)return false;Journal=std::string(bytes);return true;}
    GirlsSpire::SlotBytes ReadPreferences()override{return {false,Settings};}
    bool ReplacePreferences(std::string_view bytes)override{Settings=std::string(bytes);return true;}
    bool PreservePreferences()override{return true;}
};
}
BEGIN_DEFINE_SPEC(FGirlsSpireEnemyCodexSpec,"GirlsSpire.UI.EnemyCodex",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;TSharedPtr<SWindow> Window;
    std::shared_ptr<EnemyCodexMemory> Files;std::shared_ptr<GirlsSpire::SaveTransaction> Service;
    bool Click(const char* key){const auto element=Driver->FindElement(By::Id(key));if(!element->IsVisible()){const auto scroll=Driver->FindElement(By::Id("run.scroll"));scroll->ScrollToBeginning();DrainSlateLayout();if(!element->IsVisible()){scroll->ScrollToEndUntil(By::Id(key));DrainSlateLayout();}}return TestTrue(UTF8_TO_TCHAR(key),ClickAndDrain(element));}
END_DEFINE_SPEC(FGirlsSpireEnemyCodexSpec)
void FGirlsSpireEnemyCodexSpec::Define()
{
    BeforeEach([this]{FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();Window=SNew(SWindow).ClientSize(FVector2D(1200,1000));FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();Files=std::make_shared<EnemyCodexMemory>();Service=std::make_shared<GirlsSpire::SaveTransaction>(*Files,*Files);});
    LatentBeforeEach(WaitForSlateLayout);
    for(int text=0;text<3;++text)for(int id=0;id<=12;++id)It(*FString::Printf(TEXT("敵図鑑%dを文字%dで再読し保存を変えない"),id,text),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(90),[this,id,text]{
        using namespace GirlsSpire;Profile profile;profile.DiscoveredCombos={2};if(id)profile.DiscoveredEnemies={id};
        Run run(42,Difficulty::Beginner,HeroRole::Sword,profile);
        if(!TestTrue(TEXT("図鑑の元データを保存"),Service->Prepare(RunSaveState{run.Checkpoint(),{}},profile)==SlotIssue::None && Service->Retry()==SlotIssue::None))return;
        auto settings=std::make_shared<PreferencesStore>(*Files);Preferences preferences;preferences.Text=static_cast<TextSize>(text);if(!TestTrue(TEXT("文字サイズを適用"),settings->Save(preferences)==PreferencesIssue::None))return;
        const auto before=Service->Load();const auto slots=Files->Slots;const auto journal=Files->Journal;
        Async(EAsyncExecution::TaskGraphMainThread,[this,settings]{Window->SetContent(SNew(SRunScreen).SaveService(Service).PreferencesService(settings));}).Wait();DrainSlateLayout();
        if(!TestTrue(TEXT("敵図鑑の入口"),Driver->FindElement(By::Id("title.enemy.library"))->Exists()) || !Click("title.enemy.library"))return;
        if(id==0){TestTrue(TEXT("未発見の説明"),Driver->FindElement(By::Id("enemy.library.empty"))->Exists());TestFalse(TEXT("未発見の敵を公開しない"),Driver->FindElement(By::Id("enemy.library.entry.1"))->Exists());}
        else {
            const int hidden=id==1?2:1;TestFalse(TEXT("他の未発見種を公開しない"),Driver->FindElement(By::Id(*FString::Printf(TEXT("enemy.library.entry.%d"),hidden)))->Exists());
            const auto key=FString::Printf(TEXT("enemy.library.entry.%d"),id);if(!Click(TCHAR_TO_UTF8(*key)))return;
            if(id==12 && text==2){
                const auto top=Driver->FindElement(By::Id("run.scroll"))->GetAbsolutePosition().Y;
                const auto name=Driver->FindElement(By::Id("enemy.library.name"))->GetAbsolutePosition().Y;
                const auto picture=Driver->FindElement(By::Id("enemy.library.image"))->GetAbsolutePosition().Y;
                TestTrue(TEXT("詳細を開いた直後は名前と画像がスクロール領域の上に隠れない"),name>=top-1 && picture>=top-1);
            }
            const auto definition=FindEnemyDefinition(id);if(!TestNotNull(TEXT("共通の敵定義"),definition))return;
            const auto& unit=definition->Base;
            TestTrue(TEXT("名称と固有行動"),Driver->FindElement(By::Id("enemy.library.name"))->GetText().ToString()==UTF8_TO_TCHAR(definition->Name.data()) && Driver->FindElement(By::Id("enemy.library.description"))->GetText().ToString()==UTF8_TO_TCHAR(definition->Description.data()));
            const auto expected=FString::Printf(TEXT("HP %d / 攻撃 %d / 防御 %d\n素早さ %d / 移動 %d / 射程 %d"),unit.MaxHp,unit.Attack,unit.Defense,unit.Speed,unit.Movement,unit.Range);
            TestTrue(TEXT("生成と同じ基準能力を表示"),Driver->FindElement(By::Id("enemy.library.stats"))->GetText().ToString()==expected);
            TestTrue(TEXT("既存画像と敵種を一致"),FBoardSprites::EnemyFrame(unit)==id-1 && Driver->FindElement(By::Id("enemy.library.image"))->Exists());
            if(id==12 && text==2){
                bool captured=false;FString path;
                Async(EAsyncExecution::TaskGraphMainThread,[&]{
                    TArray<FColor> pixels;FIntVector size;
                    if(FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),pixels,size)){
                        TArray64<uint8> png;FImageUtils::PNGCompressImageArray(size.X,size.Y,pixels,png);
                        const auto folder=FPaths::ProjectSavedDir()/TEXT("Screenshots/EnemyCodex")/FGuid::NewGuid().ToString(EGuidFormats::Digits);
                        IFileManager::Get().MakeDirectory(*folder,true);path=folder/TEXT("lord-largest.png");captured=FFileHelper::SaveArrayToFile(png,*path);
                    }
                }).Wait();
                TestTrue(TEXT("最大文字の敵図鑑を描画保存"),captured);if(captured)AddInfo(FString(TEXT("敵図鑑画像: "))+path);
            }
            if(!Click("enemy.library.list"))return;
        }
        if(!Click("enemy.library.back"))return;
        const auto after=Service->Load();TestTrue(TEXT("閲覧は周回/乱数/恒久記録/世代/保存バイトを変えない"),after.Run==before.Run && after.Permanent==before.Permanent && after.Generation==before.Generation && Files->Slots==slots && Files->Journal==journal && settings->Load().Value==preferences);
        Async(EAsyncExecution::TaskGraphMainThread,[this,settings]{Window->SetContent(SNew(SRunScreen).SaveService(Service).PreferencesService(settings));}).Wait();DrainSlateLayout();if(!Click("title.enemy.library"))return;
        TestTrue(TEXT("画面再生成でも記録を維持"),Service->Load().Permanent==profile);
    });
    It(TEXT("開始保存失敗で発見を確定せず同じ内容を再試行する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this]{
        using namespace GirlsSpire;Profile profile;profile.Bonds.fill(60);profile.DiscoveredCombos={2};auto run=std::make_shared<Run>(42,Difficulty::Beginner,HeroRole::Sword,profile);run->ChooseNode(run->Snapshot().Revision,0);
        Service->Prepare(RunSaveState{run->Checkpoint(),{}},profile);Service->Retry();
        Async(EAsyncExecution::TaskGraphMainThread,[this,run]{Window->SetContent(SNew(SRunScreen).InitialRun(run).SaveService(Service).DevelopmentTools(false));}).Wait();DrainSlateLayout();
        const auto savedSlots=Files->Slots;const auto savedJournal=Files->Journal;Files->Fail=true;const auto request=Files->JournalRequests.size();if(!Click("battle.start"))return;
        TestTrue(TEXT("開始保存失敗は停止し恒久記録を確定しない"),Driver->FindElement(By::Id("save.error"))->Exists() && Service->HasPending() && Service->Load().Issue==SlotIssue::Pending && Files->Slots==savedSlots && Files->Journal==savedJournal && run->ProfileState()==profile);
        if(!Click("save.retry"))return;
        TestTrue(TEXT("同じ開始保存内容だけを再試行"),Files->JournalRequests.size()>=request+2 && Files->JournalRequests[request]==Files->JournalRequests[request+1] && Files->Slots==savedSlots && Files->Journal==savedJournal);
        Files->Fail=false;if(!Click("save.retry"))return;
        const auto started=Service->Load();if(!TestTrue(TEXT("開始戦闘を確定"),started.Run && started.Run->Combat))return;
        auto expected=profile;DiscoverEnemies(expected,started.Run->Combat->Battle.State.Units);
        if(!TestTrue(TEXT("開始と発見を同時保存・他の恒久記録を維持"),!expected.DiscoveredEnemies.empty() && started.Permanent==expected && run->ProfileState()==expected))return;
        if(!Click("save.suspend") || !Click("title.start") || !Click("save.abandon.confirm"))return;
        TestTrue(TEXT("周回放棄でも発見を保持"),Service->Load().Permanent==expected);
    });
    It(TEXT("途中召喚の初出現も保存失敗中は確定せず再試行する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this]{
        using namespace GirlsSpire;Run run(42,Difficulty::Beginner,HeroRole::Sword);auto progress=run.Checkpoint();progress.State.Day=12;progress.State.Phase=RunPhase::Event;progress.State.Active={NodeKind::FinalBoss};progress.State.Choices.clear();
        BattleUnit hero;hero.At={1,1};hero.Speed=50;hero.Hp=hero.MaxHp=1000;auto lord=CreateEnemy(EnemyKind::Lord,100,{4,4});
        Battle arena(8,8,std::vector<Tile>(64),{hero,lord},42,BattleGoal::DefeatLord,3,progress.State.Wallet.Medicines);auto checkpoint=arena.Checkpoint();checkpoint.State.Round=3;checkpoint.State.Combos.Round=3;
        CombatControl control;Service->Prepare(RunSaveState{progress,SavedCombat{checkpoint,control.Checkpoint()}},{});Service->Retry();
        Async(EAsyncExecution::TaskGraphMainThread,[this]{Window->SetContent(SNew(SRunScreen).SaveService(Service));}).Wait();DrainSlateLayout();if(!Click("title.resume"))return;
        if(!TestTrue(TEXT("開始時は城主だけ発見"),Service->Load().Permanent.DiscoveredEnemies==std::vector<int>{12}))return;
        const auto savedSlots=Files->Slots;const auto savedJournal=Files->Journal;Files->Fail=true;if(!Click("battle.wait.south"))return;
        TestTrue(TEXT("召喚後の保存失敗は発見を確定しない"),Driver->FindElement(By::Id("save.error"))->Exists() && Service->Load().Issue==SlotIssue::Pending && Files->Slots==savedSlots && Files->Journal==savedJournal);
        Files->Fail=false;if(!Click("save.retry"))return;
        const auto saved=Service->Load();TestTrue(TEXT("途中出現した足軽を重複なく保存"),saved.Run && saved.Run->Combat && saved.Permanent.DiscoveredEnemies==std::vector<int>({1,12}));
    });
    AfterEach([this]{Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}Service.reset();Files.reset();IAutomationDriverModule::Get().Disable();});
}
