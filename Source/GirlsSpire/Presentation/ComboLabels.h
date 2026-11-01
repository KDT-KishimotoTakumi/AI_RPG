#pragma once
#include "CoreMinimal.h"
#include "Domain/ComboCatalog.h"

inline FString ComboDisplayName(int id)
{
    const auto recipe=GirlsSpire::FindComboRecipe(id);
    if(recipe)return UTF8_TO_TCHAR(recipe->Name.data());
    return FString::Printf(TEXT("連携%d"),id);
}
inline FString ComboDisplayReason(int id)
{
    const auto recipe=GirlsSpire::FindComboRecipe(id);
    return recipe?UTF8_TO_TCHAR(recipe->Reason.data()):TEXT("成立条件は未登録");
}
