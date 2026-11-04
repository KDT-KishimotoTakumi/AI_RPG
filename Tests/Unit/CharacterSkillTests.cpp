#include "Domain/Battle.h"
#include "Domain/AbilityCatalog.h"
#include "../Fixtures/AbilityContextCases.h"
#include "AI/Intent.h"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
using namespace GirlsSpire;
int TestCharacterSkills()
{
    int checks=0,failed=0;const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL CharacterSkills: "<<why<<'\n';}};
    for(int id=0;id<9;++id)for(int slot=0;slot<2;++slot){
        const auto ability=*FormalSkill(id,static_cast<Skill>(slot));
        BattleUnit actor;actor.Id=id;actor.At={1,1};actor.Hp=actor.MaxHp=100;actor.Attack=20;actor.Accuracy=200;actor.Speed=30;actor.Skills={true,true};
        auto ally=actor;ally.Id=id==0?1:0;ally.At={1,2};ally.Speed=20;ally.Hp=50;ally.AttackCursed=true;ally.MovementWebbed=true;
        auto enemy=actor;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={2,1};enemy.Speed=10;enemy.Evasion=0;enemy.Hp=enemy.MaxHp=200;
        Battle base(8,8,std::vector<Tile>(64),{actor,ally,enemy},42);auto checkpoint=base.Checkpoint();checkpoint.State.AbilityRules=AbilityRuleSet::CharacterSkills;
        const auto restored=Battle::Restore(checkpoint);check(restored.has_value(),"新ルールを復元");if(!restored)continue;
        auto battle=*restored;const auto at=ability.Target==AbilityTarget::Enemy?enemy.At:ability.Target==AbilityTarget::Self?actor.At:ally.At;
        if(ability.Target!=AbilityTarget::Enemy){
            const auto ranked=RankIntentActions(*restored,{TacticalIntent::Support,ally.Id,{}});
            check(!ranked.empty() && ranked.front().Kind==BasicActionKind::UseSkill,"支援AIは枠ではなく回復/強化を支援候補へ分類");
        }
        const auto before=battle.Checkpoint();const auto preview=battle.PreviewSkill(id,static_cast<Skill>(slot),at);
        check(preview.Legal && preview.Support==(ability.Target!=AbilityTarget::Enemy),"全18技能の対象分類と合法判定");
        check(battle.Checkpoint()==before,"予告で乱数/状態不変");
        check(!battle.UseSkill(before.State.Revision+1,id,static_cast<Skill>(slot),at) && battle.Checkpoint()==before,"古いrevisionで無変更");
        check(battle.UseSkill(before.State.Revision,id,static_cast<Skill>(slot),at),"全18技能を実行");
        const auto& state=battle.Snapshot();const auto& recipient=state.Units[ability.Target==AbilityTarget::Self?0:1];
        check(state.Acted && state.Units[0].Cooldowns[slot]==ability.Cooldown,"承認CD/行動1回");
        if(ability.Target==AbilityTarget::Enemy){
            check(preview.Attack.Legal && preview.Attack.Hits[0].Damage==20+ability.Effects.AttackBonus-enemy.Defense,"承認威力を通常防御で軽減");
            check(battle.LastHits().size()==1 && state.Units[2].Hp==enemy.Hp-(battle.LastHits()[0].Hit?preview.Attack.Hits[0].Damage:0),"命中1回で被害一致");
            check(state.Units[2].AttackCursed==(battle.LastHits()[0].Hit && ability.Effects.AttackPenalty>0) && state.Units[2].MovementWebbed==(battle.LastHits()[0].Hit && ability.Effects.MovementPenalty>0),"弱体は命中時だけ");
        }else{
            check(battle.RandomState()==before.RandomState && recipient.Hp==(ability.Target==AbilityTarget::Self?100:50)+ability.Effects.Healing,"回復/強化は乱数非消費");
            check(recipient.AttackBoost.Amount==ability.Effects.AttackBuff && recipient.DefenseBoost.Amount==ability.Effects.DefenseBuff && recipient.AccuracyBoost.Amount==ability.Effects.AccuracyBuff,"全強化値の付与");
            check(!ability.Effects.Cleanse || (!recipient.AttackCursed && !recipient.MovementWebbed),"小夜の攻撃/移動低下解除");
            check(!ability.Effects.AttackBuff || !recipient.AttackBoost.SkipCurrentEnd,"他者強化は使用者の期限除外を付けない");
            for(const auto& boost:{recipient.AttackBoost,recipient.DefenseBoost,recipient.AccuracyBoost})if(boost.Amount)check(boost.SkipCurrentEnd==(recipient.Id==id),"自己対象だけ使用手番を除外");
        }
        if(!state.Combos.Pending.empty())check(state.Combos.Pending.back().Attributes==ability.Syntax,"正式構文は結/転/承を単一で記録");
        check(!battle.UseSkill(state.Revision,id,static_cast<Skill>(slot),at),"同じ手番に再使用不可");
        auto invalid=*restored;const auto wrong=ability.Target==AbilityTarget::Enemy?ally.At:enemy.At;
        check(!invalid.PreviewSkill(id,static_cast<Skill>(slot),wrong).Legal,"味方攻撃/敵支援/他者への自己技を拒否");
        check(!invalid.PreviewSkill(id,static_cast<Skill>(slot),{-1,0}).Legal && !invalid.PreviewSkill(id,static_cast<Skill>(slot),{7,7}).Legal,"盤外/空マスを拒否");
        auto disabled=checkpoint;disabled.State.Units[0].Skills[slot]=false;const auto noSkill=Battle::Restore(disabled);
        check(noSkill && !noSkill->PreviewSkill(id,static_cast<Skill>(slot),at).Legal,"未所持を拒否");
        disabled=checkpoint;disabled.State.Units[0].Cooldowns[slot]=1;const auto cooling=Battle::Restore(disabled);
        check(cooling && !cooling->PreviewSkill(id,static_cast<Skill>(slot),at).Legal,"CD中を拒否");
        auto wall=checkpoint;wall.State.Tiles[1*8+2].Wall=true;wall.State.Units[2].At={3,1};
        if(ability.Target==AbilityTarget::Enemy){const auto blocked=Battle::Restore(wall);check(blocked && !blocked->PreviewSkill(id,static_cast<Skill>(slot),{3,1}).Legal,"壁越しを拒否");}
    }
    for(bool cursed:{false,true}){
        BattleUnit sayo;sayo.Id=8;sayo.At={1,1};sayo.Speed=30;sayo.Skills={true,true};
        auto target=sayo;target.Id=0;target.At={1,2};target.Speed=20;target.AttackCursed=cursed;target.MovementWebbed=cursed;
        auto foe=sayo;foe.Id=100;foe.Side=Faction::Enemy;foe.At={3,1};foe.Speed=10;
        Battle arena(8,8,std::vector<Tile>(64),{sayo,target,foe},42);auto context=arena.Checkpoint();context.State.AbilityRules=AbilityRuleSet::CharacterSkills;
        auto battle=Battle::Restore(context);const auto before=battle->Checkpoint();const auto preview=battle->PreviewSkill(8,Skill::PowerStrike,target.At);
        check(preview.Legal==cursed && preview.Healing==0,"満タン時は解除の実効がある場合だけ鈴祓いを許可");
        if(cursed)check(battle->UseSkill(before.State.Revision,8,Skill::PowerStrike,target.At) && !battle->Snapshot().Units[1].AttackCursed && !battle->Snapshot().Units[1].MovementWebbed && battle->RandomState()==before.RandomState,"解除のみでも成功し乱数非消費");
        else check(!battle->UseSkill(before.State.Revision,8,Skill::PowerStrike,target.At) && battle->Checkpoint()==before,"実効なしの回復は全状態無変更");
    }
    const auto path=std::filesystem::path(u8"" __FILE__).parent_path().parent_path()/"Fixtures"/"skill-v13.bin";
    std::ifstream input(path,std::ios::binary);const std::string bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
    auto expected=TestData::AbilityContextCase(true);auto& hero=expected.Combat->Battle.State.Units[0];hero.AttackBoost={4,true};hero.DefenseBoost={2,false};hero.AccuracyBoost={10,true};
    check(bytes.size()==3418 && static_cast<unsigned char>(bytes[4])==13 && TestData::RosterPayloadHash(bytes)==UINT64_C(6300159118395224408) && DecodeRunPayload(bytes)==expected && InspectPayload(bytes,SaveKind::Run)==SaveError::None,"固定v13の版/既知ハッシュ/旧技能識別/強化/全状態を維持");
    if(const auto old=DecodeRunPayload(bytes)){
        auto checkpoint=old->Combat->Battle;checkpoint.State.Units[0].Hp-=20;
        auto battle=Battle::Restore(checkpoint);
        check(battle && battle->UseSkill(battle->Snapshot().Revision,0,Skill::FirstAid,checkpoint.State.Units[0].At) && !battle->Snapshot().Combos.Pending.empty() && battle->Snapshot().Combos.Pending.back().Attributes==(Continuation|Twist),"旧v13の回復は旧構文の承/転を維持して実行");
    }
    check(RunPayloadSchema>=14,"旧エンジンが新識別を破損扱いしないよう周回版を更新");
    {
        BattleUnit defender;defender.Id=0;defender.At={1,1};defender.Hp=13;defender.MaxHp=30;
        BattleUnit ninja;ninja.Id=3;ninja.Side=Faction::Enemy;ninja.Role=BattleRole::Ninja;ninja.At={4,1};ninja.Speed=30;ninja.Movement=4;ninja.Skills={true,true};
        Battle original(8,8,std::vector<Tile>(64),{defender,ninja},42);auto checkpoint=original.Checkpoint();checkpoint.State.AbilityRules=AbilityRuleSet::CompleteCharacters;
        auto battle=Battle::Restore(checkpoint);check(battle.has_value(),"正式技能の敵忍を含む予測戦場を復元");
        if(battle){
            const auto before=battle->Checkpoint();const auto distant=battle->PreviewSkill(3,Skill::FirstAid,defender.At);
            check(distant.Legal && distant.Attack.Hits.front().Damage==10,"射程3の影縫いは現在地で命中予測可能");
            auto moved=*battle;check(moved.Move(moved.Snapshot().Revision,3,{2,1}),"敵忍は合法な経路で接近できる");
            const auto close=moved.PreviewSkill(3,Skill::PowerStrike,defender.At);
            check(close.Legal && close.Attack.Hits.front().Damage==14,"接近後の霞斬りはより強い被害になる");
            check(battle->PotentialHostileDamage(0)==14,"現在地で攻撃できても移動後の強い技能を被害上限に含む");
            check(battle->Checkpoint()==before,"移動込み被害予測は戦闘状態と乱数を変更しない");
            auto limited=before;limited.State.Units[1].Movement=1;
            const auto shortMove=Battle::Restore(limited);
            check(shortMove && shortMove->PotentialHostileDamage(0)==10,"強い技の射程へ届かない移動力では被害を増やさない");
            auto waiting=before;waiting.State.Units[1].Cooldowns[0]=1;
            const auto cooldown=Battle::Restore(waiting);
            check(cooldown && cooldown->PotentialHostileDamage(0)==10,"再使用待ちの強い技は移動後も候補にしない");
            auto trapped=before;trapped.State.Units[1].Hp=5;
            for(const auto at:std::vector<Position>{{3,1},{5,1},{4,0},{4,2}})trapped.State.Tiles[at.Y*8+at.X].Trap=true;
            const auto lethalPath=Battle::Restore(trapped);
            check(lethalPath && lethalPath->PotentialHostileDamage(0)==10,"移動中に罠で倒れる敵の攻撃を加算しない");
            auto legacy=before;legacy.State.AbilityRules=AbilityRuleSet::Legacy;
            const auto oldRules=Battle::Restore(legacy);
            check(oldRules && oldRules->PotentialHostileDamage(0)==18,"旧ルールの強撃と移動の予測値を維持");
            auto allyTurn=before;allyTurn.State.Units[0].Speed=40;allyTurn.State.Order={0,3};allyTurn.State.ActiveId=0;
            const auto forecastSource=Battle::Restore(allyTurn);
            check(forecastSource.has_value(),"味方手番の安全検査用に復元");
            if(forecastSource){
                const std::vector<BasicAction> moveOnly{{BasicActionKind::Move,{2,1}}};
                const auto unsafe=IntentForecast::Build(*forecastSource,moveOnly,100);
                check(unsafe.Fallback()==IntentFallback::NoSafeCandidate,"移動後の強い技能で倒される候補を除外");
                allyTurn.State.Units[0].Hp=15;const auto healthy=Battle::Restore(allyTurn);
                const auto safe=healthy?IntentForecast::Build(*healthy,moveOnly,100):IntentForecast{};
                check(healthy && safe.Fallback()==IntentFallback::None && safe.Candidates().front().PotentialDamage==14,"耐えられる同じ移動は正しい被害表示で残す");
            }
        }
    }
    std::cout<<"CharacterSkills: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
