#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "FieldCommandSteps.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/CombatScreen.h"
#include "Presentation/CharacterPortraits.h"
#include "Presentation/RunScreen.h"
#include "Application/Encounter.h"
#include "Framework/MetaData/DriverIdMetaData.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "FieldPerformanceSpec.h"
namespace {
FVector2D FieldExtent(const TSharedRef<SWidget>& widget)
{
    const auto tag=widget->GetMetaData<FDriverIdMetaData>();if(tag && tag->Id=="battle.field.root")return widget->GetCachedGeometry().GetAbsoluteSize();
    auto* children=widget->GetChildren();for(int i=0;i<children->Num();++i){const auto size=FieldExtent(children->GetChildAt(i));if(!size.IsZero())return size;}return FVector2D::ZeroVector;
}
TSharedPtr<SWidget> FieldCaptionTarget(const TSharedRef<SWidget>& widget,const FName& id)
{
    const auto tag=widget->GetMetaData<FDriverIdMetaData>();if(tag && tag->Id==id)return widget;
    auto* children=widget->GetChildren();for(int i=0;i<children->Num();++i)if(const auto found=FieldCaptionTarget(children->GetChildAt(i),id))return found;
    return {};
}
TSharedPtr<SWidget> FieldCaptionText(const TSharedRef<SWidget>& widget)
{
    if(widget->GetTypeAsString()==TEXT("STextBlock"))return widget;
    auto* children=widget->GetChildren();for(int i=0;i<children->Num();++i)if(const auto found=FieldCaptionText(children->GetChildAt(i)))return found;
    return {};
}
FBox2D AbsoluteBox(const TSharedPtr<SWidget>& widget)
{
    const auto& geometry=widget->GetCachedGeometry();const auto at=geometry.GetAbsolutePosition();
    return FBox2D(at,at+geometry.GetAbsoluteSize());
}
// 触れるだけは重なりとみなさない。
bool BoxesOverlap(const FBox2D& a,const FBox2D& b)
{
    return FMath::Min(a.Max.X,b.Max.X)-FMath::Max(a.Min.X,b.Min.X)>1.f && FMath::Min(a.Max.Y,b.Max.Y)-FMath::Max(a.Min.Y,b.Min.Y)>1.f;
}
class FieldMemory : public GirlsSpire::SlotFiles,public GirlsSpire::JournalFiles {
public:
    std::array<std::array<std::optional<std::string>,2>,2> Slots{};std::optional<std::string> Journal;
    GirlsSpire::SlotBytes Read(GirlsSpire::SaveKind kind,bool backup)override{return {false,Slots[static_cast<int>(kind)-1][backup]};}
    GirlsSpire::SlotBytes ReadJournal()override{return {false,Journal};}
    bool Replace(GirlsSpire::SaveKind kind,bool backup,std::string_view bytes)override{Slots[static_cast<int>(kind)-1][backup]=std::string(bytes);return true;}
    bool ReplaceJournal(std::string_view bytes)override{Journal=std::string(bytes);return true;}
};
}
BEGIN_DEFINE_SPEC(FGirlsSpireFullFieldSpec,"GirlsSpire.UI.FullField",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;TSharedPtr<SWindow> Window;
    bool Click(const char* id){const auto target=Driver->FindElement(By::Id(id));if(!target->IsVisible() && Driver->FindElement(By::Id("battle.scroll"))->IsVisible()){Driver->FindElement(By::Id("battle.scroll"))->ScrollToEndUntil(By::Id(id));DrainSlateLayout();}return TestTrue(UTF8_TO_TCHAR(id),ClickAndDrain(target));}
END_DEFINE_SPEC(FGirlsSpireFullFieldSpec)
void FGirlsSpireFullFieldSpec::Define()
{
    BeforeEach([this]{FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();Window=SNew(SWindow).ClientSize(FVector2D(1280,900));FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();});
    LatentBeforeEach(WaitForSlateLayout);
    for(int size:{8,12})for(int text=0;text<3;++text)It(*FString::Printf(TEXT("全画面%dマス・文字%dで駒クリックから移動と攻撃"),size,text),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this,size,text]{
        using namespace GirlsSpire;Preferences prefs;prefs.ReducedEffects=true;prefs.Text=static_cast<TextSize>(text);
        BattleUnit hero;hero.At={1,1};hero.Speed=30;hero.Skills[0]=true;auto ally=hero;ally.Id=1;ally.At={1,3};ally.Speed=20;
        auto enemy=hero;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={3,1};enemy.Speed=10;enemy.Hp=enemy.MaxHp=100;
        auto combat=MakeShared<Battle>(size,size,std::vector<Tile>(size*size),std::vector<BattleUnit>{hero,ally,enemy},42);auto expected=*combat;
        Async(EAsyncExecution::TaskGraphMainThread,[this,combat,prefs]{Window->SetContent(SNew(SCombatScreen).Combat(combat).FullField(true).DisplayPreferences(prefs));}).Wait();DrainSlateLayout();
        if(!TestTrue(TEXT("戦場が全画面レイヤー"),Driver->FindElement(By::Id("battle.field.root"))->Exists()))return;
        Async(EAsyncExecution::TaskGraphMainThread,[this]{
            const auto left=FieldCaptionTarget(Window.ToSharedRef(),"battle.cell.2.2");
            const auto right=FieldCaptionTarget(Window.ToSharedRef(),"battle.cell.3.2");
            if(!TestTrue(TEXT("隣接する地面のマスを描画する"),left.IsValid() && right.IsValid()))return;
            const auto& geometry=left->GetCachedGeometry();
            const auto edge=geometry.LocalToAbsolute(FVector2D(geometry.GetLocalSize().X,0));
            const auto start=right->GetCachedGeometry().LocalToAbsolute(FVector2D::ZeroVector);
            TestTrue(TEXT("地続きのマスの間に隙間がない"),FMath::IsNearlyEqual(edge.X,start.X,0.5));
        }).Wait();
        TestEqual(TEXT("通常表示では空きマスの座標を省く"),Driver->FindElement(By::Id("battle.celllabel.2.2"))->GetText().ToString(),FString());
        TestEqual(TEXT("行動名は短くする"),Driver->FindElement(By::Id("battle.field.move"))->GetText().ToString(),FString(TEXT("移動")));
        TestFalse(TEXT("常時の長い説明を省く"),Driver->FindElement(By::Id("battle.help"))->Exists());
        const auto explanationState=combat->Checkpoint();
        if(!Click("battle.field.details"))return;
        TestEqual(TEXT("詳細で座標を確認できる"),Driver->FindElement(By::Id("battle.celllabel.2.2"))->GetText().ToString(),FString(TEXT("2,2")));
        TestTrue(TEXT("詳細で操作説明を確認できる"),Driver->FindElement(By::Id("battle.help"))->Exists());
        if(!Click("battle.field.details"))return;
        TestTrue(TEXT("表示切替は戦闘状態と乱数不変"),combat->Checkpoint()==explanationState);
        Async(EAsyncExecution::TaskGraphMainThread,[this]{const auto actual=FieldExtent(Window.ToSharedRef());const auto client=Window->GetClientSizeInScreen();TestTrue(TEXT("戦場レイヤーはウィンドウ全幅・全高"),actual.X>=client.X*.97f && actual.Y>=client.Y*.97f);}).Wait();
        const auto shot=[this,size,text](const TCHAR* tag){if(!((size==8 && text==0) || (size==12 && text==2)))return;Async(EAsyncExecution::TaskGraphMainThread,[this,size,text,tag]{TArray<FColor> pixels;FIntVector extent;if(!TestTrue(TEXT("全画面フィールドを撮影"),FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),pixels,extent)))return;TArray64<uint8> png;FImageUtils::PNGCompressImageArray(extent.X,extent.Y,pixels,png);const auto directory=FPaths::ProjectSavedDir()/TEXT("Screenshots");IFileManager::Get().MakeDirectory(*directory,true);const auto path=directory/FString::Printf(TEXT("FullField-%s-%d-%d-%s.png"),tag,size,text,*FGuid::NewGuid().ToString(EGuidFormats::Digits));TestTrue(TEXT("検証画像を保持"),FFileHelper::SaveArrayToFile(png,*path));AddInfo(path);}).Wait();};shot(TEXT("menu"));
        // 主人公のコマンド一覧は短く、待機と技は下位メニューで開く。
        TestFalse(TEXT("一覧に待機4方向を並べない"),Driver->FindElement(By::Id("battle.wait.north"))->Exists());
        TestFalse(TEXT("一覧に戻るを出さない"),Driver->FindElement(By::Id("battle.field.back"))->IsVisible());
        if(!Click("battle.field.wait"))return;
        TestTrue(TEXT("待機で4方向を同じ場所に開く"),Driver->FindElement(By::Id("battle.wait.north"))->IsVisible() && Driver->FindElement(By::Id("battle.wait.west"))->IsVisible() && !Driver->FindElement(By::Id("battle.field.move"))->IsVisible());
        shot(TEXT("wait"));
        if(!Click("battle.field.back"))return;
        TestFalse(TEXT("戻ると一覧へ戻る"),Driver->FindElement(By::Id("battle.wait.north"))->Exists());
        if(!Click("battle.field.skills"))return;
        TestTrue(TEXT("技で使える技を開く"),Driver->FindElement(By::Id("battle.skill.0"))->IsVisible());
        shot(TEXT("skills"));
        if(!Click("battle.field.back"))return;
        const auto before=combat->Checkpoint();
        if(!Click("battle.field.close"))return;TestFalse(TEXT("行動メニューを閉じる"),Driver->FindElement(By::Id("battle.field.menu"))->IsVisible());
        if(!Click("battle.unit.select.1"))return;
        TestTrue(TEXT("手番外の味方の名前を確認"),Driver->FindElement(By::Id("battle.field.selection"))->GetText().ToString().Contains(FCharacterPortraits::Name(1)));
        TestTrue(TEXT("情報選択は状態と乱数不変"),combat->Checkpoint()==before);
        if(!Click("battle.unit.select.0"))return;
        TestTrue(TEXT("駒から行動メニューを開く"),Driver->FindElement(By::Id("battle.field.menu"))->IsVisible());
        if(!Click("battle.field.move"))return;
        TestTrue(TEXT("移動を押しても行動メニューは残る"),Driver->FindElement(By::Id("battle.field.menu"))->IsVisible() && !Driver->FindElement(By::Id("battle.field.attack"))->IsVisible());
        shot(TEXT("dock-aim"));
        if(!Click("battle.cell.2.1"))return;
        TestTrue(TEXT("移動先を選ぶと予告と確定が同じメニューに残る"),Driver->FindElement(By::Id("battle.move.preview"))->IsVisible() && Driver->FindElement(By::Id("battle.move.confirm"))->IsVisible());
        shot(TEXT("dock-preview"));
        TestTrue(TEXT("移動先選択で状態不変"),combat->Checkpoint()==before);
        if(!Click("battle.move.confirm"))return;expected.Move(expected.Snapshot().Revision,0,{2,1});
        TestTrue(TEXT("移動は既存ルールと一致"),combat->Checkpoint()==expected.Checkpoint());
        if(!Click("battle.unit.select.0") || !Click("battle.field.attack"))return;
        TestTrue(TEXT("攻撃後はメニューが右端へ寄り敵駒をクリックできる"),Driver->FindElement(By::Id("battle.unit.select.100"))->IsInteractable());
        if(!Click("battle.unit.select.100"))return;
        TestTrue(TEXT("攻撃予告で状態不変"),combat->Checkpoint()==expected.Checkpoint());
        if(!Click("battle.confirm"))return;expected.Attack(expected.Snapshot().Revision,0,{3,1});
        TestTrue(TEXT("攻撃結果と乱数が一致"),combat->Checkpoint()==expected.Checkpoint());
    });
    for(bool suspend:{false,true})It(suspend?TEXT("周回から全画面戦場へ入り中断・再開する"):TEXT("全画面戦場の勝利から周回画面へ戻る"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this,suspend]{
        using namespace GirlsSpire;Profile profile;Run run(42,Difficulty::Beginner,HeroRole::Sword,profile);run.ChooseNode(0,0);
        auto battle=Encounter::Create(run.Snapshot(),run.RandomState(),profile).Checkpoint();
        for(auto& unit:battle.State.Units){if(unit.Id==0){unit.Range=24;unit.Attack=1000;unit.Accuracy=200;}if(unit.Side==Faction::Enemy)unit.Hp=1;}
        CombatControl control;control.ProfileDriven=true;
        const auto files=std::make_shared<FieldMemory>();const auto service=std::make_shared<SaveTransaction>(*files,*files);
        TestTrue(TEXT("中断地点を用意"),service->Prepare(RunSaveState{run.Checkpoint(),SavedCombat{battle,control.Checkpoint()}},profile)==SlotIssue::None && service->Retry()==SlotIssue::None);
        Async(EAsyncExecution::TaskGraphMainThread,[this,service]{Window->SetContent(SNew(SRunScreen).SaveService(service).FullField(true).DevelopmentTools(false));}).Wait();DrainSlateLayout();
        if(!Click("title.resume"))return;
        TestFalse(TEXT("戦場では物語の外枠を隠す"),Driver->FindElement(By::Id("run.frame"))->IsVisible());
        Async(EAsyncExecution::TaskGraphMainThread,[this]{const auto actual=FieldExtent(Window.ToSharedRef());const auto client=Window->GetClientSizeInScreen();TestTrue(TEXT("親画面の余白とスクロールに戦場を閉じ込めない"),actual.X>=client.X*.97f && actual.Y>=client.Y*.97f);}).Wait();
        if(suspend){
            if(!Click("save.suspend"))return;
            TestTrue(TEXT("中断後はタイトル外枠に戻る"),Driver->FindElement(By::Id("run.frame"))->IsVisible() && Driver->FindElement(By::Id("title.resume"))->Exists());
            if(!Click("title.resume"))return;
            const auto saved=service->Load();TestTrue(TEXT("再開で戦闘状態/RNG/保存版を維持"),saved.Run && saved.Run->Combat && saved.Run->Combat->Battle==battle && !saved.Run->ResumeAvailable);
        }else{
            // 駒の隣へ浮かぶメニューが敵駒を覆うことがあるため、先に「攻撃」を選んでメニューを右端へ寄せる。
            if(!Click("battle.field.attack") || !Click("battle.unit.select.100") || !Click("battle.confirm") || !Click("battle.return"))return;
            TestTrue(TEXT("勝利後は周回画面へ戻る"),Driver->FindElement(By::Id("run.frame"))->IsVisible() && !Driver->FindElement(By::Id("battle.field.root"))->Exists());
        }
    });
    It(TEXT("仲間の駒から作戦指示を開き自己対象を選ぶ"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this]{
        using namespace GirlsSpire;BattleUnit ally;ally.Id=1;ally.At={1,1};ally.Speed=30;BattleUnit hero;hero.At={1,2};auto enemy=hero;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={5,5};
        auto combat=MakeShared<Battle>(8,8,std::vector<Tile>(64),std::vector<BattleUnit>{ally,hero,enemy},42);const auto before=combat->Checkpoint();
        Async(EAsyncExecution::TaskGraphMainThread,[this,combat]{Window->SetContent(SNew(SCombatScreen).Combat(combat).FullField(true));}).Wait();DrainSlateLayout();
        if(!Click("battle.field.close") || !Click("battle.unit.select.1") || !Click("intent.kind.2") || !Click("battle.field.intent") || !Click("battle.unit.select.1"))return;
        TestTrue(TEXT("仲間は直接行動へ切り替えず作戦指示を維持"),Driver->FindElement(By::Id("intent.target"))->GetText().ToString().Contains(TEXT("人物1")) && combat->Checkpoint()==before);
    });
    It(TEXT("行動種別の対象を区別し自己回復を確定する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this]{
        using namespace GirlsSpire;BattleUnit hero;hero.At={1,1};hero.Speed=30;hero.Hp=10;hero.Skills[1]=true;
        auto enemy=hero;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={3,1};enemy.Speed=10;enemy.Hp=30;
        auto combat=MakeShared<Battle>(8,8,std::vector<Tile>(64),std::vector<BattleUnit>{hero,enemy},42);auto expected=*combat;const auto before=combat->Checkpoint();
        Async(EAsyncExecution::TaskGraphMainThread,[this,combat]{Window->SetContent(SNew(SCombatScreen).Combat(combat).FullField(true));}).Wait();DrainSlateLayout();
        if(!Click("battle.field.move") || !Click("battle.unit.select.100"))return;
        TestTrue(TEXT("移動中の敵クリックは攻撃にしない"),!Driver->FindElement(By::Id("battle.confirm"))->Exists() && combat->Checkpoint()==before);
        if(!Click("battle.field.open") || !Click("battle.field.attack") || !Click("battle.cell.2.2"))return;
        TestTrue(TEXT("攻撃中の空マスクリックは移動にしない"),!Driver->FindElement(By::Id("battle.move.confirm"))->Exists() && combat->Checkpoint()==before);
        if(!Click("battle.field.open") || !ClickFieldSkill(*this,Driver,[this](const char* id){return Click(id);},"battle.skill.1") || !Click("battle.unit.select.0") || !Click("battle.skill.confirm"))return;
        expected.UseSkill(expected.Snapshot().Revision,0,Skill::FirstAid,{1,1});
        TestTrue(TEXT("自身の駒で回復対象を選び効果/CD/RNGを維持"),combat->Checkpoint()==expected.Checkpoint());
    });
    It(TEXT("保存失敗中は行動メニューを閉じても復旧操作を隠さない"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this]{
        using namespace GirlsSpire;BattleUnit hero;hero.At={1,1};hero.Speed=30;auto enemy=hero;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={5,5};enemy.Speed=10;
        auto combat=MakeShared<Battle>(8,8,std::vector<Tile>(64),std::vector<BattleUnit>{hero,enemy},42);const auto before=combat->Checkpoint();const auto writable=std::make_shared<bool>(false);
        Async(EAsyncExecution::TaskGraphMainThread,[this,combat,writable]{Window->SetContent(SNew(SCombatScreen).Combat(combat).FullField(true).OnCheckpoint([writable](const SavedCombat&){return *writable;}));}).Wait();DrainSlateLayout();
        if(!Click("battle.field.close"))return;
        TestTrue(TEXT("保存停止の復旧ボタンは常に表示"),Driver->FindElement(By::Id("save.retry"))->IsVisible());
        TestTrue(TEXT("進行は不変"),combat->Checkpoint()==before);*writable=true;
        if(!Click("save.retry") || !Click("battle.field.open") || !Click("battle.unit.select.0"))return;
        TestTrue(TEXT("保存後に元の盤面へ戻る"),!Driver->FindElement(By::Id("save.error"))->Exists() && combat->Checkpoint()==before);
    });
    It(TEXT("敵忍の接近技能で倒れる作戦移動を選ばない"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this]{
        using namespace GirlsSpire;BattleUnit ally;ally.Id=1;ally.At={1,1};ally.Speed=40;ally.Hp=13;
        BattleUnit hero;hero.At={7,7};hero.Speed=10;
        BattleUnit ninja;ninja.Id=3;ninja.Side=Faction::Enemy;ninja.Role=BattleRole::Ninja;ninja.At={4,1};ninja.Speed=30;ninja.Movement=4;ninja.Skills={true,true};
        Battle original(8,8,std::vector<Tile>(64),{ally,hero,ninja},42);auto checkpoint=original.Checkpoint();checkpoint.State.AbilityRules=AbilityRuleSet::CompleteCharacters;checkpoint.State.StartingBonds[0]=100;
        const auto restored=Battle::Restore(checkpoint);if(!TestTrue(TEXT("正式技能の戦場を復元"),restored.has_value()))return;
        auto combat=MakeShared<Battle>(*restored);const auto before=combat->Checkpoint();CombatControlCheckpoint control;control.ProfileDriven=true;control.ChoiceRandomState=42;control.Trust[1]=100;
        Async(EAsyncExecution::TaskGraphMainThread,[this,combat,control]{Window->SetContent(SNew(SCombatScreen).Combat(combat).SavedControl(control).FullField(true));}).Wait();DrainSlateLayout();
        if(!Click("battle.field.intent") || !Click("battle.cell.2.1"))return;
        const auto forecast=Driver->FindElement(By::Id("intent.forecast"))->GetText().ToString();
        TestFalse(TEXT("射程3の弱い技だけで安全と判断しない"),forecast.Contains(TEXT("移動(2,1)")));
        TestTrue(TEXT("指示と予告は戦闘状態と乱数を変更しない"),combat->Checkpoint()==before);
    });
    It(TEXT("最大文字でも長い戦闘ボタンの全文を内容領域内へ表示する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this]{
        using namespace GirlsSpire;Preferences prefs;prefs.Text=TextSize::Largest;prefs.ReducedEffects=true;
        BattleUnit hero;hero.At={1,1};hero.Speed=30;hero.Range=3;
        auto enemy=hero;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.Role=BattleRole::Archer;enemy.At={3,1};enemy.Speed=10;enemy.Hp=enemy.MaxHp=100;
        auto combat=MakeShared<Battle>(8,8,std::vector<Tile>(64),std::vector<BattleUnit>{hero,enemy},42);const auto before=combat->Checkpoint();
        Async(EAsyncExecution::TaskGraphMainThread,[this,combat,prefs]{Window->SetContent(SNew(SCombatScreen).Combat(combat).FullField(true).DisplayPreferences(prefs));}).Wait();DrainSlateLayout();
        // 狙う対象の一覧は「攻撃」を押した後に出る。
        if(!Click("battle.field.attack"))return;
        const auto button=Driver->FindElement(By::Id("battle.target.100"));
        if(!button->IsVisible())Driver->FindElement(By::Id("battle.scroll"))->ScrollToEndUntil(By::Id("battle.target.100"));DrainSlateLayout();
        TestFalse(TEXT("対象がいれば理由の一行を出さない"),Driver->FindElement(By::Id("battle.field.notarget"))->Exists());
        TestTrue(TEXT("対象名/HP/操作文言を保持"),button->GetText().ToString()==TEXT("弓兵亡者・敵100を狙う（HP100）"));
        Async(EAsyncExecution::TaskGraphMainThread,[this]{
            const auto button=FieldCaptionTarget(Window.ToSharedRef(),"battle.target.100");if(!TestTrue(TEXT("実ボタンを取得"),button.IsValid()))return;
            const auto caption=FieldCaptionText(button.ToSharedRef());if(!TestTrue(TEXT("実文字レイアウトを取得"),caption.IsValid()))return;
            const auto desired=caption->GetDesiredSize(),allocated=caption->GetCachedGeometry().GetLocalSize();
            AddInfo(FString::Printf(TEXT("ボタン全文: 必要%.1f×%.1f / 配置%.1f×%.1f"),double(desired.X),double(desired.Y),double(allocated.X),double(allocated.Y)));
            TestTrue(TEXT("全文が切れず文字の配置領域に収まる"),desired.X>0 && desired.Y>0 && desired.X<=allocated.X+1 && desired.Y<=allocated.Y+1);
        }).Wait();
        TestTrue(TEXT("全文確認で戦闘/RNGを変更しない"),combat->Checkpoint()==before);
    });
    // 浮かぶ一覧は隣の敵駒を覆わず、最大文字でも盤面領域の内側に収まる。右へ置ける駒と、右端寄りで左へ置く駒の両方を見る。
    for(int size:{8,12})for(const bool rightEdge:{false,true})It(*FString::Printf(TEXT("浮かぶ一覧は隣の敵駒を覆わない（%dマス・%s）"),size,rightEdge?TEXT("右端寄り"):TEXT("左寄り")),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this,size,rightEdge]{
        using namespace GirlsSpire;Preferences prefs;prefs.ReducedEffects=true;prefs.Text=TextSize::Largest;
        const int heroX=rightEdge?size-2:1;
        BattleUnit hero;hero.At={heroX,1};hero.Speed=30;auto enemy=hero;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={heroX+1,1};enemy.Speed=10;
        auto combat=MakeShared<Battle>(size,size,std::vector<Tile>(size*size),std::vector<BattleUnit>{hero,enemy},42);const auto before=combat->Checkpoint();
        Async(EAsyncExecution::TaskGraphMainThread,[this,combat,prefs]{Window->SetContent(SNew(SCombatScreen).Combat(combat).FullField(true).DisplayPreferences(prefs));}).Wait();DrainSlateLayout();
        Async(EAsyncExecution::TaskGraphMainThread,[this,size,heroX]{
            const auto menu=FieldCaptionTarget(Window.ToSharedRef(),"battle.field.menu"),root=FieldCaptionTarget(Window.ToSharedRef(),"battle.field.root");
            const auto enemyCell=FieldCaptionTarget(Window.ToSharedRef(),*FString::Printf(TEXT("battle.cell.%d.1"),heroX+1)),heroCell=FieldCaptionTarget(Window.ToSharedRef(),*FString::Printf(TEXT("battle.cell.%d.1"),heroX));
            if(!TestTrue(TEXT("一覧・盤面・マスを取得"),menu.IsValid() && root.IsValid() && enemyCell.IsValid() && heroCell.IsValid()))return;
            const auto menuBox=AbsoluteBox(menu),rootBox=AbsoluteBox(root);
            TestFalse(TEXT("隣の敵駒のマスを覆わない"),BoxesOverlap(menuBox,AbsoluteBox(enemyCell)));
            TestFalse(TEXT("手番の主人公のマスも覆わない"),BoxesOverlap(menuBox,AbsoluteBox(heroCell)));
            TestTrue(TEXT("一覧が画面から見切れない"),menuBox.Min.X>=rootBox.Min.X-.5f && menuBox.Min.Y>=rootBox.Min.Y-.5f && menuBox.Max.X<=rootBox.Max.X+.5f && menuBox.Max.Y<=rootBox.Max.Y+.5f);
            AddInfo(FString::Printf(TEXT("%dマス 主人公x=%d 一覧 x=%.0f..%.0f y=%.0f..%.0f / 敵マス x=%.0f..%.0f"),size,heroX,menuBox.Min.X,menuBox.Max.X,menuBox.Min.Y,menuBox.Max.Y,AbsoluteBox(enemyCell).Min.X,AbsoluteBox(enemyCell).Max.X));
            if(size!=8 && size!=12)return;
            TArray<FColor> pixels;FIntVector extent;if(!TestTrue(TEXT("撮影"),FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),pixels,extent)))return;
            TArray64<uint8> png;FImageUtils::PNGCompressImageArray(extent.X,extent.Y,pixels,png);const auto directory=FPaths::ProjectSavedDir()/TEXT("Screenshots");IFileManager::Get().MakeDirectory(*directory,true);
            const auto path=directory/FString::Printf(TEXT("FullField-float-%d-%d-%s.png"),size,heroX,*FGuid::NewGuid().ToString(EGuidFormats::Digits));TestTrue(TEXT("検証画像を保持"),FFileHelper::SaveArrayToFile(png,*path));AddInfo(path);
        }).Wait();
        TestTrue(TEXT("配置の確認で戦闘状態を変更しない"),combat->Checkpoint()==before);
    });
    It(TEXT("保存して中断するは行動一覧の外にあり、待機の4方向は一覧と同じ幅"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this]{
        using namespace GirlsSpire;BattleUnit hero;hero.At={1,1};hero.Speed=30;auto enemy=hero;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={6,6};enemy.Speed=10;
        auto combat=MakeShared<Battle>(8,8,std::vector<Tile>(64),std::vector<BattleUnit>{hero,enemy},42);
        Async(EAsyncExecution::TaskGraphMainThread,[this,combat]{Window->SetContent(SNew(SCombatScreen).Combat(combat).FullField(true).OnSuspend([](const SavedCombat&){return true;}));}).Wait();DrainSlateLayout();
        const auto shot=[this](const TCHAR* tag){Async(EAsyncExecution::TaskGraphMainThread,[this,tag]{TArray<FColor> pixels;FIntVector extent;if(!TestTrue(TEXT("撮影"),FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),pixels,extent)))return;TArray64<uint8> png;FImageUtils::PNGCompressImageArray(extent.X,extent.Y,pixels,png);const auto directory=FPaths::ProjectSavedDir()/TEXT("Screenshots");IFileManager::Get().MakeDirectory(*directory,true);const auto path=directory/FString::Printf(TEXT("FullField-%s-%s.png"),tag,*FGuid::NewGuid().ToString(EGuidFormats::Digits));TestTrue(TEXT("検証画像を保持"),FFileHelper::SaveArrayToFile(png,*path));AddInfo(path);}).Wait();};
        shot(TEXT("mainlist-save"));
        Async(EAsyncExecution::TaskGraphMainThread,[this]{
            const auto menu=FieldCaptionTarget(Window.ToSharedRef(),"battle.field.menu"),save=FieldCaptionTarget(Window.ToSharedRef(),"save.suspend");
            if(!TestTrue(TEXT("一覧と保存ボタンを取得"),menu.IsValid() && save.IsValid()))return;
            TestTrue(TEXT("保存ボタンを表示"),save->GetVisibility().IsVisible());
            TestFalse(TEXT("保存ボタンは一覧と重ならない"),BoxesOverlap(AbsoluteBox(menu),AbsoluteBox(save)));
        }).Wait();
        if(!Click("battle.field.wait"))return;
        Async(EAsyncExecution::TaskGraphMainThread,[this]{
            const auto back=FieldCaptionTarget(Window.ToSharedRef(),"battle.field.back");const auto north=FieldCaptionTarget(Window.ToSharedRef(),"battle.wait.north");const auto west=FieldCaptionTarget(Window.ToSharedRef(),"battle.wait.west");
            if(!TestTrue(TEXT("待機の方向ボタンを取得"),back.IsValid() && north.IsValid() && west.IsValid()))return;
            const float list=AbsoluteBox(back).GetSize().X,northWidth=AbsoluteBox(north).GetSize().X;
            AddInfo(FString::Printf(TEXT("一覧の幅%.1f / 北%.1f / 西%.1f"),list,northWidth,AbsoluteBox(west).GetSize().X));
            TestTrue(TEXT("待機の4方向は一覧と同じ幅"),FMath::IsNearlyEqual(list,northWidth,1.f) && FMath::IsNearlyEqual(list,AbsoluteBox(west).GetSize().X,1.f));
        }).Wait();
        shot(TEXT("waitwidth"));
    });
    It(TEXT("攻撃の射程内に対象がいないと理由を出す"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this]{
        using namespace GirlsSpire;BattleUnit hero;hero.At={1,1};hero.Speed=30;auto enemy=hero;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={6,6};enemy.Speed=10;
        auto combat=MakeShared<Battle>(8,8,std::vector<Tile>(64),std::vector<BattleUnit>{hero,enemy},42);const auto before=combat->Checkpoint();
        Async(EAsyncExecution::TaskGraphMainThread,[this,combat]{Window->SetContent(SNew(SCombatScreen).Combat(combat).FullField(true));}).Wait();DrainSlateLayout();
        TestFalse(TEXT("攻撃を押す前は理由を出さない"),Driver->FindElement(By::Id("battle.field.notarget"))->Exists());
        if(!Click("battle.field.attack"))return;
        TestTrue(TEXT("対象がいなければ理由を表示"),Driver->FindElement(By::Id("battle.field.notarget"))->IsVisible() && Driver->FindElement(By::Id("battle.field.notarget"))->GetText().ToString()==TEXT("射程内に攻撃できる相手がいません"));
        TestTrue(TEXT("戻るは残る"),Driver->FindElement(By::Id("battle.field.back"))->IsVisible());
        Async(EAsyncExecution::TaskGraphMainThread,[this]{TArray<FColor> pixels;FIntVector extent;if(!TestTrue(TEXT("撮影"),FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),pixels,extent)))return;TArray64<uint8> png;FImageUtils::PNGCompressImageArray(extent.X,extent.Y,pixels,png);const auto directory=FPaths::ProjectSavedDir()/TEXT("Screenshots");IFileManager::Get().MakeDirectory(*directory,true);const auto path=directory/FString::Printf(TEXT("FullField-notarget-%s.png"),*FGuid::NewGuid().ToString(EGuidFormats::Digits));TestTrue(TEXT("検証画像を保持"),FFileHelper::SaveArrayToFile(png,*path));AddInfo(path);}).Wait();
        TestTrue(TEXT("案内の表示で戦闘/RNGを変更しない"),combat->Checkpoint()==before);
    });
    AfterEach([this]{Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}IAutomationDriverModule::Get().Disable();});
}
