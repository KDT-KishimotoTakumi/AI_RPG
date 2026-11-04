#include "AI/EnemyTactics.h"
#include "../Fixtures/AbilityContextCases.h"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
namespace {
GirlsSpire::Battle EnemyThinkingCase(GirlsSpire::EnemyThinking mode,bool pending=false,bool resolved=false,bool lethal=false)
{
    using namespace GirlsSpire;
    BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={1,1};enemy.Speed=30;enemy.Range=3;
    BattleUnit near;near.At={3,1};near.Hp=near.MaxHp=100;near.Defense=4;if(lethal)near.Hp=8;
    auto far=near;far.Id=1;far.At={1,4};far.Hp=100;far.Defense=0;
    auto checkpoint=Battle(8,8,std::vector<Tile>(64),{enemy,near,far},42).Checkpoint();
    checkpoint.State.AbilityRules=AbilityRuleSet::CompleteCharacters;checkpoint.State.EnemyAI=mode;
    if(pending){checkpoint.State.Revision=1;checkpoint.State.Combos={1,0,1,resolved?1u:0u,{{1,0,Opening,{100}}}};}
    auto battle=Battle::Restore(checkpoint);if(!battle)throw std::logic_error("難易度検証の戦闘が無効です");return *battle;
}
}
int TestEnemyTactics()
{
    using namespace GirlsSpire;int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL EnemyTactics: "<<why<<'\n';}};
    for(auto mode:{EnemyThinking::Legacy,EnemyThinking::Basic,EnemyThinking::Disrupt,EnemyThinking::Tactical}){
        const auto battle=EnemyThinkingCase(mode);const auto before=battle.Checkpoint();
        const auto attack=ChooseRankedAttack(battle);
        check(attack && attack->Action.Target==(mode==EnemyThinking::Basic?Position{3,1}:Position{1,4}),"初級は近い敵、旧/中級/上級は高い期待ダメージ");
        check(battle.Checkpoint()==before,"標的予告は全状態と乱数不変");
        const auto pending=ChooseRankedAttack(EnemyThinkingCase(mode,true));
        check(pending && pending->Action.Target==(mode==EnemyThinking::Legacy?Position{1,4}:Position{3,1}),"中級/上級は未解決連携参加者へ一回だけ妨害加点");
        const auto resolved=ChooseRankedAttack(EnemyThinkingCase(mode,true,true));
        check(resolved && resolved->Action.Target==(mode==EnemyThinking::Basic?Position{3,1}:Position{1,4}),"解決済み履歴へ妨害加点しない");
        const auto lethal=ChooseRankedAttack(EnemyThinkingCase(mode,false,false,true));
        check(lethal && lethal->Action.Target==(mode==EnemyThinking::Tactical || mode==EnemyThinking::Basic?Position{3,1}:Position{1,4}),"上級は戦闘不能となる予測命中率を先に評価");
        check(ChooseBasicAction(battle)==(attack?attack->Action:BasicAction{}),"通常敵の実行選択へ難易度を接続");
    }
    auto order=EnemyThinkingCase(EnemyThinking::Tactical).Checkpoint();order.State.Units[1].Defense=0;
    order.State.Units[2].Speed=20;order.State.Order={100,1,0};
    const auto ordered=ChooseRankedAttack(*Battle::Restore(order));
    check(ordered && ordered->Action.Target==Position{1,4},"同火力は次の手番が早い対象");
    order.State.Units[2].Speed=10;order.State.Order={100,0,1};
    const auto tied=ChooseRankedAttack(*Battle::Restore(order));check(tied && tied->Action.Target==Position{3,1},"同点は行動順、最後は対象ID");
    auto oni=EnemyThinkingCase(EnemyThinking::Basic).Checkpoint();oni.State.Units[0].Role=BattleRole::Oni;oni.State.Units[0].Skills[0]=true;
    const auto strong=ChooseRankedAttack(*Battle::Restore(oni));check(strong && strong->Action.Kind==BasicActionKind::UseSkill,"鬼武者は全難易度で有利な強撃を維持");
    oni.State.Units[0].Cooldowns[0]=1;const auto cooling=ChooseRankedAttack(*Battle::Restore(oni));check(cooling && cooling->Action.Kind==BasicActionKind::Attack,"再使用待ちの強撃を使わない");
    auto blocked=EnemyThinkingCase(EnemyThinking::Basic).Checkpoint();blocked.State.Tiles[2+8].Wall=true;blocked.State.Tiles[1+2*8].Wall=true;
    check(!ChooseRankedAttack(*Battle::Restore(blocked)),"壁越しの攻撃を候補にしない");
    blocked.State.Tiles.assign(64,Tile{});blocked.State.Acted=true;
    check(!ChooseRankedAttack(*Battle::Restore(blocked)),"行動済みは追加攻撃しない");
    auto moving=EnemyThinkingCase(EnemyThinking::Tactical).Checkpoint();moving.State.Units.pop_back();moving.State.Order={100,0};
    moving.State.Units[0].Hp=5;moving.State.Units[1].At={5,1};moving.State.Tiles[2+8].Trap=true;
    const auto battle=*Battle::Restore(moving);const auto before=battle.Checkpoint();
    const auto move=ChooseTacticalMove(battle);
    check(move==BasicAction{BasicActionKind::Move,{3,0}},"上級は致死罠を避け射線の通る位置へ移動、同点はY/X");
    check(ChooseBasicAction(battle)==move,"上級の実行選択へ移動を接続");
    check(battle.Checkpoint()==before,"移動の先読みで状態と乱数を消費しない");
    auto projection=battle;check(ApplyBasicAction(projection,move) && ChooseRankedAttack(projection).has_value(),"移動後は実際に射撃できる");
    for(int i=0;i<64;++i){const Position at{i%8,i/8};if(at!=Position{1,1} && at!=Position{5,1})moving.State.Tiles[i].Trap=true;}
    check(ChooseTacticalMove(*Battle::Restore(moving)).Kind==BasicActionKind::Wait,"安全な接近先もなければ待機");
    auto approach=EnemyThinkingCase(EnemyThinking::Tactical).Checkpoint();approach.State.Units[0].Movement=1;approach.State.Units[0].Range=1;
    approach.State.Units[1].At={5,1};approach.State.Units[2].At={1,5};
    const auto approaching=*Battle::Restore(approach);
    check(ChooseTacticalMove(approaching)==BasicAction{BasicActionKind::Move,{2,1}},"射撃可能位置に届かなければ既存の最短接近");
    for(auto mode:{EnemyThinking::Legacy,EnemyThinking::Basic,EnemyThinking::Disrupt,EnemyThinking::Tactical}){
        auto fox=EnemyThinkingCase(mode).Checkpoint();fox.State.Units[0].Role=BattleRole::Fox;fox.State.Units[0].Hp=10;fox.State.Units[0].Skills[1]=true;
        check(ChooseBasicAction(*Battle::Restore(fox))==BasicAction{BasicActionKind::UseSkill,{1,1},Skill::FirstAid},"妖狐の回復優先は全思考で維持");
        auto ninja=EnemyThinkingCase(mode).Checkpoint();ninja.State.Units[0].Role=BattleRole::Ninja;ninja.State.Acted=true;
        auto old=ninja;old.State.EnemyAI=EnemyThinking::Legacy;
        check(ChooseBasicAction(*Battle::Restore(ninja))==ChooseBasicAction(*Battle::Restore(old)),"骨忍の承認済み退避を全思考で維持");
    }
    auto area=EnemyThinkingCase(EnemyThinking::Disrupt).Checkpoint();area.State.Units[0].Role=BattleRole::FallenPrincess;area.State.Units[0].Hp=15;
    auto extra=area.State.Units[1];extra.Id=2;extra.At={4,1};area.State.Units.push_back(extra);area.State.Order.push_back(2);
    area.State.Revision=2;area.State.Combos={1,0,2,0,{{1,0,Opening,{100}},{2,2,Continuation,{100}}}};
    auto legacyArea=area;legacyArea.State.EnemyAI=EnemyThinking::Legacy;
    const auto bonus=ChooseRankedAttack(*Battle::Restore(area)),base=ChooseRankedAttack(*Battle::Restore(legacyArea));
    check(bonus && base && bonus->Action==base->Action && bonus->Score[1]==base->Score[1]+500,"範囲攻撃で連携参加者2人に当たっても500だけ加点");
    for(int value:{-1,4}){auto invalid=area;invalid.State.EnemyAI=static_cast<EnemyThinking>(value);check(!Battle::Restore(invalid),"未知の敵思考識別を拒否");}
    auto invalid=area;invalid.State.AbilityRules=AbilityRuleSet::Legacy;check(!Battle::Restore(invalid),"旧効果と新敵思考の不正混在を拒否");
    auto player=EnemyThinkingCase(EnemyThinking::Tactical);player.EndTurn(player.Snapshot().Revision,100,Facing::South);
    check(ChooseTacticalMove(player).Kind==BasicActionKind::Wait,"プレイヤーへ敵専用移動を適用しない");
    auto acted=EnemyThinkingCase(EnemyThinking::Tactical).Checkpoint();acted.State.Acted=true;
    check(ChooseTacticalMove(*Battle::Restore(acted)).Kind==BasicActionKind::Wait,"行動済みの通常敵は再移動しない");
    check(ChooseTacticalMove(EnemyThinkingCase(EnemyThinking::Basic)).Kind==BasicActionKind::Wait,"初級へ上級移動を適用しない");
    BattleUnit survivor;auto dead=survivor;dead.Id=100;dead.Side=Faction::Enemy;dead.At={5,5};dead.Hp=0;
    const auto ended=Battle(8,8,std::vector<Tile>(64),{survivor,dead},42);
    check(!ChooseRankedAttack(ended) && ChooseTacticalMove(ended).Kind==BasicActionKind::Wait,"戦闘終了後は先読みしない");
    const auto path=std::filesystem::path(u8"" __FILE__).parent_path().parent_path()/"Fixtures"/"enemy-v17.bin";
    std::ifstream input(path,std::ios::binary);const std::string bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
    auto expected=TestData::AbilityContextCase(true);expected.Combat->Battle.State.AbilityRules=AbilityRuleSet::CompleteCharacters;expected.Combat->Battle.State.Combos.Gauge=50;expected.Combat->Battle.State.PersonalityAI=true;
    const auto decoded=DecodeRunPayload(bytes);
    check(bytes.size()==3419 && TestData::RosterPayloadHash(bytes)==UINT64_C(10028712118614236382) && decoded==expected,"固定v17の性格有効と旧敵思考を保持");
    if(decoded){const auto encoded=EncodeRunPayload(*decoded);check(encoded && DecodeRunPayload(*encoded)==decoded,"旧敵思考を現行形式でも維持");}
    expected.Combat->Battle.State.EnemyAI=EnemyThinking::Tactical;
    check(!EncodeRunPayload(expected),"周回と敵思考の難易度不一致を拒否");
    expected.Run.State.Level=Difficulty::Advanced;
    const auto encoded=EncodeRunPayload(expected);check(encoded && DecodeRunPayload(*encoded)==expected,"新難易度思考を保存往復");
    check(RunPayloadSchema>=18,"敵思考の識別をRun v18以降へ分離");
    std::cout<<"EnemyTactics: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
