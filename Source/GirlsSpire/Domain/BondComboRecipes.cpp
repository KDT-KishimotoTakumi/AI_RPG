#include "ComboCatalog.h"
#include "Battle.h"
namespace GirlsSpire {
const std::vector<ComboRecipeDefinition>& BondComboCatalog()
{
    static const std::vector<ComboRecipeDefinition> recipes={
        {201,"焔を鍛つ","主人公の承→茜の結 / 固定絆100・隣接・共通の敵に命中",{Continuation,Ending}},
        {202,"共鳴の舞","主人公の承→澪の結 / 固定絆100・隣接・共通の敵に命中",{Continuation,Ending}},
        {203,"月影の合図","主人公の承→朧の結 / 固定絆100・隣接・共通の敵に命中",{Continuation,Ending}},
        {204,"結びの護符","主人公の承→紫苑の結 / 固定絆100・隣接・共通の敵に命中",{Continuation,Ending}},
        {205,"双翼の狙い","主人公の承→楓の結 / 固定絆100・隣接・共通の敵に命中",{Continuation,Ending}},
        {206,"火花の呼吸","主人公の承→火乃香の結 / 固定絆100・隣接・共通の敵に命中",{Continuation,Ending}},
        {207,"刃心一如","主人公の承→千景の結 / 固定絆100・隣接・共通の敵に命中",{Continuation,Ending}},
        {208,"魂繋ぎの鈴","主人公の承→小夜の結 / 固定絆100・隣接・共通の敵に命中",{Continuation,Ending}}
    };return recipes;
}
std::vector<ComboCandidate> FindBondComboCandidates(const ComboSnapshot& history,const std::vector<BattleUnit>& units,const std::array<int,8>& bonds)
{
    if(history.Pending.empty())return {};
    const auto& last=history.Pending.back();
    if(last.Sequence!=history.LastSequence || last.ActorId<1 || last.ActorId>8 || bonds[last.ActorId-1]!=100)return {};
    const int recipe=200+last.ActorId;
    for(const auto& first:history.Pending){
        if(first.Sequence>=last.Sequence)break;
        if(MatchesComboCondition(recipe,{&first,&last},units))return {{recipe,{first.Sequence,last.Sequence},true}};
    }
    return {};
}
}
