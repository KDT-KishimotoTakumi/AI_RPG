#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/CombatScreen.h"
#include "Application/Encounter.h"
BEGIN_DEFINE_SPEC(FGirlsSpireHeroAbilitySpec,"GirlsSpire.UI.HeroAbilities",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
END_DEFINE_SPEC(FGirlsSpireHeroAbilitySpec)
void FGirlsSpireHeroAbilitySpec::Define()
{
    BeforeEach([this] {
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        Window=SNew(SWindow).Title(FText::FromString(TEXT("Girls Spire — 主人公技能テスト"))).ClientSize(FVector2D(1200,1000));
        FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    for(int role=0;role<4;++role)for(int mode=0;mode<3;++mode)It(*FString::Printf(TEXT("武器%dの正式技能%dをマウスで予告・実行する"),role,mode),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(90),[this,role,mode] {
        using namespace GirlsSpire;
        RunSnapshot run;run.Phase=RunPhase::Event;run.Day=5;run.Role=static_cast<HeroRole>(role);run.Active={NodeKind::Battle};
        auto hero=Encounter::Create(run,42).Snapshot().Units.front();hero.At={1,1};hero.Hp=30;
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={2,1};enemy.Hp=enemy.MaxHp=200;enemy.Defense=2;
        Battle initial(8,8,std::vector<Tile>(64),{hero,enemy},42);auto checkpoint=initial.Checkpoint();checkpoint.State.Combos.Gauge=50;
        auto restored=Battle::Restore(checkpoint);if(!TestTrue(TEXT("ゲージ50の通常戦闘を用意"),restored.has_value()))return;
        auto combat=MakeShared<Battle>(*restored);auto expected=*combat;
        Async(EAsyncExecution::TaskGraphMainThread,[this,combat]{Window->SetContent(SNew(SCombatScreen).Combat(combat));}).Wait();DrainSlateLayout();
        if(!TestTrue(TEXT("主人公の正式2技能名を表示"),Driver->FindElement(By::Id("battle.skill.0"))->GetText().ToString().Contains(TEXT("鍛冶の一撃")) && Driver->FindElement(By::Id("battle.skill.1"))->GetText().ToString().Contains(TEXT("野戦の手当"))))return;
        if(!TestTrue(TEXT("主人公の正式必殺名を表示"),Driver->FindElement(By::Id("battle.ultimate.select"))->GetText().ToString().Contains(TEXT("火打ちの誓い"))))return;
        const auto click=[&](const char* id) {
            const auto target=Driver->FindElement(By::Id(id));
            if(!target->IsVisible()){const auto scroll=Driver->FindElement(By::Id("battle.scroll"));scroll->ScrollToBeginning();DrainSlateLayout();if(!target->IsVisible()){scroll->ScrollToEndUntil(By::Id(id));DrainSlateLayout();}}
            return TestTrue(*FString::Printf(TEXT("マウス操作 %hs"),id),ClickAndDrain(target));
        };
        const auto at=mode==1?hero.At:enemy.At;
        const auto before=combat->Checkpoint();
        if(!click(mode==2?"battle.ultimate.select":mode==1?"battle.skill.1":"battle.skill.0"))return;
        const auto cell=FString::Printf(TEXT("battle.cell.%d.%d"),at.X,at.Y);if(!click(TCHAR_TO_UTF8(*cell)))return;
        if(mode==2)expected.UseUltimate(expected.Snapshot().Revision,0,at);else expected.UseSkill(expected.Snapshot().Revision,0,mode==1?Skill::FirstAid:Skill::PowerStrike,at);
        TestTrue(TEXT("予告で状態や乱数を変えない"),combat->Checkpoint()==before);
        if(!click(mode==2?"battle.ultimate.confirm":"battle.skill.confirm"))return;
        TestTrue(TEXT("既存の承認効果・再使用・ゲージ・乱数・保存状態と一致"),combat->Checkpoint()==expected.Checkpoint());
        if(mode<2)TestTrue(TEXT("正式名とクールダウン2を表示"),Driver->FindElement(By::Id("battle.skill.status"))->GetText().ToString().Contains(mode==1?TEXT("野戦の手当:2"):TEXT("鍛冶の一撃:2")));
    });
    AfterEach([this] {
        Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}
        IAutomationDriverModule::Get().Disable();
    });
}
