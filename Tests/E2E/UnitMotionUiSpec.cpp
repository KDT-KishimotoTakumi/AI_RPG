#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MetaData/DriverIdMetaData.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/CombatScreen.h"
#include "AI/BasicTurn.h"
#include "../Fixtures/TeamUltimateBattle.h"
#include "Presentation/CharacterPortraits.h"
#include "Misc/Paths.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
namespace {
TSharedPtr<SWidget> MotionFindTag(const TSharedRef<SWidget>& widget,FName id)
{
    const auto tag=widget->GetMetaData<FDriverIdMetaData>();if(tag && tag->Id==id)return widget;
    auto* children=widget->GetChildren();for(int i=0;i<children->Num();++i)if(auto found=MotionFindTag(children->GetChildAt(i),id))return found;
    return nullptr;
}
void MotionWait(float seconds)
{
    auto done=MakeShared<TPromise<void>,ESPMode::ThreadSafe>();auto ready=done->GetFuture();
    Async(EAsyncExecution::TaskGraphMainThread,[done,seconds]{FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([done](float){done->SetValue();return false;}),seconds);}).Wait();ready.Wait();
}
}
BEGIN_DEFINE_SPEC(FGirlsSpireMotionUiSpec,"GirlsSpire.UI.UnitMotion",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    TSharedPtr<SWindow> Window;TSharedPtr<SCombatScreen> Screen;FAutomationDriverPtr Driver;
    void CaptureMotion(const TCHAR* name){Async(EAsyncExecution::TaskGraphMainThread,[this,name]{TArray<FColor> pixels;FIntVector size;if(!TestTrue(TEXT("2D画面の撮影"),FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),pixels,size)))return;TArray64<uint8> png;FImageUtils::PNGCompressImageArray(size.X,size.Y,pixels,png);const auto folder=FPaths::ProjectSavedDir()/TEXT("Screenshots");IFileManager::Get().MakeDirectory(*folder,true);TestTrue(TEXT("2D表示の証跡を保持"),FFileHelper::SaveArrayToFile(png,*(folder/name)));}).Wait();}
    FVector2D Translation(const TCHAR* tag=TEXT("battle.unit.art.0")){FVector2D result=FVector2D::ZeroVector;Async(EAsyncExecution::TaskGraphMainThread,[this,&result,tag]{const auto widget=MotionFindTag(Window.ToSharedRef(),tag);if(widget && widget->GetRenderTransform().IsSet())result=FVector2D(widget->GetRenderTransform()->GetTranslation());}).Wait();return result;}
END_DEFINE_SPEC(FGirlsSpireMotionUiSpec)
void FGirlsSpireMotionUiSpec::Define()
{
    BeforeEach([this]{FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();Driver=IAutomationDriverModule::Get().CreateDriver();Window=SNew(SWindow).ClientSize(FVector2D(1200,900));FSlateApplication::Get().AddWindow(Window.ToSharedRef());});
    LatentBeforeEach(WaitForSlateLayout);
    for(int mode=0;mode<3;++mode)It(*FString::Printf(TEXT("2D移動と待機・軽減・画像欠落%dで状態と再開を維持する"),mode),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this,mode]{
        using namespace GirlsSpire;BattleUnit hero;hero.At={1,1};hero.Speed=99;hero.Movement=6;hero.Hp=hero.MaxHp=40;
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={6,6};enemy.Speed=0;
        auto battle=MakeShared<Battle>(8,8,std::vector<Tile>(64),std::vector<BattleUnit>{hero,enemy},42);
        Preferences prefs;prefs.ReducedEffects=mode==1;auto portraits=mode==2?MakeShared<FCharacterPortraits>(FPaths::ProjectSavedDir()/TEXT("Tests/missing-motion-images")):MakeShared<FCharacterPortraits>();
        Async(EAsyncExecution::TaskGraphMainThread,[this,battle,prefs,portraits]{Window->SetContent(SAssignNew(Screen,SCombatScreen).Combat(battle).DisplayPreferences(prefs).Portraits(portraits));}).Wait();DrainSlateLayout();
        const auto initial=battle->Checkpoint();const auto a=Translation();MotionWait(.21f);const auto b=Translation();MotionWait(.21f);const auto c=Translation();
        if(mode==0)TestTrue(TEXT("実描画の人物が時間で揺れる"),!a.Equals(b,.0001) || !a.Equals(c,.0001));
        if(mode==1)TestTrue(TEXT("演出軽減では完全静止"),a.IsZero() && b.IsZero() && c.IsZero());
        TestTrue(TEXT("待機演出で状態/RNG不変"),battle->Checkpoint()==initial);
        auto expected=*battle;TestTrue(TEXT("同じ規則の移動を準備"),expected.Move(expected.Snapshot().Revision,0,{4,3}));
        if(!TestTrue(TEXT("移動先を実クリック"),ClickAndDrain(Driver->FindElement(By::Id("battle.cell.4.3")))))return;
        if(!TestTrue(TEXT("移動を実確定"),ClickAndDrain(Driver->FindElement(By::Id("battle.move.confirm")))))return;
        TestTrue(TEXT("確定結果と乱数は既存規則と一致"),battle->Checkpoint()==expected.Checkpoint());
        TestEqual(TEXT("通常は移動用2Dレイヤー、軽減は即時表示"),Driver->FindElement(By::Id("battle.motion.unit.0"))->Exists(),mode!=1);
        if(mode==0)CaptureMotion(TEXT("UnitMotion-Moving.png"));
        const auto control=Screen->ControlCheckpoint();const auto settled=battle->Checkpoint();MotionWait(.8f);DrainSlateLayout();
        Async(EAsyncExecution::TaskGraphMainThread,[this]{const auto ghost=MotionFindTag(Window.ToSharedRef(),"battle.motion.unit.0");TestTrue(TEXT("移動演出は有限時間で終了"),!ghost || !ghost->GetVisibility().IsVisible());}).Wait();
        TestTrue(TEXT("演出完了でも戦闘/作戦乱数不変"),battle->Checkpoint()==settled && Screen->ControlCheckpoint()==control);
        if(mode==0)CaptureMotion(TEXT("UnitMotion-Settled.png"));
        Async(EAsyncExecution::TaskGraphMainThread,[this,battle,prefs,portraits,control]{Window->SetContent(SAssignNew(Screen,SCombatScreen).Combat(battle).DisplayPreferences(prefs).Portraits(portraits).SavedControl(control));}).Wait();DrainSlateLayout();
        TestFalse(TEXT("画面再生成で移動を再生し直さない"),Driver->FindElement(By::Id("battle.motion.unit.0"))->Exists());
        TestTrue(TEXT("再開で確定位置/全状態不変"),battle->Checkpoint()==settled);
    });
    for(const bool reduced:{false,true})It(reduced?TEXT("被弾時も演出軽減で駒を揺らさない"):TEXT("確定した被弾で駒だけ反応し、確定位置へ戻る"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(90),[this,reduced]{
        using namespace GirlsSpire;
        BattleUnit hero;hero.At={1,1};hero.Speed=99;hero.Attack=12;hero.Accuracy=200;
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={2,1};enemy.Hp=enemy.MaxHp=100;enemy.Speed=0;
        auto battle=MakeShared<Battle>(8,8,std::vector<Tile>(64),std::vector<BattleUnit>{hero,enemy},42);
        Preferences prefs;prefs.ReducedEffects=reduced;
        Async(EAsyncExecution::TaskGraphMainThread,[this,battle,prefs]{Window->SetContent(SAssignNew(Screen,SCombatScreen).Combat(battle).DisplayPreferences(prefs));}).Wait();DrainSlateLayout();
        auto expected=*battle;
        if(!TestTrue(TEXT("同じ規則で被害を確定"),expected.Attack(expected.Snapshot().Revision,0,{2,1}) && !expected.LastHits().empty() && expected.LastHits().front().Damage>0))return;
        const auto control=Screen->ControlCheckpoint();
        if(!TestTrue(TEXT("攻撃対象を実クリック"),ClickAndDrain(Driver->FindElement(By::Id("battle.cell.2.1")))))return;
        if(!TestTrue(TEXT("通常攻撃を実確定"),ClickAndDrain(Driver->FindElement(By::Id("battle.confirm")))))return;
        MotionWait(.05f);
        const auto attackShift=Translation().X;
        TestTrue(TEXT("確定通常攻撃の画像は対象方向へ動き、軽減では静止"),reduced?FMath::IsNearlyZero(attackShift):attackShift>0.1);
        const auto shift=Translation(TEXT("battle.unit.art.100")).X;
        TestTrue(TEXT("通常は被弾で画像が反応し、軽減では横移動しない"),reduced?FMath::IsNearlyZero(shift):shift>0.1);
        if(!reduced)CaptureMotion(TEXT("UnitMotion-Impact.png"));
        MotionWait(.55f);
        TestTrue(TEXT("反応終了後は攻撃側・被弾側とも横移動なし"),FMath::IsNearlyZero(Translation().X) && FMath::IsNearlyZero(Translation(TEXT("battle.unit.art.100")).X));
        TestTrue(TEXT("画像の反応で確定状態と乱数を変更しない"),battle->Checkpoint()==expected.Checkpoint() && Screen->ControlCheckpoint()==control);
    });
    It(TEXT("敵の確定通常攻撃で敵画像が左へ動き、状態はAIの結果と一致する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(90),[this]{
        using namespace GirlsSpire;
        BattleUnit hero;hero.At={1,1};hero.Speed=99;hero.Hp=hero.MaxHp=100;
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={2,1};enemy.Speed=0;enemy.Accuracy=200;
        auto battle=MakeShared<Battle>(8,8,std::vector<Tile>(64),std::vector<BattleUnit>{hero,enemy},42);
        Async(EAsyncExecution::TaskGraphMainThread,[this,battle]{Window->SetContent(SAssignNew(Screen,SCombatScreen).Combat(battle));}).Wait();
        DrainSlateLayout();
        auto expected=*battle;
        if(!TestTrue(TEXT("主人公の待機から敵手番を準備"),expected.EndTurn(expected.Snapshot().Revision,0,Facing::South)))return;
        const auto attack=ChooseBasicAction(expected);
        if(!TestTrue(TEXT("隣接する左の主人公を通常攻撃"),attack.Kind==BasicActionKind::Attack && attack.Target==Position{1,1} && ApplyBasicAction(expected,attack)))return;
        if(!TestTrue(TEXT("攻撃後に敵手番を終了"),ApplyBasicAction(expected,ChooseBasicAction(expected)) && expected.Snapshot().ActiveId==0))return;
        if(!TestTrue(TEXT("主人公の待機を実クリック"),ClickAndDrain(Driver->FindElement(By::Id("battle.wait.south")))))return;
        MotionWait(.05f);
        TestTrue(TEXT("敵の駒画像が対象方向の左へ動く"),Translation(TEXT("battle.unit.art.100")).X<-.1);
        MotionWait(.55f);
        TestTrue(TEXT("敵の画像が確定位置へ戻る"),FMath::IsNearlyZero(Translation(TEXT("battle.unit.art.100")).X));
        TestTrue(TEXT("敵の表示で確定結果と乱数を変更しない"),battle->Checkpoint()==expected.Checkpoint());
    });
    const TCHAR* abilityCases[]={TEXT("攻撃技の確定で駒が対象へ動く"),TEXT("個人必殺の確定で駒が対象へ動く"),TEXT("全員必殺で発動者が対象へ動く"),TEXT("味方への回復で攻撃方向へ動かない")};
    for(int mode=0;mode<4;++mode)It(abilityCases[mode],EAsyncExecution::ThreadPool,FTimespan::FromSeconds(90),[this,mode]{
        const bool team=mode==2, heal=mode==3;
        using namespace GirlsSpire;
        BattleUnit hero;hero.At={1,1};hero.Attack=20;hero.Accuracy=200;hero.Speed=20;hero.Hp=hero.MaxHp=100;hero.Skills={true,true};
        auto enemy=hero;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={2,1};enemy.Speed=10;enemy.Hp=enemy.MaxHp=200;
        std::vector<BattleUnit> units{hero,enemy};
        if(heal) {
            units[1].At={4,1};
            BattleUnit ally;ally.Id=1;ally.At={2,1};ally.Hp=10;ally.MaxHp=40;ally.Speed=1;
            units.push_back(ally);
        }
        auto initial=team?Test::TeamUltimateBattle(0,100):Battle(8,8,std::vector<Tile>(64),units,42);
        auto checkpoint=initial.Checkpoint();
        if(!team && !heal){checkpoint.State.AbilityRules=AbilityRuleSet::Character;checkpoint.State.Combos.Gauge=50;}
        const auto restored=Battle::Restore(checkpoint);
        if(!TestTrue(TEXT("技と必殺を使える戦闘"),restored.has_value()))return;
        auto battle=MakeShared<Battle>(*restored);
        Async(EAsyncExecution::TaskGraphMainThread,[this,battle]{Window->SetContent(SAssignNew(Screen,SCombatScreen).Combat(battle).BondProfile(Profile{}));}).Wait();
        DrainSlateLayout();
        const auto click=[&](const char* id){
            auto target=Driver->FindElement(By::Id(id));
            if(!target->IsVisible()){
                auto scroll=Driver->FindElement(By::Id("battle.scroll"));
                scroll->ScrollToBeginning();DrainSlateLayout();
                if(!target->IsVisible()){scroll->ScrollToEndUntil(By::Id(id));DrainSlateLayout();}
            }
            return TestTrue(UTF8_TO_TCHAR(id),ClickAndDrain(target));
        };
        auto expected=*battle;
        const auto revision=expected.Snapshot().Revision;
        const Position target=team?Position{6,1}:Position{2,1};
        const auto skill=heal?Skill::FirstAid:Skill::PowerStrike;
        const bool applied=mode==1?expected.UseUltimate(revision,0,target):team?expected.UseTeamUltimate(revision,0,target):expected.UseSkill(revision,0,skill,target);
        if(!TestTrue(TEXT("同じ技または必殺を準備"),applied))return;
        const char* selectIds[]={"battle.skill.0","battle.ultimate.select","battle.teamultimate.select","battle.skill.1"};
        const char* confirmIds[]={"battle.skill.confirm","battle.ultimate.confirm","battle.teamultimate.confirm","battle.skill.confirm"};
        if(!click(selectIds[mode]) || !click(team?"battle.cell.6.1":"battle.cell.2.1"))return;
        TestTrue(TEXT("予告段階で画像と戦闘はそのまま"),FMath::IsNearlyZero(Translation().X) && battle->Checkpoint()==checkpoint);
        if(!click(confirmIds[mode]))return;
        MotionWait(.05f);
        const auto shift=Translation().X;
        TestTrue(TEXT("確定した攻撃技・必殺は対象方向へ動き、回復では静止"),heal?FMath::IsNearlyZero(shift):shift>.1);
        MotionWait(.55f);
        TestTrue(TEXT("演出終了後の静止と確定状態・乱数の一致"),FMath::IsNearlyZero(Translation().X) && battle->Checkpoint()==expected.Checkpoint());
    });
    AfterEach([this]{Driver.Reset();Screen.Reset();if(Window){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}IAutomationDriverModule::Get().Disable();});
}
