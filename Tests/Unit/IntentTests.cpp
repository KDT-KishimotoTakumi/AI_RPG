#include "AI/Intent.h"
#include <iostream>

int TestIntent()
{
    using namespace GirlsSpire;
    int checks=0,failed=0;
    const auto check=[&](bool pass,const char* label) { ++checks; if(!pass) { ++failed; std::cerr<<"FAIL Intent: "<<label<<'\n'; } };
    BattleUnit ally; ally.Id=1; ally.At={1,1}; ally.Speed=20;
    BattleUnit enemy; enemy.Id=100; enemy.Side=Faction::Enemy; enemy.At={6,6}; enemy.Movement=1;
    const auto make=[&] { return Battle(8,8,std::vector<Tile>(64),{ally,enemy},42); };
    const std::vector<BasicAction> ranked={{BasicActionKind::Move,{2,1}},{BasicActionKind::Move,{1,2}},{BasicActionKind::Wait,{}}};
    auto battle=make(); const auto before=battle.Snapshot(); const auto random=battle.RandomState();
    for(int trust : {-1,0,24,25,49,50,74,75,99,100,101}) {
        const auto forecast=IntentForecast::Build(battle,ranked,trust);
        const auto& candidates=forecast.Candidates();
        const std::size_t count=trust<50?3:trust<75?2:1;
        check(candidates.size()==count,"信頼ごとの候補数");
        check(forecast.Certain()==(trust>=75),"信頼75から確定");
        check(forecast.Fallback()==IntentFallback::None,"候補あり");
        if(candidates.size()==count) {
            for(std::size_t i=0;i<count;++i) {
                const int expected=trust<50?(i==0?50:i==1?30:20):(i==0?70:30);
                check(candidates[i].Action==ranked[i],"優先順を保持");
                check(candidates[i].Probability==(trust>=25 && trust<75?std::optional<int>{expected}:std::nullopt),"確率の表示境界");
            }
        }
    }
    check(battle.Snapshot()==before && battle.RandomState()==random,"予告で状態と乱数を消費しない");
    const auto two=IntentForecast::Build(battle,{ranked[0],ranked[1]},25);
    check(two.Candidates().size()==2 && two.Candidates()[0].Probability==63 && two.Candidates()[1].Probability==37,"少数候補の再配分・端数合計100");
    const auto single=IntentForecast::Build(battle,{ranked[0],ranked[0]},25);
    check(single.Candidates().size()==1 && single.Certain() && single.Candidates()[0].Probability==100,"重複を除き単一候補100");
    auto none=IntentForecast::Build(battle,{},0);
    check(none.Fallback()==IntentFallback::NoSafeCandidate && none.Certain() && none.Candidates().size()==1,"候補なし理由付き待機");
    Random choice(99); const auto choiceBefore=choice.State();
    check(none.Execute(battle,choice) && battle.Snapshot().ActiveId==100,"待機を実行");
    check(choice.State()==choiceBefore,"確定候補は乱数不消費");
    check(!none.Execute(battle,choice),"同じ予告の二重実行を拒否");
    const auto enemyTurn=IntentForecast::Build(battle,ranked,50);
    check(enemyTurn.Fallback()==IntentFallback::Unavailable && enemyTurn.Candidates().empty() && !enemyTurn.Execute(battle,choice),"敵手番では仲間選択不可");
    ally.Hp=0; auto ended=make();
    check(IntentForecast::Build(ended,ranked,0).Candidates().empty(),"終了戦闘は候補なし"); ally.Hp=30;
    for(int seed=0;seed<100;++seed) {
        auto first=make(),second=make(); Random a(seed),b(seed);
        const auto f=IntentForecast::Build(first,ranked,25),g=IntentForecast::Build(second,ranked,25);
        check(f.Candidates()==g.Candidates(),"予告再現");
        check(f.Execute(first,a) && g.Execute(second,b),"表示済み候補を実行");
        check(first.Snapshot()==second.Snapshot() && a.State()==b.State(),"同じシードの実行再現");
        Random expected(seed); const auto roll=expected.Uniform(100);
        const auto index=roll<50?0:roll<80?1:2;
        check(index==2?first.Snapshot().ActiveId==100:first.Snapshot().Units[0].At==ranked[index].Target,"表示50/30/20と抽選区間の一致");
    }
    enemy.At={3,1}; enemy.Attack=40; enemy.Movement=0;
    auto danger=make();
    const auto safe=IntentForecast::Build(danger,ranked,0);
    check(safe.Candidates().size()==2 && safe.Candidates()[0].Action==ranked[1],"HP不足で射程へ進む候補を除外");
    auto adjacent=danger; adjacent.Move(0,1,{2,1});
    check(adjacent.PotentialEnemyDamage(1)==38,"射線内の最大通常被害");
    check(adjacent.PotentialEnemyDamage(-1)==0 && adjacent.PotentialEnemyDamage(100)==0,"無効または敵対象の被害推定は0");
    const auto invalid=IntentForecast::Build(danger,{{BasicActionKind::Attack,{1,1}},{BasicActionKind::Move,{3,1}},{static_cast<BasicActionKind>(99),{}}},25);
    check(invalid.Fallback()==IntentFallback::NoSafeCandidate,"味方攻撃・占有移動・不正種別を除外");
    auto stale=make(); const auto old=IntentForecast::Build(stale,ranked,25); stale.Move(0,1,{1,2});
    const auto staleBefore=stale.Snapshot();
    check(!old.Execute(stale,choice) && stale.Snapshot()==staleBefore && choice.State()==choiceBefore,"古い予告は状態・乱数不変");
    auto otherSeed=Battle(8,8,std::vector<Tile>(64),{ally,enemy},43);
    const auto source=make(); const auto seedBound=IntentForecast::Build(source,ranked,75);
    check(!seedBound.Execute(otherSeed,choice),"同じ盤面でも戦闘乱数が異なる予告は拒否");
    enemy.Movement=2; enemy.At={5,1}; enemy.Attack=12;
    auto mobile=make(); mobile.Move(0,1,{2,1});
    const auto mobileBefore=mobile.Snapshot(); const auto mobileRandom=mobile.RandomState();
    check(mobile.PotentialEnemyDamage(1)==10,"敵の移動後の射程も安全判定に含む");
    check(mobile.Snapshot()==mobileBefore && mobile.RandomState()==mobileRandom,"安全判定で状態と乱数を変えない");
    BattleUnit secondEnemy=enemy; secondEnemy.Id=101; secondEnemy.At={2,4};
    auto combined=Battle(8,8,std::vector<Tile>(64),{ally,enemy,secondEnemy},42);
    combined.Move(0,1,{2,1});
    check(combined.PotentialEnemyDamage(1)==20,"複数敵の攻撃を一度ずつ合算");
    secondEnemy.Hp=0;
    auto knocked=Battle(8,8,std::vector<Tile>(64),{ally,enemy,secondEnemy},42);
    knocked.Move(0,1,{2,1});
    check(knocked.PotentialEnemyDamage(1)==10,"倒れた敵は危険度に含めない");
    ally.Hp=0; auto deadTarget=make();
    check(deadTarget.PotentialEnemyDamage(1)==0,"倒れた対象は0"); ally.Hp=30;
    enemy.Hp=0; auto won=make();
    check(won.PotentialEnemyDamage(1)==0,"終了した戦闘は0"); enemy.Hp=30;
    auto walls=std::vector<Tile>(64); for(int y=0;y<8;++y) walls[y*8+3].Wall=true;
    auto blocked=Battle(8,8,walls,{ally,enemy},42);
    check(blocked.PotentialEnemyDamage(1)==0,"壁を抜ける危険を誤検出しない");
    enemy.At={2,1}; enemy.Movement=0;
    auto hitBattle=make();
    const auto hitForecast=IntentForecast::Build(hitBattle,{{BasicActionKind::Attack,{2,1}}},75);
    check(hitForecast.Candidates().size()==1 && hitForecast.Candidates()[0].PotentialDamage==10,"攻撃候補にも被害上限を予告");
    check(hitForecast.Execute(hitBattle,choice) && hitBattle.Snapshot().Acted,"攻撃候補の実行");
    auto waitBattle=make();
    check(IntentForecast::Build(waitBattle,{{BasicActionKind::Wait,{8,9}},{BasicActionKind::Wait,{}}},25).Candidates().size()==1,"待機の無意味な座標で重複候補を作らない");
    enemy.At={6,6}; enemy.Movement=1;
    auto tactics=make();
    for(auto intent : {TacticalIntent::Assault,TacticalIntent::Escort,TacticalIntent::Support,TacticalIntent::Disrupt}) {
        const auto actions=RankIntentActions(tactics,{intent,std::nullopt,Position{3,1}});
        check(!actions.empty() && actions.front().Kind==BasicActionKind::Move,"全意図で指定地点へ接近");
        const auto f=IntentForecast::Build(tactics,actions,75); auto copy=tactics;
        check(f.Execute(copy,choice),"作戦候補は実行可能");
    }
    check(RankIntentActions(tactics,{static_cast<TacticalIntent>(99),{}, {}}).empty(),"不正意図拒否");
    check(RankIntentActions(tactics,{TacticalIntent::Assault,999,{}}).empty(),"不在対象拒否");
    check(RankIntentActions(tactics,{TacticalIntent::Assault,1,{}}).empty(),"攻勢の味方対象拒否");
    check(RankIntentActions(tactics,{TacticalIntent::Escort,100,{}}).empty(),"護衛の敵対象拒否");
    check(RankIntentActions(tactics,{TacticalIntent::Support,{},Position{-1,0}}).empty(),"盤外地点拒否");
    check(RankIntentActions(tactics,{TacticalIntent::Support,1,Position{1,1}}).empty(),"対象と地点の二重指定拒否");
    check(RankIntentActions(ended,{}).empty(),"終了戦闘の作戦候補なし");
    auto enemyTactics=tactics; enemyTactics.EndTurn(0,1,Facing::South);
    check(RankIntentActions(enemyTactics,{}).empty(),"敵手番の作戦候補なし");
    for(auto intent : {TacticalIntent::Assault,TacticalIntent::Escort,TacticalIntent::Support,TacticalIntent::Disrupt})
        check(!RankIntentActions(tactics,{intent,{},{}}).empty(),"対象未指定でも候補を返す");
    ally.Hp=20; enemy.At={5,1}; enemy.Movement=2; secondEnemy.At={2,4}; secondEnemy.Hp=30;
    auto exactly=Battle(8,8,std::vector<Tile>(64),{ally,enemy,secondEnemy},42);
    check(IntentForecast::Build(exactly,{{BasicActionKind::Move,{2,1}}},75).Fallback()==IntentFallback::NoSafeCandidate,"複数敵の合計とHPが同値なら移動拒否");
    ally.Hp=21;
    auto barely=Battle(8,8,std::vector<Tile>(64),{ally,enemy,secondEnemy},42);
    check(IntentForecast::Build(barely,{{BasicActionKind::Move,{2,1}}},75).Fallback()==IntentFallback::None,"合計被害よりHPが1多ければ候補可");
    ally.Hp=30; ally.Range=6; enemy.At={4,1}; enemy.Hp=1; secondEnemy.At={2,1};
    auto explicitTarget=Battle(8,8,std::vector<Tile>(64),{ally,enemy,secondEnemy},42);
    const auto explicitActions=RankIntentActions(explicitTarget,{TacticalIntent::Assault,100,{}});
    check(!explicitActions.empty() && explicitActions.front()==BasicAction{BasicActionKind::Attack,{4,1}},"攻勢では指定敵をダメージ効率より優先");
    std::cout<<"Intent: "<<checks<<" checks, "<<failed<<" failed\n";
    return failed;
}
