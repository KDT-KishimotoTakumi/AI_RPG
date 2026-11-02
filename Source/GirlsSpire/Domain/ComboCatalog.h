#pragma once
#include "Combo.h"
#include <string_view>
#include <array>
namespace GirlsSpire {
struct ComboRecipeDefinition {int Id;std::string_view Name;std::string_view Reason;std::vector<int> Pattern;};
const std::vector<ComboRecipeDefinition>& ComboCatalog();
const ComboRecipeDefinition* FindComboRecipe(int id);
const std::vector<ComboRecipeDefinition>& BondComboCatalog();
std::vector<ComboCandidate> FindBondComboCandidates(const ComboSnapshot& history,const std::vector<BattleUnit>& units,const std::array<int,8>& startingBonds);
bool MatchesComboCondition(int id,const std::vector<const ComboAction*>& actions,const std::vector<BattleUnit>& units);
}
