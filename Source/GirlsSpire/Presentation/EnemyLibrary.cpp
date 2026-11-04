#include "RunScreen.h"
#include "BoardSprites.h"
#include "Domain/EnemyCatalog.h"
#include "Framework/MetaData/DriverMetaData.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"

void SRunScreen::ShowEnemyLibrary(int enemyId)
{
    ResetPageScroll();
    Body->ClearChildren();
    if(enemyId==0) {
        AddText(TEXT("敵図鑑"),"enemy.library.heading",28);
        if(RetainedProfile.DiscoveredEnemies.empty())
            AddText(TEXT("遭遇した敵を、ここで読み返せます。"),"enemy.library.empty");
        for(int id:RetainedProfile.DiscoveredEnemies) {
            const auto definition=GirlsSpire::FindEnemyDefinition(id);
            AddAction(UTF8_TO_TCHAR(definition->Name.data()),*FString::Printf(TEXT("enemy.library.entry.%d"),id),[this,id]{ShowEnemyLibrary(id);});
        }
        AddAction(TEXT("タイトルへ戻る"),"enemy.library.back",[this]{ShowTitle();});
        return;
    }
    const auto definition=GirlsSpire::FindEnemyDefinition(enemyId);
    if(!definition){ShowEnemyLibrary();return;}
    AddText(UTF8_TO_TCHAR(definition->Name.data()),"enemy.library.name",28);
    if(!EnemyLibrarySprites)EnemyLibrarySprites=MakeShared<FBoardSprites>();
    const auto& unit=definition->Base;
    if(const auto brush=EnemyLibrarySprites->Enemy(unit))
        Body->AddSlot().AutoHeight().HAlign(HAlign_Left)
            [SNew(SBox).WidthOverride(160).HeightOverride(180)
                [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
                    [SNew(SImage).Image(brush).AddMetaData(FDriverMetaData::Id("enemy.library.image"))]]];
    else AddText(TEXT("画像を利用できません。名前と説明で確認できます。"),"enemy.library.image.missing",16);
    AddText(FString::Printf(TEXT("HP %d / 攻撃 %d / 防御 %d\n素早さ %d / 移動 %d / 射程 %d"),unit.MaxHp,unit.Attack,unit.Defense,unit.Speed,unit.Movement,unit.Range),"enemy.library.stats");
    AddText(TEXT("中級の基準値。精鋭戦のHP・難易度・一時効果によって変化します。"),"enemy.library.reference",16);
    AddText(UTF8_TO_TCHAR(definition->Description.data()),"enemy.library.description");
    AddAction(TEXT("一覧へ戻る"),"enemy.library.list",[this]{ShowEnemyLibrary();});
}
