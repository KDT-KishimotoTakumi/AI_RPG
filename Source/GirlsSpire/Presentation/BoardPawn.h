#pragma once
#include "Widgets/SCompoundWidget.h"
#include "Domain/CombatTypes.h"
#include "UnitMotion.h"

class SBoardPawn : public SCompoundWidget {
public:
    SLATE_BEGIN_ARGS(SBoardPawn):_Id(0),_Side(GirlsSpire::Faction::Player),_Image(nullptr),_Active(false),_Reduced(false),_Moving(false),_Scale(1.f){}
        SLATE_ARGUMENT(int,Id)
        SLATE_ARGUMENT(GirlsSpire::Faction,Side)
        SLATE_ARGUMENT(const FSlateBrush*,Image)
        SLATE_ARGUMENT(bool,Active)
        SLATE_ARGUMENT(bool,Reduced)
        SLATE_ARGUMENT(bool,Moving)
        SLATE_ARGUMENT(float,Scale)
        SLATE_ARGUMENT(TOptional<double>,ImpactTime)
        SLATE_ARGUMENT(TOptional<FUnitStrikeVisual>,Strike)
    SLATE_END_ARGS()
    void Construct(const FArguments& args);
};
