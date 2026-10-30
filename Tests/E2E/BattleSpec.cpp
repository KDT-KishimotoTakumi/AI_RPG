#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Widgets/SWindow.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Presentation/CombatScreen.h"

BEGIN_DEFINE_SPEC(FGirlsSpireBattleUiSpec, "GirlsSpire.UI.Battle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FAutomationDriverPtr Driver;
    TSharedPtr<SWindow> Window;
    int32 CaptureIndex = 0;
END_DEFINE_SPEC(FGirlsSpireBattleUiSpec)

void FGirlsSpireBattleUiSpec::Define()
{
    BeforeEach([this] {
        FSlateApplication::Get().CloseToolTip();
        // 新しいウィンドウの入力も最初から同じ模擬入力経路へ渡す。
        IAutomationDriverModule::Get().Enable();
        IConsoleManager::Get().ProcessUserConsoleInput(TEXT("GirlsSpire.Preview"), *GLog, nullptr);
        for (const auto& Candidate : FSlateApplication::Get().GetTopLevelWindows())
            if (Candidate->GetTitle().ToString() == TEXT("Girls Spire — 周回プレビュー")) Window = Candidate;
        Driver = IAutomationDriverModule::Get().CreateDriver();
    });
    LatentBeforeEach(WaitForSlateLayout);
    It(TEXT("経路から救出戦へ入り移動・予告・攻撃で勝利して加入する"), EAsyncExecution::ThreadPool,
        FTimespan::FromSeconds(180), [this] {
        if (!TestTrue(TEXT("開始"),ClickAndDrain(Driver->FindElement(By::Id("title.start"))))) return;
        if (!TestTrue(TEXT("出発"),ClickAndDrain(Driver->FindElement(By::Id("role.confirm"))))) return;
        if (!TestTrue(TEXT("救出経路"),ClickAndDrain(Driver->FindElement(By::Id("route.node.0"))))) return;
        if (!TestTrue(TEXT("戦闘へ"),ClickAndDrain(Driver->FindElement(By::Id("preview.battle"))))) return;
        if (!TestTrue(TEXT("戦場のマスをマウスで選び移動"),ClickAndDrain(Driver->FindElement(By::Id("battle.cell.3.1"))))) return;
        if (!TestTrue(TEXT("移動予告を確定"),ClickAndDrain(Driver->FindElement(By::Id("battle.move.confirm"))))) return;
        for (int Turn=0;Turn<12;++Turn) {
            if (Driver->FindElement(By::Id("battle.won"))->Exists()) break;
            if (!TestTrue(TEXT("敵を選んで命中予告"),ClickAndDrain(Driver->FindElement(By::Id("battle.target.100"))))) return;
            TestTrue(TEXT("予告に命中率を表示"),Driver->FindElement(By::Id("battle.preview"))->GetText().ToString().Contains(TEXT("%")));
            if (!TestTrue(TEXT("攻撃確定"),ClickAndDrain(Driver->FindElement(By::Id("battle.confirm"))))) return;
            if (Driver->FindElement(By::Id("battle.won"))->Exists()) break;
            if (!TestTrue(TEXT("待機して敵の手番へ"),ClickAndDrain(Driver->FindElement(By::Id("battle.wait.south"))))) return;
        }
        TestTrue(TEXT("実際の敵全滅で勝利"),Driver->FindElement(By::Id("battle.won"))->Exists());
        TestTrue(TEXT("戦闘結果を周回に反映"),ClickAndDrain(Driver->FindElement(By::Id("battle.return"))));
        TestTrue(TEXT("救出で加入"),Driver->FindElement(By::Id("run.party"))->GetText().ToString().Contains(TEXT("仲間1人")));
    });
    It(TEXT("移動不可・射程外を表示し待機だけなら実戦で敗北する"), EAsyncExecution::ThreadPool,
        FTimespan::FromSeconds(180), [this] {
        if (!TestTrue(TEXT("開始"),ClickAndDrain(Driver->FindElement(By::Id("title.start"))))) return;
        if (!TestTrue(TEXT("出発"),ClickAndDrain(Driver->FindElement(By::Id("role.confirm"))))) return;
        if (!TestTrue(TEXT("救出経路"),ClickAndDrain(Driver->FindElement(By::Id("route.node.0"))))) return;
        if (!TestTrue(TEXT("戦闘へ"),ClickAndDrain(Driver->FindElement(By::Id("preview.battle"))))) return;
        TestTrue(TEXT("遠すぎるマスを選択"),ClickAndDrain(Driver->FindElement(By::Id("battle.cell.3.3"))));
        TestTrue(TEXT("移動不可の説明"),Driver->FindElement(By::Id("battle.log"))->GetText().ToString().Contains(TEXT("移動できません")));
        TestTrue(TEXT("射程外の敵"),ClickAndDrain(Driver->FindElement(By::Id("battle.cell.4.1"))));
        TestTrue(TEXT("射程外の説明"),Driver->FindElement(By::Id("battle.preview"))->GetText().ToString().Contains(TEXT("攻撃できません")));
        for (int Turn=0;Turn<40;++Turn) {
            if (Driver->FindElement(By::Id("battle.lost"))->Exists()) break;
            const char* FacingIds[]={"battle.wait.north","battle.wait.east","battle.wait.south","battle.wait.west"};
            if (!TestTrue(TEXT("向きを選び待機"),ClickAndDrain(Driver->FindElement(By::Id(FacingIds[Turn%4]))))) return;
        }
        TestTrue(TEXT("敵の攻撃で全員戦闘不能"),Driver->FindElement(By::Id("battle.lost"))->Exists());
        TestTrue(TEXT("周回へ結果反映"),ClickAndDrain(Driver->FindElement(By::Id("battle.return"))));
        TestTrue(TEXT("周回敗北"),Driver->FindElement(By::Id("result.lost"))->Exists());
    });
    It(TEXT("攻撃確定前の戦場と予告を表示する"), EAsyncExecution::ThreadPool,
        FTimespan::FromSeconds(120), [this] {
        if (!TestTrue(TEXT("開始"),ClickAndDrain(Driver->FindElement(By::Id("title.start"))))) return;
        if (!TestTrue(TEXT("出発"),ClickAndDrain(Driver->FindElement(By::Id("role.confirm"))))) return;
        if (!TestTrue(TEXT("救出経路"),ClickAndDrain(Driver->FindElement(By::Id("route.node.0"))))) return;
        if (!TestTrue(TEXT("戦闘へ"),ClickAndDrain(Driver->FindElement(By::Id("preview.battle"))))) return;
        bool TerrainShown=false;
        for(int y=0;y<8 && !TerrainShown;++y) for(int x=0;x<8 && !TerrainShown;++x) {
            const auto Cell=Driver->FindElement(By::Id(*FString::Printf(TEXT("battle.celllabel.%d.%d"),x,y)))->GetText().ToString();
            TerrainShown=Cell.Contains(TEXT("高")) || Cell.Contains(TEXT("遮"));
        }
        TestTrue(TEXT("生成された高低差・遮蔽を文字で識別"),TerrainShown);
        TestTrue(TEXT("移動"),ClickAndDrain(Driver->FindElement(By::Id("battle.cell.3.1"))));
        TestTrue(TEXT("移動確定"),ClickAndDrain(Driver->FindElement(By::Id("battle.move.confirm"))));
        TestTrue(TEXT("敵をマスで選択"),ClickAndDrain(Driver->FindElement(By::Id("battle.cell.4.1"))));
        TestTrue(TEXT("確定操作を画面内に表示"),Driver->FindElement(By::Id("battle.confirm"))->IsVisible());
        TestTrue(TEXT("待機操作を画面内に表示"),Driver->FindElement(By::Id("battle.wait.west"))->IsVisible());
    });
    It(TEXT("仲間の4作戦と絆由来の低信頼予告を操作し行動を委ねる"), EAsyncExecution::ThreadPool,
        FTimespan::FromSeconds(180), [this] {
        if (!TestTrue(TEXT("開始"),ClickAndDrain(Driver->FindElement(By::Id("title.start"))))) return;
        if (!TestTrue(TEXT("出発"),ClickAndDrain(Driver->FindElement(By::Id("role.confirm"))))) return;
        // 初日だけ進行検証を使用。仲間加入後の実戦入力は予告画面を経由する。
        for(int step=0;step<2;++step) {
            if (!TestTrue(TEXT("初日の経路"),ClickAndDrain(Driver->FindElement(By::Id("route.node.0"))))) return;
            if(Driver->FindElement(By::Id("rest.heal"))->Exists()) {
                if (!TestTrue(TEXT("休息選択"),ClickAndDrain(Driver->FindElement(By::Id("rest.heal"))))) return;
                Driver->FindElement(By::Id("run.scroll"))->ScrollToEndUntil(By::Id("rest.confirm"));
                if (!TestTrue(TEXT("休息確定"),ClickAndDrain(Driver->FindElement(By::Id("rest.confirm"))))) return;
            } else if (!TestTrue(TEXT("初日完了"),ClickAndDrain(Driver->FindElement(By::Id("preview.complete"))))) return;
        }
        if (!TestTrue(TEXT("2日目の経路"),ClickAndDrain(Driver->FindElement(By::Id("route.node.0"))))) return;
        if (!TestTrue(TEXT("実戦開始"),ClickAndDrain(Driver->FindElement(By::Id("preview.battle"))))) return;
        if (!TestTrue(TEXT("主人公待機"),ClickAndDrain(Driver->FindElement(By::Id("battle.wait.south"))))) return;
        if (!TestTrue(TEXT("仲間の作戦UI"),Driver->FindElement(By::Id("intent.kind.0"))->Exists())) return;
        for(int kind=0;kind<4;++kind)
            TestTrue(TEXT("4種類の意図"),ClickAndDrain(Driver->FindElement(By::Id(*FString::Printf(TEXT("intent.kind.%d"),kind)))));
        // 入れ子の操作欄ではなく盤面上でホイールを回し、外側の画面を戻す。
        const auto showBoard=[this] {
            for(int step=0;step<8 && !Driver->FindElement(By::Id("battle.cell.4.1"))->IsVisible();++step) {
                if(!TestTrue(TEXT("盤面上で上へスクロール"),Driver->FindElement(By::Id("battle.cell.0.7"))->ScrollBy(5)))return false;
                DrainSlateLayout();
            }
            return TestTrue(TEXT("対象指定の盤面を表示"),Driver->FindElement(By::Id("battle.cell.4.1"))->IsVisible());
        };
        if(!showBoard())return;
        TestTrue(TEXT("敵を指示"),ClickAndDrain(Driver->FindElement(By::Id("battle.cell.4.1"))));
        TestTrue(TEXT("人物対象の表示"),Driver->FindElement(By::Id("intent.target"))->GetText().ToString().Contains(TEXT("人物100")));
        if(!showBoard())return;
        TestTrue(TEXT("不適切な味方対象"),ClickAndDrain(Driver->FindElement(By::Id("battle.cell.1.1"))));
        TestTrue(TEXT("不適切対象の説明"),Driver->FindElement(By::Id("battle.log"))->GetText().ToString().Contains(TEXT("指定できない")));
        TestTrue(TEXT("対象解除"),ClickAndDrain(Driver->FindElement(By::Id("intent.clear"))));
        TestTrue(TEXT("未指定に戻る"),Driver->FindElement(By::Id("intent.target"))->GetText().ToString().Contains(TEXT("任せる")));
        TestFalse(TEXT("低信頼は確率を隠す"),Driver->FindElement(By::Id("intent.forecast"))->GetText().ToString().Contains(TEXT("%")));
        // 実戦の信頼は絆から参照する。各段階の表示はBondSpecで初期Profile別に検証。
        TestTrue(TEXT("初期絆0を参照"),Driver->FindElement(By::Id("intent.trust"))->GetText().ToString().Contains(TEXT("絆（信頼）: 0")));
        TestFalse(TEXT("実戦の信頼を任意変更できない"),Driver->FindElement(By::Id("intent.trust.25"))->Exists());
        if(!showBoard())return;
        TestTrue(TEXT("地点を指示"),ClickAndDrain(Driver->FindElement(By::Id("battle.cell.2.2"))));
        TestTrue(TEXT("予告した行動を実行"),ClickAndDrain(Driver->FindElement(By::Id("intent.execute"))));
        TestTrue(TEXT("実行結果の表示"),Driver->FindElement(By::Id("battle.log"))->GetText().ToString().Contains(TEXT("作戦")));
        TestTrue(TEXT("仲間も向きを選んで待機"),ClickAndDrain(Driver->FindElement(By::Id("battle.wait.north"))));
        TestTrue(TEXT("次ラウンドの主人公を待機"),ClickAndDrain(Driver->FindElement(By::Id("battle.wait.south"))));
        TestTrue(TEXT("仲間の次手も予告"),Driver->FindElement(By::Id("intent.forecast"))->Exists());
    });
    It(TEXT("生成を8回失敗させ固定戦場で移動と攻撃を継続する"), EAsyncExecution::ThreadPool,
        FTimespan::FromSeconds(180), [this] {
        if (!TestTrue(TEXT("開始"),ClickAndDrain(Driver->FindElement(By::Id("title.start"))))) return;
        if (!TestTrue(TEXT("出発"),ClickAndDrain(Driver->FindElement(By::Id("role.confirm"))))) return;
        if (!TestTrue(TEXT("救出経路"),ClickAndDrain(Driver->FindElement(By::Id("route.node.0"))))) return;
        if (!TestTrue(TEXT("生成失敗を注入"),ClickAndDrain(Driver->FindElement(By::Id("preview.fallback"))))) return;
        const auto Description=Driver->FindElement(By::Id("battle.generation"))->GetText().ToString();
        TestTrue(TEXT("8回失敗後の切替表示"),Description.Contains(TEXT("固定戦場")) && Description.Contains(TEXT("8回")));
        TestTrue(TEXT("最大寸法の端も表示"),Driver->FindElement(By::Id("battle.cell.11.11"))->IsVisible());
        TestEqual(TEXT("端の座標ラベル"),Driver->FindElement(By::Id("battle.celllabel.11.11"))->GetText().ToString(),FString(TEXT("11,11")));
        TestTrue(TEXT("固定戦場でも移動"),ClickAndDrain(Driver->FindElement(By::Id("battle.cell.3.1"))));
        TestTrue(TEXT("固定戦場でも移動確定"),ClickAndDrain(Driver->FindElement(By::Id("battle.move.confirm"))));
        TestTrue(TEXT("固定戦場でも攻撃対象選択"),ClickAndDrain(Driver->FindElement(By::Id("battle.cell.4.1"))));
        TestTrue(TEXT("攻撃確定"),ClickAndDrain(Driver->FindElement(By::Id("battle.confirm"))));
        TestTrue(TEXT("攻撃結果"),!Driver->FindElement(By::Id("battle.log"))->GetText().IsEmpty());
    });
    It(TEXT("障害物の耐久を予告し破壊したマスへ移動する"), EAsyncExecution::ThreadPool,
        FTimespan::FromSeconds(120), [this] {
        // 初期盤面だけを固定する。破壊・待機・移動はすべて画面操作。
        Async(EAsyncExecution::TaskGraphMainThread,[this] {
            using namespace GirlsSpire;
            std::vector<Tile> tiles(64); tiles[10].ObstacleHp=20;
            BattleUnit hero; hero.Speed=20; hero.At={1,1}; hero.Role=BattleRole::Dancer;
            BattleUnit enemy; enemy.Id=100; enemy.Side=Faction::Enemy; enemy.At={7,7}; enemy.Movement=0;
            Window->SetContent(SNew(SCombatScreen).Combat(MakeShared<Battle>(8,8,tiles,std::vector<BattleUnit>{hero,enemy},42)));
        }).Wait();
        if(!TestTrue(TEXT("障害物を選択"),ClickAndDrain(Driver->FindElement(By::Id("battle.cell.2.1"))))) return;
        TestTrue(TEXT("耐久を表示"),Driver->FindElement(By::Id("battle.celllabel.2.1"))->GetText().ToString().Contains(TEXT("障20")));
        TestTrue(TEXT("確定前に耐久減少を予告"),Driver->FindElement(By::Id("battle.preview"))->GetText().ToString().Contains(TEXT("20 → 8")));
        if(!TestTrue(TEXT("地形への攻撃確定"),ClickAndDrain(Driver->FindElement(By::Id("battle.confirm"))))) return;
        TestTrue(TEXT("耐久8が残る"),Driver->FindElement(By::Id("battle.celllabel.2.1"))->GetText().ToString().Contains(TEXT("障8")));
        if(!TestTrue(TEXT("次の手番へ"),ClickAndDrain(Driver->FindElement(By::Id("battle.wait.south"))))) return;
        if(!TestTrue(TEXT("再び対象選択"),ClickAndDrain(Driver->FindElement(By::Id("battle.cell.2.1"))))) return;
        TestTrue(TEXT("残耐久まで予告"),Driver->FindElement(By::Id("battle.preview"))->GetText().ToString().Contains(TEXT("8 → 0")));
        if(!TestTrue(TEXT("破壊する"),ClickAndDrain(Driver->FindElement(By::Id("battle.confirm"))))) return;
        TestTrue(TEXT("破壊の結果表示"),Driver->FindElement(By::Id("battle.log"))->GetText().ToString().Contains(TEXT("破壊")));
        if(!TestTrue(TEXT("平地になったマスへ移動"),ClickAndDrain(Driver->FindElement(By::Id("battle.cell.2.1"))))) return;
        if(!TestTrue(TEXT("平地への移動確定"),ClickAndDrain(Driver->FindElement(By::Id("battle.move.confirm"))))) return;
        TestTrue(TEXT("移動完了"),Driver->FindElement(By::Id("battle.log"))->GetText().ToString().Contains(TEXT("移動しました")));
    });
    for(const bool fatal : {false,true})
    It(fatal?TEXT("罠の致死警告を了承すると途中で停止して敗北する"):TEXT("罠経路の予告と取消を確認して移動する"), EAsyncExecution::ThreadPool,
        FTimespan::FromSeconds(120), [this,fatal] {
        Async(EAsyncExecution::TaskGraphMainThread,[this,fatal] {
            using namespace GirlsSpire;
            std::vector<Tile> tiles(64); tiles[10].Trap=true; tiles[11].Trap=true;
            BattleUnit hero; hero.Speed=20; hero.At={1,1}; hero.Hp=fatal?5:30;
            BattleUnit enemy; enemy.Id=100; enemy.Side=Faction::Enemy; enemy.At={7,7}; enemy.Movement=0;
            Window->SetContent(SNew(SCombatScreen).Combat(MakeShared<Battle>(8,8,tiles,std::vector<BattleUnit>{hero,enemy},42)));
        }).Wait();
        if(!TestTrue(TEXT("移動先を選ぶ"),ClickAndDrain(Driver->FindElement(By::Id("battle.cell.4.1"))))) return;
        TestTrue(TEXT("罠を文字で表示"),Driver->FindElement(By::Id("battle.celllabel.2.1"))->GetText().ToString().Contains(TEXT("罠10")));
        const auto preview=Driver->FindElement(By::Id("battle.move.preview"))->GetText().ToString();
        TestTrue(TEXT("通過経路と被害を予告"),preview.Contains(TEXT("(2,1)")) && preview.Contains(fatal?TEXT("被害5"):TEXT("被害20")));
        TestTrue(TEXT("致死警告の有無"),preview.Contains(TEXT("戦闘不能"))==fatal);
        TestTrue(TEXT("確定前は元のHP"),Driver->FindElement(By::Id("battle.celllabel.1.1"))->GetText().ToString().Contains(fatal?TEXT("5"):TEXT("30")));
        if(!fatal) {
            if(!TestTrue(TEXT("移動取消"),ClickAndDrain(Driver->FindElement(By::Id("battle.move.cancel"))))) return;
            TestFalse(TEXT("予告を閉じる"),Driver->FindElement(By::Id("battle.move.confirm"))->Exists());
            if(!TestTrue(TEXT("再選択"),ClickAndDrain(Driver->FindElement(By::Id("battle.cell.4.1"))))) return;
        }
        if(!TestTrue(TEXT("警告を確認して移動確定"),ClickAndDrain(Driver->FindElement(By::Id("battle.move.confirm"))))) return;
        TestFalse(TEXT("通過した罠は消滅"),Driver->FindElement(By::Id("battle.celllabel.2.1"))->GetText().ToString().Contains(TEXT("罠")));
        if(fatal) {
            TestTrue(TEXT("未通過の罠は残る"),Driver->FindElement(By::Id("battle.celllabel.3.1"))->GetText().ToString().Contains(TEXT("罠10")));
            TestTrue(TEXT("全滅画面へ"),Driver->FindElement(By::Id("battle.lost"))->Exists());
        } else {
            TestTrue(TEXT("移動先でHP10"),Driver->FindElement(By::Id("battle.celllabel.4.1"))->GetText().ToString().Contains(TEXT("10")));
            TestTrue(TEXT("罠被害を結果表示"),Driver->FindElement(By::Id("battle.log"))->GetText().ToString().Contains(TEXT("20")));
        }
    });
    for(const int hp : {20,21})
    It(hp==20?TEXT("仲間は罠と敵攻撃で致死となる移動を選ばない"):TEXT("仲間の安全な罠経路を予告して実行する"), EAsyncExecution::ThreadPool,
        FTimespan::FromSeconds(120), [this,hp] {
        Async(EAsyncExecution::TaskGraphMainThread,[this,hp] {
            using namespace GirlsSpire;
            std::vector<Tile> tiles(64); tiles[10].Trap=true;
            BattleUnit hero; hero.At={0,0}; hero.Speed=10;
            BattleUnit ally; ally.Id=1; ally.At={1,1}; ally.Speed=20; ally.Hp=hp; ally.Movement=1;
            BattleUnit enemy; enemy.Id=100; enemy.Side=Faction::Enemy; enemy.At={3,1}; enemy.Movement=0;
            Window->SetContent(SNew(SCombatScreen).Combat(MakeShared<Battle>(8,8,tiles,std::vector<BattleUnit>{hero,ally,enemy},42)));
        }).Wait();
        if(!TestTrue(TEXT("罠地点を指示"),ClickAndDrain(Driver->FindElement(By::Id("battle.cell.2.1"))))) return;
        const auto label=Driver->FindElement(By::Id("intent.forecast"))->GetText().ToString();
        if(hp==20) {
            TestTrue(TEXT("致死移動ではなく待機"),label.Contains(TEXT("待機")) && !label.Contains(TEXT("移動")));
        } else {
            TestTrue(TEXT("罠と敵攻撃の合計を予告"),label.Contains(TEXT("被害上限20")));
            TestTrue(TEXT("経路と罠だけの被害も予告"),label.Contains(TEXT("経路")) && label.Contains(TEXT("罠被害10")));
        }
        if(!TestTrue(TEXT("仲間の予告を実行"),ClickAndDrain(Driver->FindElement(By::Id("intent.execute"))))) return;
        const auto cell=Driver->FindElement(By::Id("battle.celllabel.2.1"))->GetText().ToString();
        TestTrue(TEXT("待機なら罠維持・移動ならHP11"),cell.Contains(hp==20?TEXT("罠10"):TEXT("11")));
    });
    for(const bool heal : {false,true})
    It(heal?TEXT("応急手当を予告して回復し再使用待ちを表示する"):TEXT("強撃の予告とダメージと再使用待ちを表示する"), EAsyncExecution::ThreadPool,
        FTimespan::FromSeconds(120), [this,heal] {
        Async(EAsyncExecution::TaskGraphMainThread,[this] {
            using namespace GirlsSpire;
            BattleUnit hero; hero.Speed=20; hero.At={1,1}; hero.Hp=10; hero.Skills={true,true};
            BattleUnit enemy; enemy.Id=100; enemy.Side=Faction::Enemy; enemy.At={2,1}; enemy.Hp=enemy.MaxHp=100; enemy.Movement=0; enemy.Attack=0;
            Window->SetContent(SNew(SCombatScreen).Combat(MakeShared<Battle>(8,8,std::vector<Tile>(64),std::vector<BattleUnit>{hero,enemy},42)));
        }).Wait();
        if(!TestTrue(TEXT("技を選択"),ClickAndDrain(Driver->FindElement(By::Id(heal?"battle.skill.1":"battle.skill.0"))))) return;
        if(!TestTrue(TEXT("技の対象を選択"),ClickAndDrain(Driver->FindElement(By::Id(heal?"battle.cell.1.1":"battle.cell.2.1"))))) return;
        const auto preview=Driver->FindElement(By::Id("battle.skill.preview"))->GetText().ToString();
        TestTrue(TEXT("技の効果を予告"),preview.Contains(heal?TEXT("回復20"):TEXT("ダメージ18")));
        if(!TestTrue(TEXT("技を確定"),ClickAndDrain(Driver->FindElement(By::Id("battle.skill.confirm"))))) return;
        TestTrue(TEXT("HPに反映"),Driver->FindElement(By::Id(heal?"battle.celllabel.1.1":"battle.celllabel.2.1"))->GetText().ToString().Contains(heal?TEXT("30"):TEXT("82")));
        TestTrue(TEXT("再使用まで2回と表示"),Driver->FindElement(By::Id("battle.skill.status"))->GetText().ToString().Contains(heal?TEXT("応急手当:2"):TEXT("強撃:2")));
        TestFalse(TEXT("直ちに再使用不可"),Driver->FindElement(By::Id(heal?"battle.skill.1":"battle.skill.0"))->IsInteractable());
        if(!TestTrue(TEXT("次の自分の手番へ"),ClickAndDrain(Driver->FindElement(By::Id("battle.wait.south"))))) return;
        TestTrue(TEXT("使用直後の次手番も残り2"),Driver->FindElement(By::Id("battle.skill.status"))->GetText().ToString().Contains(heal?TEXT("応急手当:2"):TEXT("強撃:2")));
    });
    It(TEXT("仲間の支援で回復技を予告して実行する"), EAsyncExecution::ThreadPool,
        FTimespan::FromSeconds(120), [this] {
        Async(EAsyncExecution::TaskGraphMainThread,[this] {
            using namespace GirlsSpire;
            BattleUnit hero; hero.At={1,1}; hero.Hp=10;
            BattleUnit ally; ally.Id=1; ally.At={1,2}; ally.Speed=30; ally.Skills={true,true};
            BattleUnit enemy; enemy.Id=100; enemy.Side=Faction::Enemy; enemy.At={2,2};
            Window->SetContent(SNew(SCombatScreen).Combat(MakeShared<Battle>(8,8,std::vector<Tile>(64),std::vector<BattleUnit>{hero,ally,enemy},42)));
        }).Wait();
        if(!TestTrue(TEXT("支援を選択"),ClickAndDrain(Driver->FindElement(By::Id("intent.kind.2"))))) return;
        if(!TestTrue(TEXT("信頼75で確定"),ClickAndDrain(Driver->FindElement(By::Id("intent.trust.75"))))) return;
        const auto label=Driver->FindElement(By::Id("intent.forecast"))->GetText().ToString();
        TestTrue(TEXT("応急手当の回復量を予告"),label.Contains(TEXT("応急手当")) && label.Contains(TEXT("回復20")));
        if(!TestTrue(TEXT("予告を実行"),ClickAndDrain(Driver->FindElement(By::Id("intent.execute"))))) return;
        TestTrue(TEXT("主人公が回復"),Driver->FindElement(By::Id("battle.celllabel.1.1"))->GetText().ToString().Contains(TEXT("30")));
        TestTrue(TEXT("仲間の再使用待ち"),Driver->FindElement(By::Id("battle.skill.status"))->GetText().ToString().Contains(TEXT("応急手当:2")));
    });
    It(TEXT("技の取消と無効対象を確認し強撃で障害物を破壊する"), EAsyncExecution::ThreadPool,
        FTimespan::FromSeconds(120), [this] {
        Async(EAsyncExecution::TaskGraphMainThread,[this] {
            using namespace GirlsSpire;
            BattleUnit hero; hero.At={1,1}; hero.Speed=30; hero.Skills={true,true};
            BattleUnit enemy; enemy.Id=100; enemy.Side=Faction::Enemy; enemy.At={7,7};
            std::vector<Tile> tiles(64); tiles[10].ObstacleHp=20;
            Window->SetContent(SNew(SCombatScreen).Combat(MakeShared<Battle>(8,8,tiles,std::vector<BattleUnit>{hero,enemy},42)));
        }).Wait();
        if(!TestTrue(TEXT("回復を選ぶ"),ClickAndDrain(Driver->FindElement(By::Id("battle.skill.1"))))) return;
        for(const char* target:{"battle.cell.1.1","battle.cell.0.0","battle.cell.7.7"}) {
            if(!TestTrue(TEXT("無効対象を指定"),ClickAndDrain(Driver->FindElement(By::Id(target))))) return;
            TestFalse(TEXT("満タン・空マス・敵に回復できない"),Driver->FindElement(By::Id("battle.skill.confirm"))->IsInteractable());
            TestTrue(TEXT("拒否理由の表示"),Driver->FindElement(By::Id("battle.skill.preview"))->GetText().ToString().Contains(TEXT("使用できません")));
        }
        if(!TestTrue(TEXT("技を取り消す"),ClickAndDrain(Driver->FindElement(By::Id("battle.skill.cancel"))))) return;
        TestFalse(TEXT("技の予告が消える"),Driver->FindElement(By::Id("battle.skill.preview"))->Exists());
        TestTrue(TEXT("取消で行動を消費しない"),Driver->FindElement(By::Id("battle.turn"))->GetText().ToString().Contains(TEXT("行動:未")));
        if(!TestTrue(TEXT("強撃へ変更"),ClickAndDrain(Driver->FindElement(By::Id("battle.skill.0"))))) return;
        if(!TestTrue(TEXT("障害物を指定"),ClickAndDrain(Driver->FindElement(By::Id("battle.cell.2.1"))))) return;
        TestTrue(TEXT("確定20ダメージを予告"),Driver->FindElement(By::Id("battle.skill.preview"))->GetText().ToString().Contains(TEXT("ダメージ20")));
        if(!TestTrue(TEXT("障害物へ強撃"),ClickAndDrain(Driver->FindElement(By::Id("battle.skill.confirm"))))) return;
        TestFalse(TEXT("障害物が消える"),Driver->FindElement(By::Id("battle.celllabel.2.1"))->GetText().ToString().Contains(TEXT("障")));
        TestTrue(TEXT("破壊を表示"),Driver->FindElement(By::Id("battle.log"))->GetText().ToString().Contains(TEXT("障害物へ強撃")));
    });
    AfterEach([this] {
        FSlateApplication::Get().CloseToolTip();
        if (Window.IsValid()) {
            TArray<FColor> Pixels; FIntVector Size;
            if (TestTrue(TEXT("戦闘画面キャプチャ"),FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(), Pixels, Size))) {
                TArray64<uint8> Png;
                FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);
                IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("Screenshots")),true);
                const FString Path=FPaths::ProjectSavedDir()/TEXT("Screenshots")/FString::Printf(TEXT("Battle-%d.png"),++CaptureIndex);
                TestTrue(TEXT("戦闘画面の記録"),FFileHelper::SaveArrayToFile(Png,*Path));
            }
        }
        Driver.Reset();
        IAutomationDriverModule::Get().Disable();
        if (Window.IsValid()) { FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef()); Window.Reset(); }
#if defined(GIRLS_SPIRE_COVERAGE)
        TestEqual(TEXT("戦闘UI計測の記録"), __llvm_profile_write_file(), 0);
#endif
    });
}
