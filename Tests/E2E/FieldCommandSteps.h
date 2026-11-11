#pragma once
#include "Misc/AutomationTest.h"
#include "AutomationDriverCommon.h"

// 全画面戦場の主人公の手番では、「待機」「技」は短いコマンド一覧の下位メニューにある。
// 下位メニューが既に開いていれば開く操作を省き、項目だけを押す。失敗は、開く操作か項目かを区別して記録する。
// click は const char* のIDを押して成否を返す呼び出し可能な値。
template<typename DriverPtr,typename ClickFn>
bool ClickFieldSub(FAutomationTestBase& test,const DriverPtr& driver,ClickFn&& click,const char* open,const char* id)
{
    if(!driver->FindElement(By::Id(id))->Exists() && !click(open)){
        test.AddError(FString::Printf(TEXT("下位メニューを開けません: %hs"),open));
        return false;
    }
    if(!click(id)){
        test.AddError(FString::Printf(TEXT("下位メニューの項目を押せません: %hs（開く操作: %hs）"),id,open));
        return false;
    }
    return true;
}
template<typename DriverPtr,typename ClickFn>
bool ClickFieldWait(FAutomationTestBase& test,const DriverPtr& driver,ClickFn&& click,const char* id="battle.wait.south")
{
    return ClickFieldSub(test,driver,click,"battle.field.wait",id);
}
template<typename DriverPtr,typename ClickFn>
bool ClickFieldSkill(FAutomationTestBase& test,const DriverPtr& driver,ClickFn&& click,const char* id)
{
    return ClickFieldSub(test,driver,click,"battle.field.skills",id);
}