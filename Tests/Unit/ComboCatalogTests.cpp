#include "../Fixtures/ComboCatalogCases.h"
#include "Infrastructure/SavePayload.h"
#include "Domain/ComboCatalog.h"
#include <algorithm>
#include <iostream>
using namespace GirlsSpire;
int TestComboCatalog()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){if(failed<12)std::cerr<<"FAIL ComboCatalog: "<<why<<'\n';++failed;}};
    check(ComboCatalog().size()==20 && !FindComboRecipe(-1) && !FindComboRecipe(9999),"承認済み20種だけをカタログへ登録");
    Profile discovered;
    for(const auto& fixture:TestData::ComboCatalogCases()) {
        std::vector<BattleUnit> units;
        for(std::size_t i=0;i<fixture.Players.size();++i) {BattleUnit u;u.Id=static_cast<int>(i);u.At=fixture.Players[i];u.Speed=50-static_cast<int>(i);u.Range=12;u.Accuracy=200;u.Hp=u.MaxHp=200;u.Skills={true,true};if(fixture.Targets[i].empty() && fixture.Tags[i]!=Opening)u.Hp=10;units.push_back(u);}
        for(std::size_t i=0;i<fixture.Enemies.size();++i) {BattleUnit u;u.Id=100+static_cast<int>(i);u.Side=Faction::Enemy;u.At=fixture.Enemies[i];u.Hp=u.MaxHp=5000;u.Attack=0;u.Movement=0;u.Speed=0;units.push_back(u);}
        ComboLedger ledger;
        for(std::size_t i=0;i<fixture.Tags.size();++i) {
            ComboEvent event;event.Sequence=i+1;event.ActorId=static_cast<int>(i);event.Attributes=fixture.Tags[i];event.EnemyTargets=fixture.Targets[i];
            check(ledger.Observe(event),"承認済み行動を記録");
            const auto resolved=ledger.Resolve(event.Sequence,FindComboCandidates(ledger.Snapshot(),units));
            check(i+1==fixture.Tags.size()?(resolved && resolved->RecipeId==fixture.Id):!resolved,"各接頭辞で自動解決し最後に目的IDへ到達");
        }
        const int gauge=fixture.Tags.size()==2?10:fixture.Tags.size()==3?20:40;
        check(ledger.Snapshot().Gauge==gauge,"人数別ゲージのみ加算");
        const auto recipe=FindComboRecipe(fixture.Id);
        check(recipe && !recipe->Name.empty() && !recipe->Reason.empty() && recipe->Pattern.size()==fixture.Tags.size(),"名前/理由/人数を共有カタログから取得");
        check(DiscoverCombo(discovered,fixture.Id),"20種のIDを恒久発見へ登録");
        std::vector<ComboAction> actions;for(std::size_t i=0;i<fixture.Tags.size();++i)actions.push_back({i+1,static_cast<int>(i),fixture.Tags[i],fixture.Targets[i]});
        std::vector<const ComboAction*> pointers;for(const auto& action:actions)pointers.push_back(&action);
        check(MatchesComboCondition(fixture.Id,pointers,units),"各承認条件を直接満たす");
        check(recipe && recipe->Name==TestData::ComboExpectedName(fixture.Id),"承認された名前と一致");
        check(recipe && recipe->Pattern==TestData::ComboExpectedPattern(fixture) && recipe->Reason.find(TestData::ComboExpectedSyntax(fixture))!=std::string_view::npos,"承認された構文と説明が一致");
        for(bool mirrorX:{false,true})for(bool mirrorY:{false,true})for(int dx:{0,1})for(int dy:{0,1}) {
            auto moved=units;for(auto& unit:moved){unit.At.X=(mirrorX?7-unit.At.X:unit.At.X)+dx;unit.At.Y=(mirrorY?7-unit.At.Y:unit.At.Y)+dy;}
            check(MatchesComboCondition(fixture.Id,pointers,moved),"平行移動と反転でも成立条件を維持");
        }
        check(!MatchesComboCondition(9999,pointers,units),"未知のIDを拒否");
        auto bad=pointers;bad.pop_back();check(!MatchesComboCondition(fixture.Id,bad,units),"人数不足を拒否");
        bad=pointers;bad[0]=nullptr;check(!MatchesComboCondition(fixture.Id,bad,units),"存在しない行動を拒否");
        auto wrong=actions;wrong[0].Attributes=0;bad=pointers;bad[0]=&wrong[0];check(!MatchesComboCondition(fixture.Id,bad,units),"構文属性不一致を拒否");
        wrong=actions;wrong.back().ActorId=0;bad=pointers;bad.back()=&wrong.back();check(!MatchesComboCondition(fixture.Id,bad,units),"人物重複を拒否");
        wrong=actions;wrong.back().EnemyTargets={9999};bad=pointers;bad.back()=&wrong.back();check(!MatchesComboCondition(fixture.Id,bad,units),"存在しない敵への攻撃を拒否");
        wrong=actions;wrong.back().EnemyTargets.clear();bad=pointers;bad.back()=&wrong.back();check(!MatchesComboCondition(fixture.Id,bad,units),"最後に敵へ命中していない候補を拒否");
        auto changed=units;changed[0].Hp=0;check(!MatchesComboCondition(fixture.Id,pointers,changed),"戦闘不能の参加者を拒否");
        changed=units;changed[0].Side=Faction::Enemy;check(!MatchesComboCondition(fixture.Id,pointers,changed),"敵を参加者へ含めない");
        changed=units;changed.erase(changed.begin());check(!MatchesComboCondition(fixture.Id,pointers,changed),"存在しない参加者を拒否");
        changed=units;changed.back().Side=Faction::Neutral;check(!MatchesComboCondition(fixture.Id,pointers,changed),"中立への攻撃を対象に含めない");
        changed=units;BattleUnit another;another.Id=9998;another.Side=Faction::Enemy;another.At={7,7};changed.push_back(another);
        wrong=actions;
        wrong.back().EnemyTargets=(fixture.Id==102 || fixture.Id==104 || fixture.Id==110 || fixture.Id==111)?actions[0].EnemyTargets:std::vector<int>{9998};
        bad=pointers;bad.back()=&wrong.back();
        check(MatchesComboCondition(fixture.Id,bad,changed)==(fixture.Id==2 || fixture.Id==5),"有効な敵IDでも共通/別対象条件が崩れれば不成立・旧2/5は対象一致を要求しない");
        int successful=0;
        for(unsigned seed=0;seed<12;++seed)for(bool kill:{false,true}) {
            Battle battle(8,8,std::vector<Tile>(64),units,seed);bool allHit=true;
            for(std::size_t i=0;i<fixture.Tags.size();++i) {
                const int actor=static_cast<int>(i);bool legal=false;
                if(fixture.Tags[i]==Opening)legal=battle.EndTurn(battle.Snapshot().Revision,actor,Facing::South);
                else if(fixture.Targets[i].empty())legal=battle.UseSkill(battle.Snapshot().Revision,actor,Skill::FirstAid,units[i].At);
                else {
                    const int target=fixture.Targets[i][0];const auto enemy=std::find_if(units.begin(),units.end(),[target](const BattleUnit& u){return u.Id==target;});
                    if(kill && i+1==fixture.Tags.size()) {
                        auto checkpoint=battle.Checkpoint();for(auto& u:checkpoint.State.Units)if(u.Id==target)u.Hp=1;
                        const auto restored=Battle::Restore(checkpoint);check(restored.has_value(),"撃破直前も保存復元可能");if(restored)battle=*restored;
                    }
                    legal=fixture.Tags[i]&Twist?battle.UseSkill(battle.Snapshot().Revision,actor,Skill::PowerStrike,enemy->At):battle.Attack(battle.Snapshot().Revision,actor,enemy->At);
                    allHit=allHit && std::all_of(battle.LastHits().begin(),battle.LastHits().end(),[](const AttackHit& hit){return hit.Hit;});
                }
                check(legal,"移動/待機/攻撃/技能を実際の戦闘APIで解決");
                if(!legal)break;
                const auto restored=Battle::Restore(battle.Checkpoint());check(restored && restored->Checkpoint()==battle.Checkpoint(),"各行動で全状態/乱数を復元");
                if(i+1<fixture.Tags.size() && fixture.Tags[i]!=Opening) {
                    const auto recorded=battle.Snapshot().Combos.LastSequence;
                    check(battle.EndTurn(battle.Snapshot().Revision,actor,Facing::South),"次の人物へ手番を移す");
                    check(fixture.Targets[i].empty()?battle.Snapshot().Combos.LastSequence>recorded:battle.Snapshot().Combos.LastSequence==recorded,"攻撃後の終了と回復後の待機を区別する");
                }
            }
            const bool matched=battle.Snapshot().LastCombo && battle.Snapshot().LastCombo->RecipeId==fixture.Id;
            if(allHit){++successful;check(matched && battle.Snapshot().Combos.Gauge==gauge,(std::string("実戦の成立 ID=")+std::to_string(fixture.Id)).c_str());}
            else check(!matched,"外れた行動を目的レシピへ再使用しない");
        }
        check(successful>0,"各レシピに実戦APIでの成功例がある");
    }
    const auto encoded=EncodeProfilePayload(discovered);check(discovered.DiscoveredCombos.size()==20 && encoded && DecodeProfilePayload(*encoded)==discovered,"20種の恒久発見を保存復元");
    {
        BattleUnit a;a.At={1,1};auto b=a;b.Id=1;
        BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={1,2};auto other=enemy;other.Id=101;other.At={3,2};
        ComboAction first{1,0,Twist|Ending,{100}},last{2,1,Continuation|Ending,{101}};
        const std::vector<const ComboAction*> pair{&first,&last};
        for(int distance:{1,2,3,4,5}) {b.At={1+distance,1};check(MatchesComboCondition(104,pair,{a,b,enemy,other})==(distance>=2 && distance<=4),"直線距離2～4の両境界");}
        b.At={3,2};check(!MatchesComboCondition(104,pair,{a,b,enemy,other}),"直線から外れた候補を拒否");
        first.Attributes=Continuation|Ending;last.EnemyTargets={100};
        for(int distance:{1,2,3,4,5}) {b.At={1+distance,1};check(MatchesComboCondition(101,pair,{a,b,enemy,other})==(distance>=2),"距離2以上の境界");}
        a.At={1,1};b.At={3,1};enemy.At={2,1};enemy.Hp=0;first.Attributes=Twist|Ending;
        check(MatchesComboCondition(103,pair,{a,b,enemy}),"挟撃は撃破済みの敵も位置に使う");
        a.At={2,0};b.At={2,2};check(MatchesComboCondition(103,pair,{a,b,enemy}),"縦方向の挟撃");
        a.At={1,1};b.At={4,1};check(!MatchesComboCondition(103,pair,{a,b,enemy}) && !MatchesComboCondition(105,pair,{a,b,enemy}),"片側が隣接していない包囲を拒否");
    }
    {
        std::vector<BattleUnit> units;for(int i=0;i<3;++i){BattleUnit u;u.Id=i;units.push_back(u);}BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={6,6};units.push_back(enemy);
        std::vector<ComboAction> actions={{1,0,Twist|Ending,{100}},{2,1,Continuation|Ending,{100}},{3,2,Ending,{100}}};
        const std::vector<const ComboAction*> selected{&actions[0],&actions[1],&actions[2]};
        std::vector<int> order{0,1,2};const Position corner[]={{1,1},{1,4},{3,1}};
        do {for(int i=0;i<3;++i)units[i].At=corner[order[i]];check(MatchesComboCondition(108,selected,units),"L字の角がどの参加者でも成立");}while(std::next_permutation(order.begin(),order.end()));
        units[0].At={1,1};units[1].At={2,1};units[2].At={3,1};check(!MatchesComboCondition(108,selected,units),"一直線をL字にしない");
        units[0].At={1,2};units[1].At={1,1};units[2].At={3,1};
        ComboSnapshot history;history.LastSequence=3;history.Pending={{1,0,Continuation|Ending,{100}},{2,1,Continuation|Ending,{100}},{3,2,Ending,{100}}};
        const auto found=FindComboCandidates(history,units);const auto at=std::find_if(found.begin(),found.end(),[](const ComboCandidate& c){return c.RecipeId==101;});
        check(at!=found.end() && at->Actions==std::vector<std::uint64_t>({2,3}),"先頭候補が位置不一致でも後続の有効列を探索");
    }
    std::cout<<"ComboCatalog: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
