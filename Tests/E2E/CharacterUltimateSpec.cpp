#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/CombatScreen.h"
#include "Domain/AbilityCatalog.h"
BEGIN_DEFINE_SPEC(FGirlsSpireCharacterUltimateSpec,"GirlsSpire.UI.CharacterUltimates",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
END_DEFINE_SPEC(FGirlsSpireCharacterUltimateSpec)
void FGirlsSpireCharacterUltimateSpec::Define()
{
    BeforeEach([this]{FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();Window=SNew(SWindow).ClientSize(FVector2D(1200,1000));FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();});
    LatentBeforeEach(WaitForSlateLayout);
    for(int id=0;id<9;++id)for(int rank:{0,100})It(*FString::Printf(TEXT("人物%dの絆%d必殺を予告・取消・確定・保存する"),id,rank),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(90),[this,id,rank]{
        using namespace GirlsSpire;const auto ability=*FormalUltimate(id,rank);
        BattleUnit actor;actor.Id=id;actor.At={3,3};actor.Hp=60;actor.MaxHp=100;actor.Attack=20;actor.Speed=30;actor.Accuracy=200;actor.Range=3;actor.Skills={true,true};
        auto ally=actor;ally.Id=id?0:1;ally.At={3,4};ally.Hp=30;ally.Speed=20;ally.AttackCursed=true;ally.MovementWebbed=true;
        auto enemy=actor;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={4,3};enemy.Speed=10;enemy.Hp=enemy.MaxHp=300;enemy.Defense=10;
        auto second=enemy;second.Id=101;second.At={5,3};auto third=enemy;third.Id=102;third.At={4,4};
        Battle arena(8,8,std::vector<Tile>(64),{actor,ally,enemy,second,third},42);auto checkpoint=arena.Checkpoint();checkpoint.State.AbilityRules=AbilityRuleSet::CharacterAbilities;checkpoint.State.StartingBonds.fill(rank);checkpoint.State.Combos.Gauge=50;
        const auto restored=Battle::Restore(checkpoint);if(!TestTrue(TEXT("正式必殺状態を復元"),restored.has_value()))return;
        auto combat=MakeShared<Battle>(*restored);auto expected=*combat;CombatControlCheckpoint control;control.ProfileDriven=true;control.ChoiceRandomState=42;for(int i=1;i<=8;++i)control.Trust[i]=rank;
        const auto saved=std::make_shared<std::optional<SavedCombat>>();
        Async(EAsyncExecution::TaskGraphMainThread,[this,combat,control,saved]{Window->SetContent(SNew(SCombatScreen).Combat(combat).SavedControl(control).OnCheckpoint([saved](const SavedCombat& value){*saved=value;return true;}));}).Wait();DrainSlateLayout();
        const auto click=[&](const char* key){const auto target=Driver->FindElement(By::Id(key));if(!target->IsVisible()){const auto scroll=Driver->FindElement(By::Id("battle.scroll"));scroll->ScrollToBeginning();DrainSlateLayout();if(!target->IsVisible()){scroll->ScrollToEndUntil(By::Id(key));DrainSlateLayout();}}return TestTrue(UTF8_TO_TCHAR(key),ClickAndDrain(target));};
        TestTrue(TEXT("正式な必殺名を表示"),Driver->FindElement(By::Id("battle.ultimate.select"))->GetText().ToString().Contains(UTF8_TO_TCHAR(ability.Name.data())));
        const auto before=combat->Checkpoint();if(!click("battle.ultimate.select"))return;
        const Position at=ability.Area==AbilityArea::SelfRadius || ability.Area==AbilityArea::AdjacentFour?actor.At:enemy.At;
        const auto cell=FString::Printf(TEXT("battle.cell.%d.%d"),at.X,at.Y);if(!click(TCHAR_TO_UTF8(*cell)))return;
        TestTrue(TEXT("有効予告/説明で状態とRNG不変"),combat->PreviewUltimate(id,at).Legal && combat->Checkpoint()==before && Driver->FindElement(By::Id("battle.ultimate.preview"))->GetText().ToString().Contains(TEXT("ゲージ50消費→0")));
        if(!click("battle.ultimate.cancel"))return;TestTrue(TEXT("取消は無消費"),combat->Checkpoint()==before);
        if(!click("battle.ultimate.select") || !click(TCHAR_TO_UTF8(*cell)))return;
        expected.UseUltimate(expected.Snapshot().Revision,id,at);if(!click("battle.ultimate.confirm"))return;
        TestTrue(TEXT("回復/範囲/弱体/強化/結/ゲージ/乱数を保存"),combat->Checkpoint()==expected.Checkpoint() && saved->has_value() && saved->value().Battle==expected.Checkpoint());
        Run run(42,Difficulty::Beginner,HeroRole::Sword);auto progress=run.Checkpoint();progress.State.Day=5;progress.State.Phase=RunPhase::Event;progress.State.Active={NodeKind::Battle};progress.State.Choices.clear();progress.State.Recruited={id?id-1:0};
        if(saved->has_value()){const RunSaveState state{progress,saved->value()};const auto bytes=EncodeRunPayload(state);TestTrue(TEXT("正式必殺後をv15で全状態往復"),bytes && DecodeRunPayload(*bytes)==state);}
    });
    AfterEach([this]{Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}IAutomationDriverModule::Get().Disable();});
}
