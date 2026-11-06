#include "CombatScreen.h"
#include "CharacterPortraits.h"
#include "Framework/MetaData/DriverMetaData.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SCombatScreen::RefreshPortraits()
{
    using namespace GirlsSpire;
    PortraitPanel->ClearChildren();
    const auto& state=Combat->Snapshot();
    for(const auto& unit:state.Units) {
        if(unit.Id<0 || unit.Id>8)continue;
        TSharedPtr<SVerticalBox> card;
        PortraitPanel->AddSlot().AutoWidth().Padding(0,0,4,0)
            [SNew(SBox).WidthOverride(76)[SAssignNew(card,SVerticalBox)]];
        const auto image=PortraitResources->Get(unit.Id,AppearanceSnapshot[unit.Id]);
        if(image) {
            card->AddSlot().AutoHeight().HAlign(HAlign_Center)
                [SNew(SBox).WidthOverride(64).HeightOverride(96)
                    [SNew(SImage).Image(image.Get()).ColorAndOpacity(unit.Hp>0?FLinearColor::White:FLinearColor(0.35f,0.35f,0.35f,1))
                        .AddMetaData(FDriverMetaData::Id(*FString::Printf(TEXT("battle.portrait.image.%d"),unit.Id)))]];
        } else {
            card->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("画像未読込")))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular",FontSize(12)))
                .AddMetaData(FDriverMetaData::Id(*FString::Printf(TEXT("battle.portrait.missing.%d"),unit.Id)))];
        }
        const FString values[]={FCharacterPortraits::Name(unit.Id),FString::Printf(TEXT("HP %d / %d"),unit.Hp,unit.MaxHp),
            unit.Hp<=0?TEXT("戦闘不能"):unit.Id==state.ActiveId?TEXT("手番中"):TEXT("待機中"),
            unit.Side==Faction::Player?TEXT("味方"):unit.Side==Faction::Enemy?TEXT("敵"):TEXT("中立・手番なし")};
        const TCHAR* fields[]={TEXT("name"),TEXT("hp"),TEXT("state"),TEXT("faction")};
        for(int i=0;i<4;++i)card->AddSlot().AutoHeight()
            [SNew(STextBlock).Text(FText::FromString(values[i])).AutoWrapText(true)
                .Font(FCoreStyle::GetDefaultFontStyle("Regular",FontSize(12)))
                .AddMetaData(FDriverMetaData::Id(*FString::Printf(TEXT("battle.portrait.%s.%d"),fields[i],unit.Id)))];
    }
}
