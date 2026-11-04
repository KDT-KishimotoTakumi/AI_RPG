#include "AI/BasicTurn.h"
#include "AI/Intent.h"
#include "AI/EnemyCharacterSkills.h"
#include "Domain/CharacterCatalog.h"
#include "Domain/AbilityCatalog.h"
#include "../Fixtures/AbilityContextCases.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
namespace {
GirlsSpire::Battle EnemyCharacterArena(int id,bool enabled=true,GirlsSpire::Position target={2,1})
{
    using namespace GirlsSpire;
    auto actor=CreateCharacter(id,{1,1});actor.Side=Faction::Enemy;actor.Hp=actor.MaxHp=100;actor.Speed=40;
    BattleUnit victim;victim.At=target;victim.Hp=victim.MaxHp=100;victim.Speed=20;
    auto ally=victim;ally.Id=100;ally.Side=Faction::Enemy;ally.At={1,2};ally.Attack=30;ally.Speed=10;
    Battle base(8,8,std::vector<Tile>(64),{actor,victim,ally},42);
    auto checkpoint=base.Checkpoint();checkpoint.State.AbilityRules=AbilityRuleSet::CompleteCharacters;
    checkpoint.State.EnemyAI=EnemyThinking::Basic;checkpoint.State.EnemyCharacterSkills=enabled;
    return *Battle::Restore(checkpoint);
}
GirlsSpire::Battle EnemySupportArena(int id)
{
    using namespace GirlsSpire;
    BattleUnit hero;hero.At={1,1};hero.Speed=40;hero.Hp=hero.MaxHp=100;
    auto caster=CreateCharacter(id,{4,3});caster.Side=Faction::Enemy;caster.Attack=0;caster.Movement=0;caster.Speed=30;
    BattleUnit attacker;attacker.Id=100;attacker.Side=Faction::Enemy;attacker.At={4,1};attacker.Range=3;attacker.Movement=0;attacker.Speed=20;attacker.Hp=attacker.MaxHp=100;
    Battle base(8,8,std::vector<Tile>(64),{hero,caster,attacker},42);auto checkpoint=base.Checkpoint();
    checkpoint.State.AbilityRules=AbilityRuleSet::CompleteCharacters;checkpoint.State.EnemyAI=EnemyThinking::Basic;checkpoint.State.EnemyCharacterSkills=true;
    return *Battle::Restore(checkpoint);
}
}
int TestEnemyCharacterSkills()
{
    using namespace GirlsSpire;int checks=0,failed=0;
    const auto check=[&](bool pass,const char* message){++checks;if(!pass){++failed;std::cerr<<"FAIL EnemyCharacterSkills: "<<message<<'\n';}};
    for(const auto mode:{EnemyThinking::Basic,EnemyThinking::Disrupt,EnemyThinking::Tactical})for(int id:{1,3,4,5,6,7}){
        auto checkpoint=EnemyCharacterArena(id).Checkpoint();checkpoint.State.EnemyAI=mode;
        const auto battle=Battle::Restore(checkpoint);const auto before=battle->Checkpoint();
        const auto attack=ChooseBasicAction(*battle);
        check(attack.Kind==BasicActionKind::UseSkill && attack.Ability==Skill::PowerStrike && attack.Target==Position{2,1},"全難易度の攻撃役は高い期待被害の正式技能を使う");
        check(battle->Checkpoint()==before,"技能判断は戦闘状態と乱数を変更しない");
        checkpoint.State.Units[0].Cooldowns[0]=1;
        const auto waiting=Battle::Restore(checkpoint);const auto next=ChooseBasicAction(*waiting);
        const bool weakener=id==1 || id==3 || id==4 || id==6;
        check(next.Kind==(weakener?BasicActionKind::UseSkill:BasicActionKind::Attack) && (!weakener || next.Ability==Skill::FirstAid),"同威力では新しい弱体を優先し強化より攻撃を優先");
        if(weakener){
            checkpoint.State.Units[1].AttackCursed=id==4;checkpoint.State.Units[1].MovementWebbed=id!=4;
            const auto already=Battle::Restore(checkpoint);
            check(ChooseBasicAction(*already).Kind==BasicActionKind::Attack,"付与済み弱体の更新だけなら通常攻撃を優先");
        }
        const auto old=EnemyCharacterArena(id,false);
        check(ChooseBasicAction(old).Kind==BasicActionKind::Attack,"旧識別の敵は正式技能へ途中変更しない");
    }
    const auto distant=EnemyCharacterArena(3,true,{4,1});
    const auto ranged=ChooseBasicAction(distant);
    check(ranged.Kind==BasicActionKind::UseSkill && ranged.Ability==Skill::FirstAid,"通常攻撃射程外でも影縫いが合法なら選択する");
    for(int id:{2,8})for(int hp:{50,51}){
        auto checkpoint=EnemyCharacterArena(id).Checkpoint();checkpoint.State.Units[2].Hp=hp;
        const auto battle=Battle::Restore(checkpoint);const auto action=ChooseBasicAction(*battle);
        check(action.Kind==(hp==50?BasicActionKind::UseSkill:BasicActionKind::Attack) && (hp!=50 || (action.Ability==Skill::PowerStrike && action.Target==Position{1,2})),"回復役はHP50%以下を攻撃より優先");
        if(hp==50){auto healed=*battle;check(ApplyBasicAction(healed,action) && healed.Snapshot().Units[2].Hp==hp+(id==2?20:12),"回復は承認済みの人物技能効果で実行する");}
    }
    auto cleansing=EnemyCharacterArena(8).Checkpoint();cleansing.State.Units[2].AttackCursed=true;
    const auto cleanse=ChooseBasicAction(*Battle::Restore(cleansing));
    check(cleanse.Kind==BasicActionKind::UseSkill && cleanse.Ability==Skill::PowerStrike && cleanse.Target==Position{1,2},"小夜は満タンでも実効のある低下解除を優先する");
    auto same=EnemyCharacterArena(2).Checkpoint();same.State.Units[0].Hp=40;same.State.Units[2].Hp=40;
    check(ChooseBasicAction(*Battle::Restore(same)).Target==Position{1,1},"同じHP割合は低い人物IDで回復対象を固定する");
    for(int id:{2,5,7,8}){
        const auto battle=EnemyCharacterArena(id,true,{7,7});const auto action=ChooseBasicAction(battle);
        const auto target=id==5 || id==7?Position{1,1}:Position{1,2};
        check(action.Kind==BasicActionKind::UseSkill && action.Ability==Skill::FirstAid && action.Target==target,"攻撃不能時だけ自身または高攻撃力の味方を強化する");
        auto boosted=battle;check(ApplyBasicAction(boosted,action),"準備の強化も共通の技能実行を通す");
        auto checkpoint=battle.Checkpoint();for(auto& unit:checkpoint.State.Units)if(unit.Side==Faction::Enemy){unit.AttackBoost={4,false};unit.DefenseBoost={4,false};unit.AccuracyBoost={15,false};}
        const auto already=Battle::Restore(checkpoint);const auto next=ChooseBasicAction(*already);
        check(next.Kind!=BasicActionKind::UseSkill,"同じ値以上の強化を繰り返さず既存移動へ戻る");
    }
    auto invalid=EnemyCharacterArena(1).Checkpoint();invalid.State.AbilityRules=AbilityRuleSet::Legacy;invalid.State.EnemyAI=EnemyThinking::Legacy;
    check(!Battle::Restore(invalid),"旧効果に新しい人物技能AIを混ぜない");
    {
        const auto playerTurn=EnemySupportArena(2);
        check(!UsesCharacterEnemyAI(playerTurn) && !ChooseCharacterRecovery(playerTurn) && !ChooseCharacterPreparation(playerTurn) && !ChooseCharacterAttack(playerTurn,{4,1}),"人物技能AIをプレイヤー手番へ適用しない");
        auto generic=playerTurn.Checkpoint();generic.State.ActiveId=100;generic.TurnIndex=2;
        check(!UsesCharacterEnemyAI(*Battle::Restore(generic)),"通常10敵とボスへ人物技能の選択を追加しない");
        auto done=EnemyCharacterArena(1).Checkpoint();done.State.Acted=true;
        const auto acted=Battle::Restore(done);
        check(!ChooseCharacterAttack(*acted,{2,1}) && !ChooseCharacterRecovery(*acted) && !ChooseCharacterPreparation(*acted),"行動後に技能を再使用しない");
        auto ended=playerTurn.Checkpoint();for(auto& unit:ended.State.Units)if(unit.Side==Faction::Enemy)unit.Hp=0;
        ended.State.Outcome=BattleOutcome::Won;ended.State.ActiveId=-1;
        check(!UsesCharacterEnemyAI(*Battle::Restore(ended)),"戦闘終了後に判断しない");
        auto reverse=same;std::reverse(reverse.State.Units.begin(),reverse.State.Units.end());
        check(ChooseBasicAction(*Battle::Restore(reverse)).Target==Position{1,1},"配列順を逆転しても回復対象の同点処理は不変");
        auto blocked=EnemyCharacterArena(2).Checkpoint();blocked.State.Units[2].At={1,3};blocked.State.Units[2].Hp=40;blocked.State.Tiles[2*8+1].Wall=true;
        check(ChooseBasicAction(*Battle::Restore(blocked)).Kind==BasicActionKind::Attack,"壁越しには回復しない");
        blocked.State.Tiles[2*8+1].Wall=false;blocked.State.Units[0].Cooldowns[0]=1;
        check(ChooseBasicAction(*Battle::Restore(blocked)).Kind==BasicActionKind::Attack,"回復がCD中なら攻撃へ戻る");
        blocked.State.Units[0].Cooldowns[0]=0;blocked.State.Units[2].At={1,5};
        check(ChooseBasicAction(*Battle::Restore(blocked)).Kind==BasicActionKind::Attack,"回復射程外なら攻撃へ戻る");
        auto retreat=EnemyCharacterArena(3,true,{4,1});const auto shadow=ChooseBasicAction(retreat);
        check(ApplyBasicAction(retreat,shadow) && ChooseBasicAction(retreat).Kind==BasicActionKind::Move,"影縫いを旧応急手当と誤認せず忍の攻撃後退避を維持");
        auto tactical=distant.Checkpoint();tactical.State.EnemyAI=EnemyThinking::Tactical;tactical.State.Units[0].Cooldowns[1]=1;
        const auto approaching=Battle::Restore(tactical);const auto step=ChooseBasicAction(*approaching);
        check(step.Kind==BasicActionKind::Move && step.Target==Position{3,1},"上級の接近先も正式技能の攻撃候補で評価する");
        auto advanced=*approaching;check(ApplyBasicAction(advanced,step) && ChooseBasicAction(advanced).Kind==BasicActionKind::UseSkill,"接近後も同じ技能比較で判断する");
        auto shot=distant.Checkpoint();shot.State.Tiles[1*8+2].Wall=true;
        check(!ChooseCharacterAttack(*Battle::Restore(shot),{4,1}),"正式攻撃技能も壁越しに撃たない");
        auto overkill=EnemyCharacterArena(7).Checkpoint();overkill.State.Units[1].Hp=1;
        check(ChooseBasicAction(*Battle::Restore(overkill)).Kind==BasicActionKind::Attack,"同じ撃破期待値なら強い技能のCDを浪費しない");
        auto tied=EnemyCharacterArena(2,true,{7,7}).Checkpoint();tied.State.Units[2].Attack=12;
        check(ChooseBasicAction(*Battle::Restore(tied)).Target==Position{1,1},"同じ攻撃力の強化対象は人物IDで固定する");
        tied.State.Units[0].Attack=0;tied.State.Units[0].AttackCursed=true;tied.State.Units[2].Attack=0;
        check(ChooseBasicAction(*Battle::Restore(tied)).Target==Position{1,1},"低下後の実効攻撃力は0で比較する");
        for(const auto mode:{EnemyThinking::Basic,EnemyThinking::Disrupt,EnemyThinking::Tactical}){
            auto selection=EnemyCharacterArena(6).Checkpoint();selection.State.EnemyAI=mode;selection.State.Units[1].Defense=1000;
            selection.State.Units[2].Id=7;selection.State.Units[2].Side=Faction::Player;selection.State.Units[2].At={4,1};selection.State.Units[2].Defense=0;selection.State.Order={6,0,7};
            const auto action=ChooseBasicAction(*Battle::Restore(selection));
            check(action.Target==(mode==EnemyThinking::Basic?Position{2,1}:Position{4,1}),"技能を比較しても難易度別の標的優先は維持");
        }
    }
    {
        const auto supported=EnemySupportArena(2);const auto before=supported.Checkpoint();
        auto actual=supported;actual.EndTurn(actual.Snapshot().Revision,0,Facing::South);
        const auto preparation=ChooseBasicAction(actual);
        check(preparation.Kind==BasicActionKind::UseSkill && preparation.Target==Position{4,1} && ApplyBasicAction(actual,preparation),"敵の舞姫は攻撃できないとき後続の敵を強化する");
        actual.EndTurn(actual.Snapshot().Revision,2,Facing::South);
        const auto attack=actual.PreviewAttack(100,{1,1});
        check(attack.Legal && attack.Hits.front().Damage==14,"強化後の実際の攻撃予告は14になる");
        check(supported.PotentialEnemyDamage(0)==14 && supported.Checkpoint()==before,"先に付与可能な敵強化を安全上限へ含め状態/RNG不変");
        auto pending=before;pending.State.Units[1].Cooldowns[1]=1;
        check(Battle::Restore(pending)->PotentialEnemyDamage(0)==10,"CD中の強化は次の敵手番に仮定しない");
        auto later=before;later.State.Units[1].Speed=10;later.State.Units[2].Speed=30;later.State.Order={0,100,2};
        check(Battle::Restore(later)->PotentialEnemyDamage(0)==10,"攻撃者の後に動く支援は先に適用しない");
        auto oldSupport=before;oldSupport.State.EnemyCharacterSkills=false;
        check(Battle::Restore(oldSupport)->PotentialEnemyDamage(0)==10,"旧識別の戦闘へ支援の仮定を追加しない");
        auto fragile=before;fragile.State.Units[0].Hp=13;
        const auto forecast=IntentForecast::Build(*Battle::Restore(fragile),{{BasicActionKind::Move,{2,1}}},100);
        check(forecast.Fallback()==IntentFallback::NoSafeCandidate,"敵強化後の被害で倒れる移動を除外する");
        auto clean=EnemySupportArena(8).Checkpoint();clean.State.Units[2].AttackCursed=true;
        check(Battle::Restore(clean)->PotentialEnemyDamage(0)==10,"先に可能な攻撃低下解除を安全予測へ含める");
        clean.State.Units[2].AttackCursed=false;clean.State.Units[2].MovementWebbed=true;clean.State.Units[2].Movement=2;clean.State.Units[2].Range=1;
        check(Battle::Restore(clean)->PotentialEnemyDamage(0)==10,"先に移動低下を解除して接近できる敵も含める");
        auto self=EnemyCharacterArena(2).Checkpoint();self.State.Units[2].Hp=0;
        check(Battle::Restore(self)->PotentialEnemyDamage(0)==10,"自分の強化と攻撃を同じ一手番で二重計上しない");
        auto healedPath=before;healedPath.State.Units[2].Hp=5;healedPath.State.Units[2].Movement=2;healedPath.State.Units[2].Range=1;
        healedPath.State.Tiles[1*8+3].Trap=true;healedPath.State.Tiles[1*8+2].Trap=true;
        const auto healingSource=Battle::Restore(healedPath);
        auto healingActual=*healingSource;healingActual.EndTurn(healingActual.Snapshot().Revision,0,Facing::South);
        const auto heal=ChooseBasicAction(healingActual);
        check(ApplyBasicAction(healingActual,heal) && healingActual.EndTurn(healingActual.Snapshot().Revision,2,Facing::South) && healingActual.Move(healingActual.Snapshot().Revision,100,{2,1}),"先行する回復で罠を越えて攻撃地点まで進める");
        check(healingSource->PotentialEnemyDamage(0)>=10,"回復後に致死罠を越えられる敵の被害を過小評価しない");
    }
    auto saved=TestData::AbilityContextCase(true);saved.Combat->Battle.State.AbilityRules=AbilityRuleSet::CompleteCharacters;
    saved.Combat->Battle.State.EnemyAI=EnemyThinking::Basic;saved.Combat->Battle.State.EnemyCharacterSkills=true;
    const auto encoded=EncodeRunPayload(saved);
    check(encoded && DecodeRunPayload(*encoded)==saved && RunPayloadSchema==19,"新しい人物技能AI識別をRun v19で保存往復する");
    const auto path=std::filesystem::path(u8"" __FILE__).parent_path().parent_path()/"Fixtures"/"enemy-v18.bin";
    std::ifstream input(path,std::ios::binary);const std::string bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
    const auto old=DecodeRunPayload(bytes);
    check(bytes.size()==3423 && TestData::RosterPayloadHash(bytes)==UINT64_C(7282346423887295310) && old && !old->Combat->Battle.State.EnemyCharacterSkills,"固定v18を旧敵技能AIとして読み込む");
    {
        BattleUnit hero;hero.At={1,1};hero.Hp=20;
        auto gunner=CreateCharacter(6,{5,1});gunner.Side=Faction::Enemy;
        const TerrainLayout plain{8,8,std::vector<Tile>(64)};
        check(Battlefield::Validate(plain,{hero,gunner},BattleGoal::Eliminate)==FieldFailure::None,"旧技能では安全な境界の戦場を用意");
        check(Battlefield::Validate(plain,{hero,gunner},BattleGoal::Eliminate,true)==FieldFailure::Unsafe,"正式な装填射撃の22被害を初期安全検査に使う");
        try{(void)Battlefield::Generate(42,{hero,gunner},BattleGoal::Eliminate,0,{},true);check(false,"正式技能で固定戦場も危険なら拒否");}
        catch(const std::invalid_argument&){check(true,"正式技能で固定戦場も危険なら拒否");}
    }
    if(old)for(const auto id:{2,100}){
        auto checkpoint=old->Combat->Battle;checkpoint.State.ActiveId=id;checkpoint.State.Moved=false;checkpoint.State.Acted=false;checkpoint.State.UsedSkill=-1;checkpoint.State.ChargeReady=false;
        checkpoint.TurnIndex=static_cast<std::size_t>(std::find(checkpoint.State.Order.begin(),checkpoint.State.Order.end(),id)-checkpoint.State.Order.begin());
        const auto battle=Battle::Restore(checkpoint);const auto action=ChooseBasicAction(*battle);
        check(action.Kind==(id==2?BasicActionKind::Move:BasicActionKind::Attack) && action.Target==(id==2?Position{7,2}:Position{8,2}),"固定v18の人物2/敵100は記録した行動を維持する");
    }
    std::cout<<"EnemyCharacterSkills: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
