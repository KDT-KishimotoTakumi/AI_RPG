#include "Misc/AutomationTest.h"
#include "Presentation/UnitMotion.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGirlsSpireUnitMotionTest,"GirlsSpire.Motion.Path",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FGirlsSpireUnitMotionTest::RunTest(const FString&)
{
    const std::vector<GirlsSpire::Position> path={{1,1},{1,2},{2,2}};
    TestTrue(TEXT("経路の長さに応じた有限の演出時間"),FUnitMotion::Duration(path)>0 && FUnitMotion::Duration(path)<=.65);
    TestEqual(TEXT("空/単点の演出時間はゼロ"),FUnitMotion::Duration({})+FUnitMotion::Duration({{1,1}}),0.);
    TestEqual(TEXT("長い経路も0.65秒が上限"),FUnitMotion::Duration(std::vector<GirlsSpire::Position>(20)),.65);
    TestTrue(TEXT("開始位置"),FUnitMotion::Sample(path,0,false).Equals(FVector2D(1,1)));
    TestTrue(TEXT("角まで最初の辺を進む"),FUnitMotion::Sample(path,.05,false).Equals(FVector2D(1,1.5)));
    TestTrue(TEXT("壁を横切る直線補間をしない"),FUnitMotion::Sample(path,.1,false).Equals(FVector2D(1,2)));
    TestTrue(TEXT("角を曲がる"),FUnitMotion::Sample(path,.15,false).Equals(FVector2D(1.5,2)));
    TestTrue(TEXT("終了後は確定位置"),FUnitMotion::Sample(path,100,false).Equals(FVector2D(2,2)));
    TestTrue(TEXT("負の時間は開始位置"),FUnitMotion::Sample(path,-1,false).Equals(FVector2D(1,1)));
    TestTrue(TEXT("演出軽減では即時に確定位置"),FUnitMotion::Sample(path,0,true).Equals(FVector2D(2,2)));
    TestTrue(TEXT("空/単点経路は静止"),FUnitMotion::Sample({},0,false).IsZero() && FUnitMotion::Sample({{2,3}},0,false).Equals(FVector2D(2,3)));
    TestTrue(TEXT("待機中も画像が揺れる"),FMath::Abs(FUnitMotion::IdleLift(.3,false))>.5);
    TestTrue(TEXT("揺れは小さく周期的"),FMath::Abs(FUnitMotion::IdleLift(.3,false))<=1.5 && FMath::IsNearlyZero(FUnitMotion::IdleLift(1.2,false)));
    TestEqual(TEXT("演出軽減は揺れなし"),FUnitMotion::IdleLift(.3,true),0.);
    return true;
}
