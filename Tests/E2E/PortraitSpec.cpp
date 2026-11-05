#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/CombatScreen.h"
#include "Presentation/CharacterPortraits.h"
#include "Domain/Appearance.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"

BEGIN_DEFINE_SPEC(FGirlsSpirePortraitSpec,"GirlsSpire.UI.Portraits",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
END_DEFINE_SPEC(FGirlsSpirePortraitSpec)
void FGirlsSpirePortraitSpec::Define()
{
    BeforeEach([this] {
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        Window=SNew(SWindow).Title(FText::FromString(TEXT("Girls Spire — 立ち絵テスト"))).ClientSize(FVector2D(1100,760));
        FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    It(TEXT("配色差分を標準と別に読み込み不正値と欠落を拒否する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(60),[this] {
        Async(EAsyncExecution::TaskGraphMainThread,[this] {
            using GirlsSpire::AppearanceVariant;
            FCharacterPortraits portraits;
            const auto standard=portraits.Get(0);
            const auto alternate=portraits.Get(0,AppearanceVariant::Alternate);
            if(!TestTrue(TEXT("標準と配色差分の実PNG読込み"),standard.IsValid() && alternate.IsValid()))return;
            TestTrue(TEXT("別リソースで混同しない"),standard!=alternate);
            TestTrue(TEXT("標準キャッシュを保持"),standard==portraits.Get(0,AppearanceVariant::Standard));
            TestTrue(TEXT("配色キャッシュを再利用"),alternate==portraits.Get(0,AppearanceVariant::Alternate));
            TestTrue(TEXT("配色画像の対応"),alternate->GetResourceName().ToString().EndsWith(TEXT("hero-ren-dye1.png")));
            TestTrue(TEXT("配色でも寸法は固定"),alternate->ImageSize==standard->ImageSize && alternate->ImageSize==FVector2D(1024,1536));
            const TCHAR* stems[]={TEXT("hero-ren"),TEXT("akane"),TEXT("mio"),TEXT("oboro"),TEXT("shion"),TEXT("kaede"),TEXT("honoka"),TEXT("chikage"),TEXT("sayo")};
            for(int id=0;id<=8;++id) {
                const auto image=portraits.Get(id,AppearanceVariant::Alternate);
                if(!TestTrue(FString::Printf(TEXT("人物%dの配色PNGを実読込み"),id),image.IsValid()))continue;
                TestTrue(TEXT("人物別の配色画像に対応"),image->GetResourceName().ToString().EndsWith(FString(stems[id])+TEXT("-dye1.png")));
                TestTrue(TEXT("配色別キャッシュと標準を混同しない"),image==portraits.Get(id,AppearanceVariant::Alternate) && image!=portraits.Get(id));
                TestTrue(TEXT("9人とも実画像寸法を維持"),image->ImageSize==FVector2D(1024,1536));
            }
            TestFalse(TEXT("負の配色値を拒否"),portraits.Get(0,static_cast<AppearanceVariant>(-1)).IsValid());
            TestFalse(TEXT("範囲外配色を拒否"),portraits.Get(0,static_cast<AppearanceVariant>(2)).IsValid());
            TestFalse(TEXT("不正人物の配色を拒否"),portraits.Get(-1,AppearanceVariant::Alternate).IsValid());
            TestFalse(TEXT("上限外人物の配色を拒否"),portraits.Get(9,AppearanceVariant::Alternate).IsValid());
            FCharacterPortraits missing(FPaths::ProjectSavedDir()/TEXT("Tests/Portraits/NotInstalled"));
            for(int id=0;id<=8;++id)TestFalse(TEXT("欠落する配色を標準と偽らず安全に返す"),missing.Get(id,AppearanceVariant::Alternate).IsValid());
        }).Wait();
    });
    It(TEXT("画像対応・再利用・欠落・不正な人物を安全に扱う"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(60),[this] {
        Async(EAsyncExecution::TaskGraphMainThread,[this] {
            FCharacterPortraits portraits;
            const TCHAR* names[]={TEXT("黒鉄 蓮"),TEXT("真田 茜"),TEXT("白峰 澪"),TEXT("朧"),TEXT("土御門 紫苑"),TEXT("鷹司 楓"),TEXT("雑賀 火乃香"),TEXT("柳生 千景"),TEXT("鈴鹿 小夜")};
            const TCHAR* files[]={TEXT("hero-ren"),TEXT("akane"),TEXT("mio"),TEXT("oboro"),TEXT("shion"),TEXT("kaede"),TEXT("honoka"),TEXT("chikage"),TEXT("sayo")};
            for(int id=0;id<9;++id) {
                TestEqual(TEXT("人物名対応"),FString(FCharacterPortraits::Name(id)),FString(names[id]));
                const auto image=portraits.Get(id);
                if(!TestTrue(TEXT("PNGの実読込み"),image.IsValid()))continue;
                TestTrue(TEXT("同じ画面内では再利用"),image==portraits.Get(id));
                TestTrue(TEXT("人物と画像の対応"),image->GetResourceName().ToString().EndsWith(FString(files[id])+TEXT("-v1.png")));
                TestTrue(TEXT("実画像サイズ"),image->ImageSize==FVector2D(1024,1536));
            }
            TestFalse(TEXT("負のIDを拒否"),portraits.Get(-1).IsValid());TestFalse(TEXT("範囲外IDを拒否"),portraits.Get(9).IsValid());
            TestEqual(TEXT("不正人物名を安全に表示"),FString(FCharacterPortraits::Name(-1)),FString(TEXT("不明な人物")));
            TestEqual(TEXT("上限外人物名も安全"),FString(FCharacterPortraits::Name(9)),FString(TEXT("不明な人物")));
            FCharacterPortraits missing(FPaths::ProjectSavedDir()/TEXT("Tests/Portraits/NotInstalled"));
            TestFalse(TEXT("欠落画像は代替表示へ返す"),missing.Get(0).IsValid());
        }).Wait();
    });
    for(int id=0;id<=8;++id)It(*FString::Printf(TEXT("人物%dの実画像・名前・HPを戦闘中に表示する"),id),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(60),[this,id] {
        using namespace GirlsSpire;
        BattleUnit actor;actor.Id=id;actor.At={1,1};actor.Hp=21;actor.MaxHp=30;actor.Speed=20;
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={6,6};enemy.Hp=enemy.MaxHp=100;
        auto combat=MakeShared<Battle>(8,8,std::vector<Tile>(64),std::vector<BattleUnit>{actor,enemy},42);
        Async(EAsyncExecution::TaskGraphMainThread,[this,combat]{Window->SetContent(SNew(SCombatScreen).Combat(combat));}).Wait();DrainSlateLayout();
        const auto image=FString::Printf(TEXT("battle.portrait.image.%d"),id);
        const auto name=FString::Printf(TEXT("battle.portrait.name.%d"),id);
        const auto hp=FString::Printf(TEXT("battle.portrait.hp.%d"),id);
        TestTrue(TEXT("実画像を読み込んだ立ち絵を表示"),Driver->FindElement(By::Id(*image))->IsVisible());
        TestTrue(TEXT("人物名を表示"),Driver->FindElement(By::Id(*name))->Exists());
        if(TestTrue(TEXT("HP表示あり"),Driver->FindElement(By::Id(*hp))->Exists()))
            TestTrue(TEXT("現在HPと最大HPが一致"),Driver->FindElement(By::Id(*hp))->GetText().ToString().Contains(TEXT("21 / 30")));
        TestFalse(TEXT("敵の味方立ち絵は表示しない"),Driver->FindElement(By::Id("battle.portrait.image.100"))->Exists());
    });
    It(TEXT("5人の立ち絵と戦闘不能を表示し待機後にHPを更新する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(90),[this] {
        using namespace GirlsSpire;
        std::vector<BattleUnit> units;
        for(int id=0;id<5;++id){BattleUnit actor;actor.Id=id;actor.At={1,id+1};actor.Speed=20-id;actor.Defense=0;actor.Hp=id==4?0:30;units.push_back(actor);}
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={2,1};enemy.Accuracy=200;enemy.Speed=10;units.push_back(enemy);
        auto combat=MakeShared<Battle>(8,8,std::vector<Tile>(64),units,42);
        Async(EAsyncExecution::TaskGraphMainThread,[this,combat]{Window->SetContent(SNew(SCombatScreen).Combat(combat));}).Wait();DrainSlateLayout();
        for(int id=0;id<5;++id)TestTrue(TEXT("5人全員の画像を同時表示"),Driver->FindElement(By::Id(*FString::Printf(TEXT("battle.portrait.image.%d"),id)))->IsVisible());
        TestEqual(TEXT("戦闘不能は文字でも示す"),Driver->FindElement(By::Id("battle.portrait.state.4"))->GetText().ToString(),FString(TEXT("戦闘不能")));
        const auto wait=Driver->FindElement(By::Id("battle.wait.south"));
        for(int turn=0;turn<4;++turn){
            const auto scroll=Driver->FindElement(By::Id("battle.scroll"));scroll->ScrollToEndUntil(By::Id("battle.wait.south"));DrainSlateLayout();
            if(!TestTrue(TEXT("立ち絵表示中もマウスで待機"),ClickAndDrain(wait)))return;
        }
        const auto hp=combat->Snapshot().Units.front().Hp;TestTrue(TEXT("敵の攻撃を受けた"),hp<30);
        TestEqual(TEXT("行動後のHPへ更新"),Driver->FindElement(By::Id("battle.portrait.hp.0"))->GetText().ToString(),FString::Printf(TEXT("HP %d / 30"),hp));
        Async(EAsyncExecution::TaskGraphMainThread,[this] {
            TArray<FColor> pixels;FIntVector size;
            if(FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),pixels,size)) {
                TArray64<uint8> png;FImageUtils::PNGCompressImageArray(size.X,size.Y,pixels,png);
                const auto folder=FPaths::ProjectSavedDir()/TEXT("Screenshots");IFileManager::Get().MakeDirectory(*folder,true);
                FFileHelper::SaveArrayToFile(png,*(folder/TEXT("Portraits-Party.png")));
            }
        }).Wait();
    });
    It(TEXT("画像欠落時も名前・HPとマウス操作を維持する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(60),[this] {
        using namespace GirlsSpire;
        BattleUnit hero;hero.At={1,1};hero.Speed=20;
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={6,6};
        auto combat=MakeShared<Battle>(8,8,std::vector<Tile>(64),std::vector<BattleUnit>{hero,enemy},42);
        auto missing=MakeShared<FCharacterPortraits>(FPaths::ProjectSavedDir()/TEXT("Tests/Portraits/NotInstalled"));
        Async(EAsyncExecution::TaskGraphMainThread,[this,combat,missing]{Window->SetContent(SNew(SCombatScreen).Combat(combat).Portraits(missing));}).Wait();DrainSlateLayout();
        TestTrue(TEXT("画像欠落を文字で表示"),Driver->FindElement(By::Id("battle.portrait.missing.0"))->IsVisible());
        TestEqual(TEXT("名前を維持"),Driver->FindElement(By::Id("battle.portrait.name.0"))->GetText().ToString(),FString(TEXT("黒鉄 蓮")));
        TestEqual(TEXT("HPを維持"),Driver->FindElement(By::Id("battle.portrait.hp.0"))->GetText().ToString(),FString(TEXT("HP 30 / 30")));
        const auto before=combat->Snapshot().Revision;
        Driver->FindElement(By::Id("battle.scroll"))->ScrollToEndUntil(By::Id("battle.wait.south"));DrainSlateLayout();
        if(TestTrue(TEXT("画像なしでも待機可能"),ClickAndDrain(Driver->FindElement(By::Id("battle.wait.south")))))
            TestTrue(TEXT("実際に手番が進む"),combat->Snapshot().Revision>before);
    });
    AfterEach([this] {
        Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}
        IAutomationDriverModule::Get().Disable();
    });
}
