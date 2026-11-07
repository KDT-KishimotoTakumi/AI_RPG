#pragma once
#include "CoreMinimal.h"
#include "Domain/CombatTypes.h"
#include <vector>

struct FUnitMotion {
    static double Duration(const std::vector<GirlsSpire::Position>& path);
    static FVector2D Sample(const std::vector<GirlsSpire::Position>& path,double age,bool reduced);
    static double IdleLift(double age,bool reduced);
    static double ImpactShift(double age,bool reduced);
};
