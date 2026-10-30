#include "Battle.h"
#include <algorithm>
namespace GirlsSpire {
int Battle::MovementAllowance(int actorId) const
{
    const auto actor=Find(actorId);
    return actor?std::max(0,actor->Movement-(actor->MovementWebbed?2:0)):0;
}
MovePreview Battle::PreviewMove(int actorId,Position to) const
{
    MovePreview preview;
    const auto destinations=Reachable(actorId);
    if(std::find(destinations.begin(),destinations.end(),to)==destinations.end()) return preview;
    const auto& actor=*Find(actorId);
    const int size=static_cast<int>(state_.Tiles.size());
    const int origin=actor.At.Y*state_.Width+actor.At.X;
    const int target=to.Y*state_.Width+to.X;
    const int limit=MovementAllowance(actorId);
    // 歩数ごとの最小罠数を保持。最短距離だけの探索では安全な迂回路を捨ててしまう。
    std::vector<int> costs((limit+1)*size,10000),parents((limit+1)*size,-1);
    costs[origin]=0;
    const Position directions[]={{0,-1},{1,0},{0,1},{-1,0}};
    int bestStep=-1,bestCost=10000;
    for(int step=1;step<=limit;++step) {
        for(int index=0;index<size;++index) {
            const int cost=costs[(step-1)*size+index];
            if(cost==10000) continue;
            const Position from{index%state_.Width,index/state_.Width};
            for(const auto direction:directions) {
                const Position next{from.X+direction.X,from.Y+direction.Y};
                if(!TerrainStep(from,next)) continue;
                const int occupant=Occupant(next);
                if(occupant>=0 && Find(occupant)->Side!=actor.Side) continue;
                const int nextIndex=next.Y*state_.Width+next.X;
                const int nextCost=cost+(state_.Tiles[nextIndex].Trap?10:0);
                const int key=step*size+nextIndex;
                if(nextCost<costs[key]) { costs[key]=nextCost; parents[key]=index; }
            }
        }
        if(costs[step*size+target]<bestCost) { bestCost=costs[step*size+target]; bestStep=step; }
    }
    // Reachableと同じ地形・占有規則なので、ここでは必ず経路が存在する。
    int index=target;
    for(int step=bestStep;step>0;--step) {
        preview.Path.push_back({index%state_.Width,index/state_.Width});
        index=parents[step*size+index];
    }
    std::reverse(preview.Path.begin(),preview.Path.end());
    preview.Legal=true;
    for(std::size_t i=0;i<preview.Path.size();++i) {
        const auto at=preview.Path[i];
        if(state_.Tiles[at.Y*state_.Width+at.X].Trap) preview.Damage+=std::min(10,actor.Hp-preview.Damage);
        if(preview.Damage==actor.Hp) {
            preview.KnockedOut=true; preview.Path.resize(i+1); break;
        }
    }
    return preview;
}

bool Battle::Move(std::uint64_t revision,int actorId,Position to)
{
    if(revision!=state_.Revision) return false;
    const auto preview=PreviewMove(actorId,to);
    if(!preview.Legal) return false;
    auto& actor=*Find(actorId);
    auto previous=actor.At;
    const Position direction{preview.Path.front().X-previous.X,preview.Path.front().Y-previous.Y};
    bool straight=true;
    for(const auto at:preview.Path) {
        if(at.X-previous.X!=direction.X || at.Y-previous.Y!=direction.Y)straight=false;
        previous=at;
        actor.At=at;
        auto& tile=state_.Tiles[at.Y*state_.Width+at.X];
        if(tile.Trap) { actor.Hp=std::max(0,actor.Hp-10); tile.Trap=false; }
    }
    state_.Moved=true; ++state_.Revision;
    state_.ChargeReady=actor.Role==BattleRole::Cavalry && actor.Hp>0 && preview.Path.size()>=3 && straight;
    RecordCombo(actorId,Opening,!preview.KnockedOut,{},preview.KnockedOut?std::vector<int>{actorId}:std::vector<int>{});
    Evaluate();
    if(preview.KnockedOut && state_.Outcome==BattleOutcome::Ongoing) { AdvanceTurn(); Evaluate(); }
    return true;
}
}
