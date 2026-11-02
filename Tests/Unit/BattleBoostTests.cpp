#include "Domain/Battle.h"
#include "../Fixtures/AbilityContextCases.h"
#include "../Fixtures/RosterBaseline.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
using namespace GirlsSpire;
int TestBattleBoost()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL BattleBoost: "<<why<<'\n';}};
    BattleUnit hero;hero.At={1,1};hero.Attack=20;hero.Accuracy=60;hero.Speed=20;hero.Skills={true,true};hero.Hp=hero.MaxHp=100;
    auto enemy=hero;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={2,1};enemy.Speed=10;enemy.Defense=3;enemy.Evasion=0;
    Battle initial(8,8,std::vector<Tile>(64),{hero,enemy},42);
    auto checkpoint=initial.Checkpoint();checkpoint.State.AbilityRules=AbilityRuleSet::Character;checkpoint.State.Combos.Gauge=50;
    checkpoint.State.Units[0].AttackBoost={6,true};checkpoint.State.Units[0].AccuracyBoost={15,true};checkpoint.State.Units[1].DefenseBoost={4,false};
    auto boosted=Battle::Restore(checkpoint);check(boosted.has_value(),"有効な正式強化状態を復元");
    if(boosted){
        const auto before=boosted->Checkpoint();const auto hit=boosted->PreviewAttack(0,enemy.At);
        check(hit.Legal && hit.Hits[0].Damage==19 && hit.Hits[0].Chance==75,"通常攻撃で攻撃6/防御4/命中15を反映");
        const auto skill=boosted->PreviewSkill(0,Skill::PowerStrike,enemy.At);const auto ultimate=boosted->PreviewUltimate(0,enemy.At);
        check(skill.Legal && skill.Attack.Hits[0].Damage==27 && ultimate.Legal && ultimate.Hits[0].Damage==39,"技能/個人必殺も共通の強化計算を利用");
        check(boosted->Checkpoint()==before,"予告で全状態/期限/乱数不変");
        auto cursed=checkpoint;cursed.State.Units[0].AttackCursed=true;
        const auto weakened=Battle::Restore(cursed);check(weakened && weakened->PreviewAttack(0,enemy.At).Hits[0].Damage==15,"攻撃低下と強化が併存");
        check(boosted->EndTurn(boosted->Snapshot().Revision,0,Facing::South),"自己使用手番を終了");
        check(boosted->Snapshot().Units[0].AttackBoost==TimedBoost{6,false} && boosted->Snapshot().Units[0].AccuracyBoost==TimedBoost{15,false} && boosted->Snapshot().Units[1].DefenseBoost==TimedBoost{4,false},"自己付与は除外し他者の期限は進めない");
        check(boosted->EndTurn(boosted->Snapshot().Revision,100,Facing::North) && boosted->Snapshot().Units[1].DefenseBoost==TimedBoost{},"外部付与は受け手の次手番終了で消える");
        check(boosted->EndTurn(boosted->Snapshot().Revision,0,Facing::South) && boosted->Snapshot().Units[0].AttackBoost==TimedBoost{} && boosted->Snapshot().Units[0].AccuracyBoost==TimedBoost{},"自己付与も次の自分の手番終了で消える");
        auto original=Battle::Restore(checkpoint);check(original && original->Attack(original->Snapshot().Revision,0,enemy.At) && original->LastHits()[0].Damage==(original->LastHits()[0].Hit?19:0),"実行の被害も予告と一致");
    }
    auto safety=checkpoint;safety.State.Units[0].DefenseBoost={4,false};
    const auto expiring=Battle::Restore(safety);
    check(expiring && expiring->PotentialHostileDamage(0)==26,"安全予測は敵の手番より前に失効する防御強化を差し引かない");
    safety.State.Units[0].DefenseBoost={4,true};
    const auto remaining=Battle::Restore(safety);
    check(remaining && remaining->PotentialHostileDamage(0)==22,"使用手番除外で残る防御強化は安全予測へ反映");
    auto enemyTurn=initial;enemyTurn.EndTurn(enemyTurn.Snapshot().Revision,0,Facing::South);
    safety=enemyTurn.Checkpoint();safety.State.AbilityRules=AbilityRuleSet::Character;safety.State.Units[0].DefenseBoost={4,false};
    const auto immediate=Battle::Restore(safety);
    check(immediate && immediate->PotentialHostileDamage(0)==22,"現在の敵の攻撃より後に切れる防御は維持");
    for(int kind=0;kind<3;++kind)for(int value:{-1,kind==0?7:kind==1?5:16,std::numeric_limits<int>::max()}){
        auto bad=checkpoint;auto& unit=bad.State.Units[0];auto& boost=kind==0?unit.AttackBoost:kind==1?unit.DefenseBoost:unit.AccuracyBoost;boost={value,false};
        check(!Battle::Restore(bad),"負数/承認表の最大を超える強化を拒否");
    }
    auto bad=checkpoint;bad.State.Units[0].AttackBoost={0,true};check(!Battle::Restore(bad),"空の自己期限を拒否");
    bad=checkpoint;bad.State.AbilityRules=AbilityRuleSet::Legacy;check(!Battle::Restore(bad),"旧ルールに強化状態を混入できない");
    hero.AttackBoost={6,true};hero.DefenseBoost={4,true};hero.AccuracyBoost={15,true};
    Battle fresh(8,8,std::vector<Tile>(64),{hero,enemy},42);check(fresh.Snapshot().Units[0].AttackBoost==TimedBoost{} && fresh.Snapshot().Units[0].DefenseBoost==TimedBoost{} && fresh.Snapshot().Units[0].AccuracyBoost==TimedBoost{},"新戦闘へ一時強化を持ち越さない");
    auto saved=TestData::AbilityContextCase(true);saved.Combat->Battle.State.Units[0].AttackBoost={4,true};saved.Combat->Battle.State.Units[0].DefenseBoost={2,false};saved.Combat->Battle.State.Units[0].AccuracyBoost={10,true};
    const auto encoded=EncodeRunPayload(saved);check(RunPayloadSchema>=13 && encoded && DecodeRunPayload(*encoded)==saved,"v13で全強化と除外期限を保存");
    if(encoded)for(std::size_t n=0;n<encoded->size();++n)check(!DecodeRunPayload(std::string_view(*encoded).substr(0,n)),"強化の全切断を拒否");
    const auto path=std::filesystem::path(u8"" __FILE__).parent_path().parent_path()/"Fixtures"/"ability-v12.bin";
    std::ifstream input(path,std::ios::binary);const std::string bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
    check(bytes.size()==6549 && bytes.substr(0,5)=="GSA12","固定v12原本のサイズ/署名");
    std::size_t offset=5;const std::uint64_t hashes[]={UINT64_C(4525358339648513907),UINT64_C(4765611127552566987)};
    for(int i=0;i<2 && offset+4<=bytes.size();++i){
        std::uint32_t length=0;for(unsigned j=0;j<4;++j)length|=static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset+j]))<<(8*j);offset+=4;
        if(length>bytes.size()-offset){check(false,"固定v12の境界");break;}
        const std::string_view payload(bytes.data()+offset,length);offset+=length;
        check(TestData::RosterPayloadHash(payload)==hashes[i] && DecodeRunPayload(payload)==TestData::AbilityContextCase(i!=0) && InspectPayload(payload,SaveKind::Run)==SaveError::None,"固定v12の既知ハッシュ/全状態/乱数と空強化補完を維持");
    }
    check(offset==bytes.size(),"固定v12を余剰なく検証");
    std::cout<<"BattleBoost: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
