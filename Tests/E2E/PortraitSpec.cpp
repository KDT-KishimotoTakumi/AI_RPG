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
#include "Framework/MetaData/DriverIdMetaData.h"
#include <vector>

namespace {
TSharedPtr<SWidget> FindAppearanceWidget(const TSharedRef<SWidget>& widget,FName id)
{
    if(!widget->GetVisibility().IsVisible())return nullptr;
    const auto tag=widget->GetMetaData<FDriverIdMetaData>();if(tag && tag->Id==id)return widget;
    auto* children=widget->GetChildren();
    for(int i=0;i<children->Num();++i)if(const auto found=FindAppearanceWidget(children->GetChildAt(i),id))return found;
    return nullptr;
}
}

BEGIN_DEFINE_SPEC(FGirlsSpirePortraitSpec,"GirlsSpire.UI.Portraits",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
    std::vector<FColor> CaptureRegion(const char* id) {
        std::vector<FColor> region;
        Async(EAsyncExecution::TaskGraphMainThread,[this,&region,id]{
            const auto target=FindAppearanceWidget(Window.ToSharedRef(),FName(UTF8_TO_TCHAR(id)));
            if(!TestTrue(TEXT("比較する画像が描画される"),target.IsValid()))return;
            const auto geometry=target->GetCachedGeometry();
            const auto position=geometry.LocalToAbsolute(FVector2D::ZeroVector),extent=geometry.LocalToAbsolute(geometry.GetLocalSize())-position;
            TArray<FColor> pixels;FIntVector size;
            if(!TestTrue(TEXT("外見比較用の実画面を取得"),FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),pixels,size)))return;
            TArray64<uint8> png;FImageUtils::PNGCompressImageArray(size.X,size.Y,pixels,png);
            const auto dir=FPaths::ProjectSavedDir()/TEXT("Screenshots/AppearanceCombat")/FGuid::NewGuid().ToString(EGuidFormats::Digits);
            IFileManager::Get().MakeDirectory(*dir,true);TestTrue(TEXT("比較画面を保全"),FFileHelper::SaveArrayToFile(png,*(dir/TEXT("frame.png"))));
            const auto origin=Window->GetCachedGeometry().LocalToAbsolute(FVector2D::ZeroVector);
            const int x=FMath::RoundToInt(position.X-origin.X),y=FMath::RoundToInt(position.Y-origin.Y);
            const int width=FMath::RoundToInt(extent.X),height=FMath::RoundToInt(extent.Y);
            if(!TestTrue(TEXT("画像範囲が撮影内に収まる"),x>=0 && y>=0 && width>0 && height>0 && x+width<=size.X && y+height<=size.Y))return;
            for(int row=y;row<y+height;++row)for(int column=x;column<x+width;++column)region.push_back(pixels[row*size.X+column]);
        }).Wait();
        return region;
    }
END_DEFINE_SPEC(FGirlsSpirePortraitSpec)
void FGirlsSpirePortraitSpec::Define()
{
    BeforeEach([this] {
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        Window=SNew(SWindow).Title(FText::FromString(TEXT("Girls Spire — 立ち絵テスト"))).ClientSize(FVector2D(1100,760));
        FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    It(TEXT("新規外見を両画像へ反映し保存外見を現在設定より優先する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this]{
        using namespace GirlsSpire;
        BattleUnit hero;hero.At={1,1};hero.Speed=30;
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={6,6};enemy.Speed=10;
        const auto original=MakeShared<Battle>(8,8,std::vector<Tile>(64),std::vector<BattleUnit>{hero,enemy},42);
        const auto before=original->Checkpoint();Preferences preferences;preferences.ReducedEffects=true;
        const auto recorded=std::make_shared<std::optional<SavedCombat>>();auto& last=*recorded;
        Profile profile;profile.VictoryRecorded=true;
        const auto show=[&](const Profile& current,std::optional<AppearanceSelection> saved){
            Async(EAsyncExecution::TaskGraphMainThread,[&,current,saved]{
                const auto battle=MakeShared<Battle>(*Battle::Restore(before));
                Window->SetContent(SNew(SCombatScreen).Combat(battle).BondProfile(current).SavedAppearances(saved).DisplayPreferences(preferences)
                    .OnCheckpoint([recorded](const SavedCombat& state){*recorded=state;return true;}));
            }).Wait();DrainSlateLayout();
        };
        show(profile,{});const auto normalPortrait=CaptureRegion("battle.portrait.image.0"),normalPawn=CaptureRegion("battle.unit.art.0");
        if(!TestTrue(TEXT("標準の外見をチェックポイントへ保存"),last && last->Appearances[0]==AppearanceVariant::Standard && last->Battle==before))return;
        const auto standardControl=last->Control;
        profile.SelectedAppearances[0]=AppearanceVariant::Alternate;show(profile,{});
        const auto alternatePortrait=CaptureRegion("battle.portrait.image.0"),alternatePawn=CaptureRegion("battle.unit.art.0");
        const auto different=[](const auto& a,const auto& b){if(a.size()!=b.size() || a.empty())return false;int n=0;for(std::size_t i=0;i<a.size();++i)if(a[i]!=b[i])++n;return n>20;};
        TestTrue(TEXT("立ち絵の実画素が配色へ変化"),different(normalPortrait,alternatePortrait));
        TestTrue(TEXT("戦場スプライトの実画素も配色へ変化"),different(normalPawn,alternatePawn));
        if(!TestTrue(TEXT("配色だけを固定し戦闘/乱数/作戦を変更しない"),last && last->Appearances==profile.SelectedAppearances && last->Battle==before && last->Control==standardControl))return;
        const auto frozen=last->Appearances;profile.SelectedAppearances.fill(AppearanceVariant::Standard);show(profile,frozen);
        TestTrue(TEXT("再開した立ち絵の実画素が以前の配色のまま"),CaptureRegion("battle.portrait.image.0")==alternatePortrait);
        TestTrue(TEXT("再開したスプライトの実画素が以前の配色のまま"),CaptureRegion("battle.unit.art.0")==alternatePawn);
        TestTrue(TEXT("再保存も同じ固定外見"),last && last->Appearances==frozen);
        show(profile,{});TestTrue(TEXT("次の新規戦闘には変更後の標準を使用"),CaptureRegion("battle.portrait.image.0")==normalPortrait);
    });
    It(TEXT("不正な保存外見は表示を止め保存しない"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(60),[this]{
        using namespace GirlsSpire;AppearanceSelection invalid{};invalid[0]=static_cast<AppearanceVariant>(2);const auto writes=std::make_shared<int>(0);
        BattleUnit hero;hero.At={1,1};hero.Speed=30;BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={6,6};enemy.Speed=10;
        auto battle=MakeShared<Battle>(8,8,std::vector<Tile>(64),std::vector<BattleUnit>{hero,enemy},42);
        Async(EAsyncExecution::TaskGraphMainThread,[&]{Window->SetContent(SNew(SCombatScreen).Combat(battle).SavedAppearances(invalid).OnCheckpoint([writes](const SavedCombat&){++*writes;return true;}));}).Wait();DrainSlateLayout();
        TestTrue(TEXT("保存外見の不正を表示"),Driver->FindElement(By::Id("save.appearance.error"))->Exists());
        TestEqual(TEXT("不正な見た目で進行・保存しない"),*writes,0);
    });
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
