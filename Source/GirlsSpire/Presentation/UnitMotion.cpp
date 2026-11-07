#include "UnitMotion.h"
double FUnitMotion::Duration(const std::vector<GirlsSpire::Position>& path)
{
    return path.size()<2?0:FMath::Min(.65,.1*static_cast<double>(path.size()-1));
}
FVector2D FUnitMotion::Sample(const std::vector<GirlsSpire::Position>& path,double age,bool reduced)
{
    if(path.empty())return FVector2D::ZeroVector;
    if(reduced || path.size()==1)return FVector2D(path.back().X,path.back().Y);
    const auto progress=FMath::Clamp(age/Duration(path),0.,1.)*static_cast<double>(path.size()-1);
    const auto index=FMath::Min(static_cast<std::size_t>(progress),path.size()-2);
    const FVector2D a(path[index].X,path[index].Y),b(path[index+1].X,path[index+1].Y);
    return FMath::Lerp(a,b,progress-static_cast<double>(index));
}
double FUnitMotion::IdleLift(double age,bool reduced)
{
    return reduced?0:FMath::Sin(age*2*UE_DOUBLE_PI/1.2)*1.2;
}
double FUnitMotion::ImpactShift(double age,bool reduced)
{
    if(reduced)return 0;
    const auto phase=FMath::Clamp(age/.45,0.,1.);
    return 6.*(1.-FMath::Abs(2.*phase-1.));
}
