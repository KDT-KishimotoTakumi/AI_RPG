#include "BoardCues.h"
#include <algorithm>
namespace GirlsSpire {
std::string BoardCue::Symbols() const
{
    std::string result;
    if(Side)result=*Side==Faction::Player?"○":(*Side==Faction::Enemy?"△":"◇");
    if(Move)result+="＋";
    if(Attack)result+="×";
    if(Trap || RouteTrapDamage>0 || EnemyDamage>0)result+="！";
    return result;
}
std::vector<BoardCue> ReadBoardCues(const Battle& battle,bool threats)
{
    const auto& state=battle.Snapshot();
    std::vector<BoardCue> result(state.Tiles.size());
    const auto reachable=battle.Reachable(state.ActiveId);
    const auto active=std::find_if(state.Units.begin(),state.Units.end(),[&](const auto& unit){return unit.Id==state.ActiveId;});
    for(std::size_t index=0;index<result.size();++index){
        auto& cue=result[index];
        const Position at{static_cast<int>(index)%state.Width,static_cast<int>(index)/state.Width};
        cue.Trap=state.Tiles[index].Trap;
        for(const auto& unit:state.Units)if(unit.Hp>0 && unit.At==at)cue.Side=unit.Side;
        cue.Move=std::find(reachable.begin(),reachable.end(),at)!=reachable.end();
        cue.Attack=battle.PreviewAttack(state.ActiveId,at).Legal;
        if(cue.Move)cue.RouteTrapDamage=battle.PreviewMove(state.ActiveId,at).Damage;
        if(threats && active!=state.Units.end() && active->Side==Faction::Player){
            if(active->At==at)cue.EnemyDamage=battle.PotentialEnemyDamage(active->Id);
            else if(cue.Move){
                auto projection=battle;
                if(projection.Move(state.Revision,active->Id,at))cue.EnemyDamage=projection.PotentialEnemyDamage(active->Id);
            }
        }
    }
    return result;
}
}
