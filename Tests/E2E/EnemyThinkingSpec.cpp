#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/CombatScreen.h"
#include "../Fixtures/EnemyThinkingCase.h"
BEGIN_DEFINE_SPEC(FGirlsSpireEnemyThinkingSpec,"GirlsSpire.UI.EnemyThinking",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
END_DEFINE_SPEC(FGirlsSpireEnemyThinkingSpec)
void FGirlsSpireEnemyThinkingSpec::Define()
{
    BeforeEach([this]{FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();Window=SNew(SWindow).ClientSize(FVector2D(1200,1000));FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();});
    LatentBeforeEach(WaitForSlateLayout);
    for(int value=0;value<4;++value)for(bool pending:{false,true})It(*FString::Printf(TEXT("敵思考%d・連携履歴%dを実行し保存"),value,pending),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this,value,pending]{
        using namespace GirlsSpire;const auto mode=static_cast<EnemyThinking>(value);const auto state=TestData::EnemyThinkingCase(mode,pending);
        const auto restored=Battle::Restore(state.Combat->Battle);if(!TestTrue(TEXT("難易度状態を復元"),restored.has_value()))return;
        auto combat=MakeShared<Battle>(*restored);auto expected=*restored;const auto control=state.Combat->Control;const auto saved=std::make_shared<std::optional<SavedCombat>>();
        Async(EAsyncExecution::TaskGraphMainThread,[this,combat,control,saved]{Window->SetContent(SNew(SCombatScreen).Combat(combat).SavedControl(control).OnCheckpoint([saved](const SavedCombat& value){*saved=value;return true;}));}).Wait();DrainSlateLayout();
        TestTrue(TEXT("表示で状態/RNG不変"),combat->Checkpoint()==state.Combat->Battle);
        expected.EndTurn(expected.Snapshot().Revision,0,Facing::South);
        const auto action=ChooseBasicAction(expected);const bool preferNear=mode==EnemyThinking::Basic || mode==EnemyThinking::Tactical || (mode==EnemyThinking::Disrupt && pending);
        TestTrue(TEXT("承認済み条件の標的"),action.Kind==BasicActionKind::Attack && action.Target==(preferNear?Position{3,1}:Position{1,4}));
        for(int step=0;step<8 && expected.Snapshot().ActiveId==100;++step)if(!ApplyBasicAction(expected,ChooseBasicAction(expected)))break;
        const auto wait=Driver->FindElement(By::Id("battle.wait.south"));if(!wait->IsVisible()){Driver->FindElement(By::Id("battle.scroll"))->ScrollToEndUntil(By::Id("battle.wait.south"));DrainSlateLayout();}
        if(!TestTrue(TEXT("待機から敵の手番を進める"),ClickAndDrain(wait)))return;
        TestTrue(TEXT("実行結果・RNG・難易度識別を保存"),combat->Checkpoint()==expected.Checkpoint() && saved->has_value() && saved->value().Battle==expected.Checkpoint());
        if(saved->has_value()){auto result=state;result.Combat=saved->value();const auto bytes=EncodeRunPayload(result);TestTrue(TEXT("Run v18で全状態往復"),bytes && DecodeRunPayload(*bytes)==result);}
    });
    It(TEXT("上級は致死罠を避け射線を得て攻撃し保存する"),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this]{
        using namespace GirlsSpire;auto state=TestData::EnemyThinkingCase(EnemyThinking::Tactical);
        auto& checkpoint=state.Combat->Battle;checkpoint.State.Units[0].Hp=5;checkpoint.State.Units[1].Hp=100;checkpoint.State.Units[1].At={5,1};checkpoint.State.Units[2].At={6,6};checkpoint.State.Tiles[2+8].Trap=true;
        const auto restored=Battle::Restore(checkpoint);if(!TestTrue(TEXT("上級移動の状態を復元"),restored.has_value()))return;
        auto combat=MakeShared<Battle>(*restored);auto expected=*restored;const auto control=state.Combat->Control;const auto saved=std::make_shared<std::optional<SavedCombat>>();
        Async(EAsyncExecution::TaskGraphMainThread,[this,combat,control,saved]{Window->SetContent(SNew(SCombatScreen).Combat(combat).SavedControl(control).OnCheckpoint([saved](const SavedCombat& value){*saved=value;return true;}));}).Wait();DrainSlateLayout();
        expected.EndTurn(expected.Snapshot().Revision,0,Facing::South);
        TestTrue(TEXT("射線と安全な経路から移動を選ぶ"),ChooseBasicAction(expected)==BasicAction{BasicActionKind::Move,{3,0}});
        for(int step=0;step<8 && expected.Snapshot().ActiveId==100;++step)if(!ApplyBasicAction(expected,ChooseBasicAction(expected)))break;
        const auto wait=Driver->FindElement(By::Id("battle.wait.south"));if(!wait->IsVisible()){Driver->FindElement(By::Id("battle.scroll"))->ScrollToEndUntil(By::Id("battle.wait.south"));DrainSlateLayout();}
        if(!TestTrue(TEXT("マウスで敵の手番へ"),ClickAndDrain(wait)))return;
        TestTrue(TEXT("移動・攻撃・終了とRNGを同じ結果で保存"),combat->Checkpoint()==expected.Checkpoint() && saved->has_value() && saved->value().Battle==expected.Checkpoint());
        TestTrue(TEXT("敵は罠を踏まず射線を得る"),combat->Snapshot().Units[0].At==Position{3,0} && combat->Snapshot().Units[0].Hp==5 && combat->Snapshot().Tiles[2+8].Trap);
    });
    AfterEach([this]{Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}IAutomationDriverModule::Get().Disable();});
}
