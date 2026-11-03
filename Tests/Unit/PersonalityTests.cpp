#include "AI/Personality.h"
#include "AI/Intent.h"
#include "../Fixtures/AbilityContextCases.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
namespace {
GirlsSpire::Battle MakePersonalityBattle(int id,bool cursed=false,bool webbed=false,int hp=30)
{
    using namespace GirlsSpire;
    BattleUnit actor;actor.Id=id;actor.At={1,1};actor.Speed=30;actor.Range=3;
    actor.Skills[0]=true;actor.Skills[1]=true;
    BattleUnit ally;ally.Id=0;ally.At={1,2};ally.Hp=hp;ally.AttackCursed=cursed;ally.MovementWebbed=webbed;
    BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={3,1};enemy.AttackCursed=cursed;enemy.MovementWebbed=webbed;enemy.Hp=hp;
    auto checkpoint=Battle(8,8,std::vector<Tile>(64),{actor,ally,enemy},42).Checkpoint();
    checkpoint.State.AbilityRules=AbilityRuleSet::CompleteCharacters;
    auto battle=Battle::Restore(checkpoint);
    if(!battle)throw std::logic_error("性格検証の戦闘が無効です");
    return *battle;
}
}
int TestPersonality()
{
    using namespace GirlsSpire;
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL Personality: "<<why<<'\n';}};
    const BasicAction attack{BasicActionKind::Attack,{3,1}};
    const BasicAction skill{BasicActionKind::UseSkill,{3,1},Skill::PowerStrike};
    const BasicAction debuff{BasicActionKind::UseSkill,{3,1},Skill::FirstAid};
    const BasicAction heal{BasicActionKind::UseSkill,{1,2},Skill::PowerStrike};
    const BasicAction buff{BasicActionKind::UseSkill,{1,2},Skill::FirstAid};
    check(PersonalityBonus(MakePersonalityBattle(1,false,false,10),attack)==500,"茜は致死予測の通常攻撃を優先");
    check(PersonalityBonus(MakePersonalityBattle(1),attack)==0,"茜は非致死を加点しない");
    check(PersonalityBonus(MakePersonalityBattle(1,false,false,18),skill)==500,"茜は技能の増加分を含む致死予測");
    check(PersonalityBonus(MakePersonalityBattle(1,false,false,19),skill)==0,"茜は技能ダメージよりHPが1高ければ加点なし");
    check(PersonalityBonus(MakePersonalityBattle(2,false,false,29),heal)==500,"澪は実回復1以上を優先");
    check(PersonalityBonus(MakePersonalityBattle(2),heal)==0,"澪は満タンへの回復不可");
    check(PersonalityBonus(MakePersonalityBattle(2,false,false,10),buff)==0,"澪は強化だけを加点しない");
    for(bool curse:{false,true})for(bool web:{false,true}){
        const auto battle=MakePersonalityBattle(3,curse,web);
        check(PersonalityBonus(battle,attack)==(curse||web?500:0),"朧の通常攻撃は既存弱体を照合");
        check(PersonalityBonus(battle,skill)==(curse||web?500:0),"朧の技能も既存弱体を照合");
        check(PersonalityBonus(MakePersonalityBattle(4,curse,web),debuff)==(curse?0:500),"紫苑は同じ弱体がある場合を除外");
    }
    check(PersonalityBonus(MakePersonalityBattle(4),skill)==0,"紫苑は弱体なしの攻撃を加点しない");
    auto edge=MakePersonalityBattle(5).Checkpoint();edge.State.Units[2].At={4,1};
    check(PersonalityBonus(*Battle::Restore(edge),attack)==0,"対象座標の空きマスは加点しない");
    check(PersonalityBonus(*Battle::Restore(edge),{BasicActionKind::Attack,{4,1}})==500,"楓の通常射程端");
    check(PersonalityBonus(MakePersonalityBattle(5),attack)==0,"楓は射程端未満を加点しない");
    edge.State.Units[2].At={6,1};
    check(PersonalityBonus(*Battle::Restore(edge),{BasicActionKind::UseSkill,{6,1},Skill::PowerStrike})==500,"楓は技能の固有射程端を使う");
    check(PersonalityBonus(MakePersonalityBattle(5),skill)==0,"楓の技能射程端未満");
    check(PersonalityBonus(MakePersonalityBattle(6),skill)==500,"火乃香は攻撃技能を優先");
    check(PersonalityBonus(MakePersonalityBattle(6),debuff)==500,"火乃香は弱体付き攻撃も一回だけ加点");
    check(PersonalityBonus(MakePersonalityBattle(6),attack)==0,"火乃香の通常攻撃は加点なし");
    check(PersonalityBonus(MakePersonalityBattle(7),{BasicActionKind::UseSkill,{1,1},Skill::FirstAid})==500,"千景は自身の防御強化を優先");
    check(PersonalityBonus(MakePersonalityBattle(7),buff)==0,"千景は他者への不正な自己強化を加点しない");
    check(PersonalityBonus(MakePersonalityBattle(7),skill)==0,"千景の攻撃技能は加点なし");
    for(bool curse:{false,true})for(bool web:{false,true}){
        check(PersonalityBonus(MakePersonalityBattle(8,curse,web),heal)==(curse||web?500:0),"小夜は満タンでも実際の解除を優先");
        check(PersonalityBonus(MakePersonalityBattle(8,curse,web,10),buff)==0,"小夜は解除のない強化を加点しない");
    }
    check(PersonalityBonus(MakePersonalityBattle(8,false,false,10),heal)==0,"小夜は回復だけの場合に解除加点しない");
    for(int id:{1,2,3,5,6}){
        auto generic=MakePersonalityBattle(id,id==3,false,10).Checkpoint();generic.State.Units[0].Skills.fill(true);
        if(id==5)generic.State.Units[2].At={6,1};
        const auto battle=*Battle::Restore(generic);
        const BasicAction action=id==2?BasicAction{BasicActionKind::UseSkill,{1,2},Skill::Heal}:
            id==5?BasicAction{BasicActionKind::UseSkill,{6,1},Skill::FarStrike}:
            BasicAction{BasicActionKind::UseSkill,{3,1},Skill::Sweep};
        check(PersonalityBonus(battle,action)==500,"習得済みの共通技能も既存候補として人物条件で加点");
    }
    for(int id=1;id<=8;++id){
        const auto battle=MakePersonalityBattle(id,true,true,10);const auto before=battle.Checkpoint();
        for(const BasicAction action:std::vector<BasicAction>{{},{BasicActionKind::Move,{2,1}},{BasicActionKind::Attack,{1,2}},{BasicActionKind::UseSkill,{3,1},static_cast<Skill>(99)},{static_cast<BasicActionKind>(99),{3,1}}})
            check(PersonalityBonus(battle,action)==0,"移動/待機/不正行動/味方攻撃を加点しない");
        (void)PersonalityBonus(battle,skill);check(battle.Checkpoint()==before,"判定は状態と乱数を変更しない");
        auto unavailable=before;unavailable.State.Units[0].Cooldowns[0]=1;
        check(PersonalityBonus(*Battle::Restore(unavailable),skill)==0,"CD中は加点しない");
        unavailable=before;unavailable.State.Acted=true;
        check(PersonalityBonus(*Battle::Restore(unavailable),attack)==0,"行動済みは加点しない");
        unavailable=before;unavailable.State.AbilityRules=AbilityRuleSet::Legacy;
        check(PersonalityBonus(*Battle::Restore(unavailable),skill)==0,"旧技能の意味を変えない");
    }
    auto scene=MakePersonalityBattle(3).Checkpoint();
    scene.State.Units[0].Skills={};scene.State.Units[2].AttackCursed=true;
    auto second=scene.State.Units[2];second.Id=101;second.At={1,3};second.AttackCursed=false;
    scene.State.Units.push_back(second);scene.State.Order.push_back(101);
    const auto old=*Battle::Restore(scene);scene.State.PersonalityAI=true;const auto fresh=*Battle::Restore(scene);
    const IntentOrder escort{TacticalIntent::Escort,{},{}};
    const auto oldRank=RankIntentActions(old,escort),newRank=RankIntentActions(fresh,escort);
    check(oldRank.front().Target==Position{1,3},"旧順位は護衛相手に近い敵");
    check(newRank.front().Target==Position{3,1},"新規順位は朧の弱体狙いが500点で逆転");
    for(int trust:{0,24,25,49,50,74,75,100}){
        const auto f=IntentForecast::Build(fresh,newRank,trust),g=IntentForecast::Build(old,oldRank,trust);
        check(f.Candidates().size()==g.Candidates().size(),"加点で信頼別候補数を変えない");
        for(std::size_t i=0;i<f.Candidates().size();++i)check(f.Candidates()[i].Probability==g.Candidates()[i].Probability,"信頼別確率/非表示を維持");
        auto a=fresh,b=old;Random ra(42),rb(42);
        check(f.Execute(a,ra) && g.Execute(b,rb) && ra.State()==rb.State(),"性格で抽選数を増やさない");
    }
    check(RankIntentActions(fresh,{TacticalIntent::Assault,101,{}}).front().Target==Position{1,3},"指定敵を性格より優先");
    auto tied=scene;tied.State.Units[2].AttackCursed=false;
    check(RankIntentActions(*Battle::Restore(tied),escort)==oldRank,"加点なしでは従来同点順を維持");
    auto danger=scene;danger.State.Units[0].Hp=1;danger.State.Tiles[2+8].Trap=true;
    danger.State.Units[2].Attack=100;danger.State.Units[3].Attack=100;
    const auto dangerous=*Battle::Restore(danger);
    const auto dangerousRank=RankIntentActions(dangerous,{TacticalIntent::Assault,{},Position{7,7}});
    const BasicAction lethalMove{BasicActionKind::Move,{2,1}};
    check(std::find(dangerousRank.begin(),dangerousRank.end(),lethalMove)!=dangerousRank.end(),"除外前の順位には致死罠への移動がある");
    const auto safe=IntentForecast::Build(dangerous,dangerousRank,0);
    check(std::none_of(safe.Candidates().begin(),safe.Candidates().end(),[&](const auto& candidate){return candidate.Action==lethalMove;}),"性格有効でも致死罠移動を候補から除外");
    check(!safe.Candidates().empty(),"性格有効でも安全な候補へ退避");
    for(const auto& candidate:safe.Candidates())if(candidate.Action.Kind==BasicActionKind::Move){
        const auto movement=dangerous.PreviewMove(3,candidate.Action.Target);auto projected=dangerous;ApplyBasicAction(projected,candidate.Action);
        check(!movement.KnockedOut && movement.Damage+projected.PotentialEnemyDamage(3)<1,"加点の適用後も罠と次手番の自滅を除外");
    }
    auto heroTurn=old;heroTurn.EndTurn(heroTurn.Snapshot().Revision,3,Facing::South);
    check(PersonalityBonus(heroTurn,attack)==0,"主人公に性格加点しない");
    heroTurn.EndTurn(heroTurn.Snapshot().Revision,0,Facing::South);
    check(PersonalityBonus(heroTurn,attack)==0,"敵に仲間の性格加点を適用しない");
    BattleUnit survivor;survivor.Id=1;survivor.At={1,1};auto defeated=survivor;defeated.Id=100;defeated.Side=Faction::Enemy;defeated.At={3,1};defeated.Hp=0;
    auto ended=Battle(8,8,std::vector<Tile>(64),{survivor,defeated},42).Checkpoint();ended.State.AbilityRules=AbilityRuleSet::CompleteCharacters;
    check(PersonalityBonus(*Battle::Restore(ended),attack)==0,"戦闘終了後は加点しない");
    const auto path=std::filesystem::path(u8"" __FILE__).parent_path().parent_path()/"Fixtures"/"personality-v16.bin";
    std::ifstream input(path,std::ios::binary);const std::string bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
    auto expected=TestData::AbilityContextCase(true);expected.Combat->Battle.State.AbilityRules=AbilityRuleSet::CompleteCharacters;expected.Combat->Battle.State.Combos.Gauge=50;
    const auto decoded=DecodeRunPayload(bytes);
    check(bytes.size()==3418 && static_cast<unsigned char>(bytes[4])==16 && TestData::RosterPayloadHash(bytes)==UINT64_C(1940897666974691934) && decoded==expected,"固定v16の全状態と旧性格無効を保持");
    if(decoded){const auto encoded=EncodeRunPayload(*decoded);check(encoded && DecodeRunPayload(*encoded)==decoded,"旧戦闘を現行形式で保存しても性格無効を保持");
        if(!encoded)return failed;
        auto before=*Battle::Restore(decoded->Combat->Battle),after=*Battle::Restore(DecodeRunPayload(*encoded)->Combat->Battle);
        before.EndTurn(before.Snapshot().Revision,before.Snapshot().ActiveId,Facing::South);after.EndTurn(after.Snapshot().Revision,after.Snapshot().ActiveId,Facing::South);
        for(int kind=0;kind<4;++kind)check(RankIntentActions(before,{static_cast<TacticalIntent>(kind),{},{}})==RankIntentActions(after,{static_cast<TacticalIntent>(kind),{},{}}),"固定v16の次手番も全作戦の順位を維持");
    }
    expected.Combat->Battle.State.PersonalityAI=true;
    const auto encoded=EncodeRunPayload(expected);
    check(encoded && DecodeRunPayload(*encoded)==expected,"新規性格識別を保存往復");
    check(RunPayloadSchema==17,"新識別をRun v17へ分離");
    for(auto rules:{AbilityRuleSet::Legacy,AbilityRuleSet::Character,AbilityRuleSet::CharacterSkills,AbilityRuleSet::CharacterAbilities}){
        auto invalid=scene;invalid.State.AbilityRules=rules;check(!Battle::Restore(invalid),"旧効果と新性格の不正な混在を拒否");
    }
    std::cout<<"Personality: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
