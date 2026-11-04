#include "RunScreen.h"
#include "ComboLabels.h"
#include "Widgets/SBoxPanel.h"

void SRunScreen::ShowComboLibrary(int recipeId)
{
    ResetPageScroll();
    Body->ClearChildren();
    if(recipeId==0) {
        AddText(TEXT("連携図鑑"),"combo.library.heading",28);
        if(RetainedProfile.DiscoveredCombos.empty())
            AddText(TEXT("連携を発見すると、ここで成立条件を読み返せます。"),"combo.library.empty");
        for(int id:RetainedProfile.DiscoveredCombos)
            AddAction(ComboDisplayName(id),*FString::Printf(TEXT("combo.library.entry.%d"),id),[this,id]{ShowComboLibrary(id);});
        AddAction(TEXT("タイトルへ戻る"),"combo.library.back",[this]{ShowTitle();});
        return;
    }
    // 発見・効果の判定元と同じカタログを使い、閲覧では保存や乱数を操作しない。
    AddText(ComboDisplayName(recipeId),"combo.library.name",28);
    AddText(ComboDisplayReason(recipeId),"combo.library.reason");
    AddAction(TEXT("一覧へ戻る"),"combo.library.list",[this]{ShowComboLibrary();});
}
