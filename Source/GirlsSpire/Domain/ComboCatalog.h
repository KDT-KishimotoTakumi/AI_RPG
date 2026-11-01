#pragma once
#include "Combo.h"
#include <string_view>
namespace GirlsSpire {
struct ComboRecipeDefinition {int Id;std::string_view Name;std::string_view Reason;std::vector<int> Pattern;};
const std::vector<ComboRecipeDefinition>& ComboCatalog();
const ComboRecipeDefinition* FindComboRecipe(int id);
bool MatchesComboCondition(int id,const std::vector<const ComboAction*>& actions,const std::vector<BattleUnit>& units);
}
