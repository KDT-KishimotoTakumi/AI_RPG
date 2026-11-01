#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/CombatScreen.h"
#include "../Fixtures/ComboCatalogCases.h"
BEGIN_DEFINE_SPEC(FGirlsSpireComboCatalogSpec,"GirlsSpire.UI.ComboCatalog",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
    bool Click(const FString& id);
END_DEFINE_SPEC(FGirlsSpireComboCatalogSpec)
bool FGirlsSpireComboCatalogSpec::Click(const FString& id)
{
    auto target=Driver->FindElement(By::Id(*id));
    if(!target->IsVisible()) {
        const auto scroll=Driver->FindElement(By::Id("battle.scroll"));scroll->ScrollToBeginning();DrainSlateLayout();
        if(!target->IsVisible()){scroll->ScrollToEndUntil(By::Id(*id));DrainSlateLayout();}
    }
    return ClickAndDrain(target);
}
void FGirlsSpireComboCatalogSpec::Define()
{
    BeforeEach([this] {
        FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();
        Window=SNew(SWindow).Title(FText::FromString(TEXT("Girls Spire — 20連携テスト"))).ClientSize(FVector2D(1100,760));
        FSlateApplication::Get().AddWindow(Window.ToSharedRef());Driver=IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    auto fixtures=GirlsSpire::TestData::ComboCatalogCases();
    for(const auto& fixture:fixtures)for(bool kill:{false,true}) {
        if(kill && fixture.Id!=103 && fixture.Id!=105)continue;
        It(*FString::Printf(TEXT("連携%d%sを実操作で成立させる"),fixture.Id,kill?TEXT("撃破対象"):TEXT("")),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this,fixture,kill] {
            using namespace GirlsSpire;
            std::vector<BattleUnit> units;
            for(std::size_t i=0;i<fixture.Players.size();++i) {
                BattleUnit unit;unit.Id=static_cast<int>(i);unit.At=fixture.Players[i];unit.Speed=50-static_cast<int>(i);unit.Range=12;unit.Accuracy=200;unit.Hp=unit.MaxHp=200;
                if(fixture.Targets[i].empty() && fixture.Tags[i]!=Opening){unit.Skills[1]=true;unit.Hp=10;}
                else if(fixture.Tags[i]&Twist)unit.Skills[0]=true;
                units.push_back(unit);
            }
            for(std::size_t i=0;i<fixture.Enemies.size();++i) {
                BattleUnit enemy;enemy.Id=100+static_cast<int>(i);enemy.Side=Faction::Enemy;enemy.At=fixture.Enemies[i];enemy.Hp=enemy.MaxHp=5000;enemy.Attack=0;enemy.Movement=0;enemy.Speed=0;
                if(kill)enemy.Hp=fixture.Id==103?19:11;
                units.push_back(enemy);
            }
            auto battle=MakeShared<Battle>(8,8,std::vector<Tile>(64),units,TestData::ComboWinningSeed());
            Async(EAsyncExecution::TaskGraphMainThread,[this,battle]{Window->SetContent(SNew(SCombatScreen).Combat(battle));}).Wait();DrainSlateLayout();
            for(std::size_t i=0;i<fixture.Tags.size();++i) {
                if(!TestEqual(TEXT("予定した人物の手番"),battle->Snapshot().ActiveId,static_cast<int>(i)))return;
                if(fixture.Tags[i]==Opening) {if(!Click(TEXT("battle.wait.south")))return;}
                else if(i==0) {
                    const int target=fixture.Targets[i][0];
                    if(fixture.Tags[i]&Twist) {
                        if(!Click(TEXT("battle.skill.0")))return;
                        const auto at=fixture.Enemies[target-100];if(!Click(FString::Printf(TEXT("battle.cell.%d.%d"),at.X,at.Y)) || !Click(TEXT("battle.skill.confirm")))return;
                    } else if(!Click(FString::Printf(TEXT("battle.target.%d"),target)) || !Click(TEXT("battle.confirm")))return;
                } else {
                    if(!Click(TEXT("intent.trust.75")))return;
                    if(fixture.Targets[i].empty()) {if(!Click(TEXT("intent.kind.2")))return;}
                    else {
                        if(!Click(TEXT("intent.kind.0")))return;
                        const auto at=fixture.Enemies[fixture.Targets[i][0]-100];if(!Click(FString::Printf(TEXT("battle.cell.%d.%d"),at.X,at.Y)))return;
                    }
                    if(!Click(TEXT("intent.execute")))return;
                }
                if(i+1<fixture.Tags.size()) {
                    if(!TestEqual(TEXT("途中で短い連携を発動しない"),battle->Snapshot().Combos.Gauge,0))return;
                    if(fixture.Tags[i]!=Opening && !Click(TEXT("battle.wait.south")))return;
                }
            }
            const int gauge=fixture.Tags.size()==2?10:fixture.Tags.size()==3?20:40;
            TestTrue(TEXT("承認IDと人数別ゲージ"),battle->Snapshot().LastCombo && battle->Snapshot().LastCombo->RecipeId==fixture.Id && battle->Snapshot().Combos.Gauge==gauge);
            const auto label=Driver->FindElement(By::Id("battle.combo.result"))->GetText().ToString();
            const auto syntax=TestData::ComboExpectedSyntax(fixture);
            TestTrue(TEXT("承認済み名称・構文・参加者・成立理由・初発見"),label.Contains(UTF8_TO_TCHAR(TestData::ComboExpectedName(fixture.Id))) && label.Contains(UTF8_TO_TCHAR(syntax.c_str())) && label.Contains(TEXT("参加者")) && label.Contains(TEXT("成立理由")) && label.Contains(TEXT("初発見")));
            if(kill)TestTrue(TEXT("撃破した敵の位置も成立判定へ使う"),battle->Snapshot().Outcome==BattleOutcome::Won && battle->Snapshot().Units.back().Hp==0);
            const auto restored=Battle::Restore(battle->Checkpoint());TestTrue(TEXT("20種の実操作結果を復元"),restored && restored->Checkpoint()==battle->Checkpoint());
        });
    }
    AfterEach([this] {
        Driver.Reset();if(Window.IsValid()){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}
        IAutomationDriverModule::Get().Disable();
    });
}
