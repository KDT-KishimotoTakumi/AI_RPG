#include "Infrastructure/SavePayload.h"
#include <iostream>
using namespace GirlsSpire;
int TestComboDiscovery()
{
    int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL ComboDiscovery: "<<why<<'\n';}};
    // v3の架空データへv4の発見済みID列を追加した、外部形式からの受け入れ試験。
    std::string v4;
    const auto integer=[&](int n){for(int i=0;i<4;++i)v4.push_back(static_cast<char>((static_cast<unsigned>(n)>>(8*i))&255));};
    integer(2);integer(4);for(int i=0;i<8;++i)integer(50);for(int i=0;i<17;++i)v4.push_back(static_cast<char>(i%2));integer(1);integer(2);
    check(DecodeProfilePayload(v4).has_value(),"恒久データv4の連携発見を読み込める");
    const auto loaded=DecodeProfilePayload(v4);check(loaded && loaded->DiscoveredCombos==std::vector<int>{2},"発見済みIDを維持");
    auto v3=v4.substr(0,v4.size()-8);v3[4]=3;
    const auto old=DecodeProfilePayload(v3);check(old && old->DiscoveredCombos.empty() && old->Bonds[0]==50 && old->SpecialConversations[1] && old->Epilogues[1] && !old->SpecialConversations[0] && !old->Epilogues[0],"旧v3の絆・専用交流・後日談を保持し発見だけ空に移行");
    Profile profile;profile.Bonds.fill(80);profile.SpecialConversations.fill(true);profile.Epilogues.fill(true);
    for(int id:{5,2,3,2,5})check(DiscoverCombo(profile,id),"発見は順序に依存せず重複しない");
    check(profile.DiscoveredCombos==std::vector<int>({2,3,5}),"正規順で一度だけ保持");
    const auto before=profile;check(!DiscoverCombo(profile,0) && !DiscoverCombo(profile,-1) && profile==before,"不正IDは状態不変");
    for(auto ids:std::vector<std::vector<int>>{{2,2},{3,2},{0},{-1}}) {
        auto bad=profile;bad.DiscoveredCombos=ids;const auto unchanged=bad;
        check(!ValidProfile(bad) && !DiscoverCombo(bad,5) && bad==unchanged && !EncodeProfilePayload(bad),"不正な恒久発見リストを拒否");
    }
    const auto encoded=EncodeProfilePayload(profile);check(encoded && DecodeProfilePayload(*encoded)==profile,"全恒久記録と発見を保存復元");
    Run run(42,Difficulty::Beginner,HeroRole::Sword,profile);const auto checkpoint=run.Checkpoint();
    check(run.DiscoverCombo(7) && !run.DiscoverCombo(-1) && run.Checkpoint()==checkpoint,"発見だけで周回や乱数を進めない");
    run.ChooseNode(0,0);run.CompleteBattle(run.Snapshot().Revision,BattleOutcome::Lost);
    check(run.ProfileState().DiscoveredCombos==std::vector<int>({2,3,5,7}),"敗北で発見が消失しない");
    Run next(99,Difficulty::Advanced,HeroRole::Matchlock,run.ProfileState());
    check(next.ProfileState()==run.ProfileState(),"次周開始でも発見と既存恒久要素を保持");
    std::cout<<"ComboDiscovery: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
