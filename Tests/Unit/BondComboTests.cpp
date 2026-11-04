#include "Domain/Battle.h"
#include "Domain/ComboCatalog.h"
#include "../Fixtures/AbilityContextCases.h"
#include <fstream>
#include <filesystem>
#include <iterator>
#include <iostream>
using namespace GirlsSpire;
int TestBondCombos()
{
    int checks=0,failed=0;const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL BondCombos: "<<why<<'\n';}};
    const char* names[]={"焔を鍛つ","共鳴の舞","月影の合図","結びの護符","双翼の狙い","火花の呼吸","刃心一如","魂繋ぎの鈴"};
    check(ComboCatalog().size()==20 && BondComboCatalog().size()==8,"既存20と専用8を別に保持");
    for(int id=1;id<=8;++id){
        const auto recipe=FindComboRecipe(200+id);check(recipe && recipe->Name==names[id-1] && recipe->Pattern==std::vector<int>{Continuation,Ending},"承認済み8名称/承→結");
        BattleUnit hero;hero.At={0,1};hero.Hp=hero.MaxHp=100;hero.Attack=10;hero.Accuracy=200;hero.Speed=30;hero.Range=3;
        auto ally=hero;ally.Id=id;ally.At={2,1};ally.Speed=20;auto enemy=hero;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={1,2};enemy.Speed=10;enemy.Evasion=0;enemy.Hp=enemy.MaxHp=1000;
        for(int rank=0;rank<=100;++rank){
            Battle initial(8,8,std::vector<Tile>(64),{hero,ally,enemy},42);auto context=initial.Checkpoint();context.State.AbilityRules=AbilityRuleSet::CompleteCharacters;context.State.StartingBonds[id-1]=rank;
            auto battle=Battle::Restore(context);check(battle.has_value(),"新識別を復元");if(!battle)continue;
            const bool played=battle->Move(battle->Snapshot().Revision,0,{1,1}) && battle->Attack(battle->Snapshot().Revision,0,enemy.At) && battle->EndTurn(battle->Snapshot().Revision,0,Facing::South) && battle->Attack(battle->Snapshot().Revision,id,enemy.At);
            check(played && battle->Snapshot().LastCombo && battle->Snapshot().LastCombo->RecipeId==(rank==100?200+id:2) && battle->Snapshot().LastCombo->GaugeGain==(rank==100?20:10) && battle->Snapshot().Combos.Gauge==(rank==100?20:10),"全絆値で実操作の通常/専用優先と重複加算なし");
            if(rank==100){
                check(battle->Snapshot().LastCombo->Participants==std::vector<int>{0,id},"人物と順序を固定");
                Profile profile;check(DiscoverCombo(profile,200+id) && profile.DiscoveredCombos==std::vector<int>{200+id},"恒久発見へ追加");
                const auto encoded=EncodeProfilePayload(profile);check(encoded && DecodeProfilePayload(*encoded)==profile,"Profile v4へ発見を往復");
            }
        }
        std::vector<BattleUnit> units={hero,ally,enemy};units[0].At={1,1};
        ComboSnapshot history{1,0,2,0,{{1,0,Continuation,{100}},{2,id,Ending,{100}}}};std::array<int,8> bonds{};bonds[id-1]=100;
        check(FindBondComboCandidates(history,units,bonds).size()==1,"承/結の候補を照合");
        for(int variant=0;variant<6;++variant){auto h=history;auto u=units;auto b=bonds;
            if(variant==0)b[id-1]=99;if(variant==1)h.Pending[0].Attributes=Opening;if(variant==2)h.Pending[0].EnemyTargets.clear();if(variant==3)h.Pending[1].EnemyTargets.clear();if(variant==4)u[1].At={4,1};if(variant==5)u[0].Hp=0;
            check(FindBondComboCandidates(h,u,b).empty(),"絆/属性/共通命中/隣接/生存の不成立");
        }
    }
    ComboSnapshot history{1,95,2,0,{{1,0,Continuation,{100}},{2,1,Ending,{100}}}};
    ComboLedger ledger;check(ledger.Restore(history),"上限用履歴");const auto resolution=ledger.Resolve(2,{{2,{1,2}},{201,{1,2},true}});
    check(resolution && resolution->RecipeId==201 && resolution->GaugeGain==5 && ledger.Snapshot().Gauge==100,"専用を優先し上限100で実増分だけを記録");
    const auto path=std::filesystem::path(u8"" __FILE__).parent_path().parent_path()/"Fixtures"/"bond-v15.bin";std::ifstream input(path,std::ios::binary);const std::string bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};auto expected=TestData::AbilityContextCase(true);expected.Combat->Battle.State.AbilityRules=AbilityRuleSet::CharacterAbilities;expected.Combat->Battle.State.Combos.Gauge=50;
    check(bytes.size()==3418 && static_cast<unsigned char>(bytes[4])==15 && TestData::RosterPayloadHash(bytes)==UINT64_C(110507814626026738) && DecodeRunPayload(bytes)==expected,"固定v15は旧20連携識別を維持");
    check(RunPayloadSchema==19 && ProfilePayloadSchema==6,"周回v19/恒久v6");
    std::cout<<"BondCombos: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
