#include "ComboCatalog.h"
#include "Battle.h"
#include <algorithm>
#include <cstdlib>
#include <set>
namespace GirlsSpire {
namespace {
const BattleUnit* FindUnit(const std::vector<BattleUnit>& units,int id)
{
    const auto at=std::find_if(units.begin(),units.end(),[id](const BattleUnit& u){return u.Id==id;});
    return at==units.end()?nullptr:&*at;
}
std::vector<int> Common(const ComboAction& a,const ComboAction& b)
{
    std::vector<int> result;
    for(int id:a.EnemyTargets)if(std::find(b.EnemyTargets.begin(),b.EnemyTargets.end(),id)!=b.EnemyTargets.end())result.push_back(id);
    return result;
}
bool Separate(const ComboAction& a,const ComboAction& b) {return !a.EnemyTargets.empty() && !b.EnemyTargets.empty() && Common(a,b).empty();}
int Distance(Position a,Position b) {return std::abs(a.X-b.X)+std::abs(a.Y-b.Y);}
bool Corner(Position a,Position b,Position c)
{
    return (a.X==b.X && a.Y==c.Y && a.Y!=b.Y && a.X!=c.X) || (a.Y==b.Y && a.X==c.X && a.X!=b.X && a.Y!=c.Y);
}
}
bool MatchesComboCondition(int id,const std::vector<const ComboAction*>& actions,const std::vector<BattleUnit>& units)
{
    const auto recipe=FindComboRecipe(id);
    if(!recipe || actions.size()!=recipe->Pattern.size())return false;
    std::vector<Position> positions;std::set<int> actors;
    for(std::size_t i=0;i<actions.size();++i) {
        const auto action=actions[i];if(!action || !(action->Attributes&recipe->Pattern[i]))return false;
        const auto actor=FindUnit(units,action->ActorId);
        if(!actor || actor->Side!=Faction::Player || actor->Hp<=0 || !actors.insert(actor->Id).second)return false;
        for(int target:action->EnemyTargets) {const auto enemy=FindUnit(units,target);if(!enemy || enemy->Side!=Faction::Enemy)return false;}
        positions.push_back(actor->At);
    }
    if(actions.back()->EnemyTargets.empty())return false;
    const auto p=[&](int i){return positions[i];};
    const auto same=[&](int a,int b){return !Common(*actions[a],*actions[b]).empty();};
    const auto diff=[&](int a,int b){return Separate(*actions[a],*actions[b]);};
    const bool aligned=p(0).X==p(1).X || p(0).Y==p(1).Y;
    const int distance=Distance(p(0),p(1));
    if(id==2)return distance==1;
    if(id==3)return same(1,2);
    if(id==5)return std::all_of(positions.begin(),positions.end(),[&](Position at){return Distance(at,positions.back())<=4;});
    if(id==101)return same(0,1) && aligned && distance>=2;
    if(id==102)return diff(0,1) && distance==1;
    if(id==104)return diff(0,1) && aligned && distance>=2 && distance<=4;
    if(id==103 || id==105) {
        for(int target:Common(*actions[0],*actions[1])) {
            // 撃破済みでも命中した敵の位置を使用する。
            const auto t=FindUnit(units,target)->At;const auto a=p(0),b=p(1);
            if(Distance(a,t)!=1 || Distance(b,t)!=1)continue;
            if(id==103 && a.X+b.X==2*t.X && a.Y+b.Y==2*t.Y)return true;
            if(id==105 && std::abs(a.X-b.X)==1 && std::abs(a.Y-b.Y)==1)return true;
        }
        return false;
    }
    if(id==106)return same(0,2) && p(0).Y==p(1).Y && p(1).Y==p(2).Y;
    if(id==107)return same(0,2) && p(0).X==p(1).X && p(1).X==p(2).X;
    if(id==108)return same(1,2) && (Corner(p(0),p(1),p(2)) || Corner(p(1),p(0),p(2)) || Corner(p(2),p(0),p(1)));
    if(id==109)return same(1,2) && p(0).Y==p(1).Y && p(1).Y==p(2).Y;
    if(id==110)return diff(0,1) && diff(0,2) && diff(1,2);
    if(id==111)return same(0,1) && diff(0,2) && diff(1,2);
    auto shared=actions[0]->EnemyTargets;
    for(std::size_t i=1;i<actions.size();++i)std::erase_if(shared,[&](int target){return std::find(actions[i]->EnemyTargets.begin(),actions[i]->EnemyTargets.end(),target)==actions[i]->EnemyTargets.end();});
    return !shared.empty();
}
}
