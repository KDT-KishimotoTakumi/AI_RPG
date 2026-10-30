#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Domain/Rules.h"
#include "Domain/Combo.h"
#include "Domain/Random.h"
#include "Application/Run.h"
#include "Application/Encounter.h"
#include "AI/BasicTurn.h"
#include "AI/Intent.h"
#if defined(GIRLS_SPIRE_COVERAGE)
extern "C" int __llvm_profile_write_file();
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGirlsSpireRulesSmoke, "GirlsSpire.Foundation.Rules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGirlsSpireRulesSmoke::RunTest(const FString& Parameters)
{
    using namespace GirlsSpire;
    ComboLedger combo;
    TestTrue(TEXT("連携の先行行動"),combo.Observe({1,1,0}));
    TestTrue(TEXT("連携間の敵行動"),combo.Observe({2,1,100,Faction::Enemy}));
    TestTrue(TEXT("連携の後続行動"),combo.Observe({3,1,1}));
    const auto combination=combo.Resolve(3,{{1,{1,3}}});
    TestTrue(TEXT("連携成立と参加者"),combination && *combination==ComboResolution{1,{0,1},10});
    TestEqual(TEXT("連携ゲージ加算"),combo.Snapshot().Gauge,10);
    TestFalse(TEXT("連携二重適用拒否"),combo.Resolve(3,{{1,{1,3}}}).has_value());
    TestEqual(TEXT("命中下限"), ClampHitChance(-100), 5);
    TestEqual(TEXT("命中上限"), ClampHitChance(120), 95);
    TestTrue(TEXT("信頼75で確定予告"), GetPrediction(75).Certain);
    TestTrue(TEXT("絆100で専用技解放"), GetPrediction(100).SpecialUnlocked);
    int gauge = AddComboGauge(60, 5);
    TestTrue(TEXT("全員必殺発動"), SpendUltimate(gauge, true));
    TestEqual(TEXT("消費後ゲージ"), gauge, 0);
    Random random(42);
    random.Next();
    Random restored(random.State());
    TestTrue(TEXT("状態復元後の乱数一致"), random.Next() == restored.Next());
    Random bounded(42);
    TestEqual(TEXT("範囲付き乱数の固定値"), bounded.Uniform(100), std::uint32_t{13});
    Run run(42, Difficulty::Beginner, HeroRole::Sword);
    Run repeat(42, Difficulty::Beginner, HeroRole::Sword);
    TestTrue(TEXT("同一シードの初期経路"), run.Snapshot() == repeat.Snapshot());
    TestTrue(TEXT("経路生成後の乱数状態"), run.RandomState() == repeat.RandomState());
    repeat.ChooseNode(repeat.Snapshot().Revision, 0);
    GenerationResult terrain, repeatedTerrain;
    auto combat = Encounter::Create(repeat.Snapshot(), 42, &terrain);
    auto replay = Encounter::Create(repeat.Snapshot(), 42, &repeatedTerrain);
    TestTrue(TEXT("生成・再試行状態の再現"),terrain==repeatedTerrain);
    TestTrue(TEXT("戦場の再現"), combat.Snapshot() == replay.Snapshot());
    TestTrue(TEXT("AI候補の再現"), ChooseBasicAction(combat) == ChooseBasicAction(replay));
    combat.Move(0, 0, {3,1}); replay.Move(0, 0, {3,1});
    TestTrue(TEXT("命中予告の再現"), combat.PreviewAttack(0,{4,1}) == replay.PreviewAttack(0,{4,1}));
    combat.Attack(1,0,{4,1}); replay.Attack(1,0,{4,1});
    TestTrue(TEXT("攻撃結果の再現"), combat.LastHits() == replay.LastHits());
    TestTrue(TEXT("戦闘乱数の再現"), combat.RandomState() == replay.RandomState());
    const auto forecast=IntentForecast::Build(combat,RankIntentActions(combat,{}),25);
    const auto repeatForecast=IntentForecast::Build(replay,RankIntentActions(replay,{}),25);
    TestTrue(TEXT("仲間の予告データ再現"),forecast.Candidates()==repeatForecast.Candidates());
    Random choice(42);
    TestTrue(TEXT("予告からの実行"),forecast.Execute(combat,choice));
    for (int step = 0; step < 24 && run.Snapshot().Phase == RunPhase::Route; ++step)
    {
        TestTrue(TEXT("経路を選択"), run.ChooseNode(run.Snapshot().Revision, 0));
        TestTrue(TEXT("イベントを完了"), run.CompleteEvent(run.Snapshot().Revision));
    }
    TestTrue(TEXT("12日間の周回勝利"), run.Snapshot().Phase == RunPhase::Won);
    TestEqual(TEXT("最終日"), run.Snapshot().Day, 12);
    TestEqual(TEXT("救出済み4人"), static_cast<int>(run.Snapshot().Recruited.size()), 4);
    TestEqual(TEXT("未加入4人"), static_cast<int>(run.Unrecruited().size()), 4);
#if defined(GIRLS_SPIRE_COVERAGE)
    // Automationの強制終了では通常の終了ハンドラーが呼ばれないため、明示的に記録する。
    TestEqual(TEXT("カバレッジ記録"), __llvm_profile_write_file(), 0);
#endif
    return true;
}
#endif
