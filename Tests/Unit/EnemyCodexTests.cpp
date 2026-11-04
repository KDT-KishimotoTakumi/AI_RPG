#include "Domain/EnemyCatalog.h"
#include "Infrastructure/SavePayload.h"
#include "../Fixtures/EnemyEncounterFingerprint.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace GirlsSpire;
int TestEnemyCodex()
{
    int checks=0,failed=0;const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL EnemyCodex: "<<why<<'\n';}};
    check(EnemyCatalog().size()==12,"既存12敵種の共通定義");
    const BattleRole roles[]={BattleRole::Standard,BattleRole::Archer,BattleRole::Gunner,BattleRole::Cavalry,BattleRole::Ninja,BattleRole::Oni,BattleRole::CurseMonk,BattleRole::Fox,BattleRole::Spider,BattleRole::Standard,BattleRole::FallenPrincess,BattleRole::CastleLord};
    const int hp[]={22,22,22,22,22,22,22,22,22,35,160,240};
    const int attack[]={10,10,10,10,10,16,10,8,10,10,18,22};
    const int movement[]={3,3,2,5,4,2,2,3,2,3,2,2};
    const int range[]={1,4,5,1,1,1,3,3,3,1,2,3};
    std::vector<BattleUnit> encountered;
    for(int id=1;id<=12;++id){
        const auto definition=FindEnemyDefinition(id);check(definition && !definition->Name.empty() && !definition->Description.empty(),"名称と固有行動を共有定義から取得");
        auto enemy=CreateEnemy(static_cast<EnemyKind>(id),100+id,{1,2});
        check(enemy.Id==100+id && enemy.At==Position{1,2} && enemy.Side==Faction::Enemy && enemy.Role==roles[id-1] && enemy.Hp==hp[id-1] && enemy.MaxHp==hp[id-1] && enemy.Attack==attack[id-1] && enemy.Movement==movement[id-1] && enemy.Range==range[id-1],"既存敵の能力/位置指定を維持");
        BattleUnit seen;seen.Id=100+id;seen.Side=Faction::Enemy;seen.Role=roles[id-1];seen.CreatesCover=id==10;encountered.push_back(seen);
        check(EnemyKindOf(seen)==static_cast<EnemyKind>(id),"役割から安定した敵種IDへ対応");
        seen.Id=1;check(!EnemyKindOf(seen),"未救出人物を敵種へ重複登録しない");
        seen.Id=101;seen.Side=Faction::Player;check(!EnemyKindOf(seen),"味方を発見対象にしない");
    }
    for(int id:{-1,0,13,9999}){check(!FindEnemyDefinition(id),"未登録の図鑑IDは参照不可");try{(void)CreateEnemy(static_cast<EnemyKind>(id),100,{});check(false,"不正な敵種を生成拒否");}catch(const std::invalid_argument&){check(true,"不正な敵種を生成拒否");}}
    Profile profile;profile.Bonds.fill(42);profile.HighestDifficulty=1;const auto prior=profile;
    check(DiscoverEnemies(profile,encountered) && profile.DiscoveredEnemies.size()==12,"遭遇した全種を登録");
    auto expected=prior;for(int i=1;i<=12;++i)expected.DiscoveredEnemies.push_back(i);
    check(profile==expected,"発見以外の恒久記録は不変");
    std::reverse(encountered.begin(),encountered.end());check(DiscoverEnemies(profile,encountered) && profile==expected,"重複/並び順で増殖せず整列");
    check(DiscoverEnemies(profile,{}) && profile==expected,"敵なしでも既存記録は保持");
    for(auto ids:{std::vector<int>{0},std::vector<int>{13},std::vector<int>{2,1},std::vector<int>{1,1}}){auto bad=prior;bad.DiscoveredEnemies=ids;const auto before=bad;check(!ValidProfile(bad) && !DiscoverEnemies(bad,encountered) && bad==before && !EncodeProfilePayload(bad),"不正な敵発見記録は保存/更新を拒否し不変");}
    check(ProfilePayloadSchema==6,"敵図鑑は恒久v6");
    const auto bytes=EncodeProfilePayload(profile);check(bytes && DecodeProfilePayload(*bytes)==profile,"発見済み全種の保存往復");
    const auto hash=TestData::EnemyEncounterHash();check(hash && *hash==UINT64_C(9375212860804229285),"生成1200戦場の全保存バイトは抽出前と同じ");
    for(int highest=0;highest<=2;++highest){
        auto old=prior;old.HighestDifficulty=highest;old.DiscoveredCombos={2,201};old.SpecialConversations[3]=true;old.Epilogues[5]=true;
        auto v5=EncodeProfilePayload(old);if(!v5){check(false,"旧形式の架空データを準備");continue;}
        v5->resize(v5->size()-4);(*v5)[4]=5;
        check(InspectPayload(*v5,SaveKind::Profile)==SaveError::None && DecodeProfilePayload(*v5)==old,"v5は既存解放/絆/交流/後日談/連携を保持し敵図鑑だけ空");
        for(std::size_t length=0;length<v5->size();++length)check(!DecodeProfilePayload(v5->substr(0,length)),"切断したv5は受理しない");
        check(!DecodeProfilePayload(*v5+"x"),"v5の余剰を拒否");
        auto bad=*v5;bad[bad.size()-4]=3;check(!DecodeProfilePayload(bad),"v5移行でも不正な難易度を拒否");
    }
    if(bytes){for(int id:{0,11,13}){auto bad=*bytes;bad[bad.size()-4]=static_cast<char>(id);check(!DecodeProfilePayload(bad),"v6の不正/重複した敵発見IDを読込拒否");}}
    for(int id:{-1,0,99,10000}){try{(void)CreateEnemy(EnemyKind::Footman,id,{});check(false,"不正な戦闘人物IDを生成拒否");}catch(const std::invalid_argument&){check(true,"不正な戦闘人物IDを生成拒否");}}
    auto unknown=encountered.front();unknown.Role=BattleRole::Dancer;unknown.CreatesCover=false;check(!EnemyKindOf(unknown),"未登録の役割は敵種を推測しない");
    Run retained(42,Difficulty::Beginner,HeroRole::Sword,expected);check(retained.DiscoverEnemies(encountered) && retained.ProfileState()==expected,"周回サービスでも発見は重複しない");retained.ChooseNode(retained.Snapshot().Revision,0);retained.CompleteEvent(retained.Snapshot().Revision,true);
    check(retained.ProfileState()==expected,"全滅でも敵図鑑を恒久保持");
    auto resumed=Run::Restore(retained.Checkpoint(),expected);check(resumed && resumed->ProfileState()==expected,"敗北保存の再開でも発見を保持");
    std::cout<<"EnemyCodex: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
