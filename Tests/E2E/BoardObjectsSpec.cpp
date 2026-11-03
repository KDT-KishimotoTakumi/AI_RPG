#include "Misc/AutomationTest.h"
#include "SlateTestSync.h"
#include "AutomationDriverCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MetaData/DriverIdMetaData.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Presentation/CombatScreen.h"
#include "Presentation/BoardSprites.h"
#include "Presentation/CharacterPortraits.h"
#include "Misc/Paths.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
namespace {
bool ObjectPainted(const TSharedRef<SWidget>& widget,FName id)
{
    if(!widget->GetVisibility().IsVisible())return false;
    const auto tag=widget->GetMetaData<FDriverIdMetaData>();if(tag && tag->Id==id)return widget->GetCachedGeometry().GetLocalSize().GetMin()>0;
    auto* children=widget->GetChildren();for(int i=0;i<children->Num();++i)if(ObjectPainted(children->GetChildAt(i),id))return true;
    return false;
}
FVector2D ObjectLocalSize(const TSharedRef<SWidget>& widget,FName id)
{
    const auto tag=widget->GetMetaData<FDriverIdMetaData>();if(tag && tag->Id==id)return widget->GetCachedGeometry().GetLocalSize();
    auto* children=widget->GetChildren();for(int i=0;i<children->Num();++i){const auto size=ObjectLocalSize(children->GetChildAt(i),id);if(!size.IsZero())return size;}return FVector2D::ZeroVector;
}
}
BEGIN_DEFINE_SPEC(FGirlsSpireBoardObjectsSpec,"GirlsSpire.UI.BoardObjects",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    TSharedPtr<SWindow> Window;FAutomationDriverPtr Driver;
    bool Painted(FName id){bool result=false;Async(EAsyncExecution::TaskGraphMainThread,[this,id,&result]{result=ObjectPainted(Window.ToSharedRef(),id);}).Wait();return result;}
    bool Click(const char* id){const auto element=Driver->FindElement(By::Id(id));if(!element->IsVisible()){Driver->FindElement(By::Id("battle.scroll"))->ScrollToEndUntil(By::Id(id));DrainSlateLayout();}return TestTrue(UTF8_TO_TCHAR(id),ClickAndDrain(element));}
    void Capture(){Async(EAsyncExecution::TaskGraphMainThread,[this]{TArray<FColor> pixels;FIntVector size;if(!TestTrue(TEXT("独立スプライトを撮影"),FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),pixels,size)))return;TArray64<uint8> png;FImageUtils::PNGCompressImageArray(size.X,size.Y,pixels,png);const auto dir=FPaths::ProjectSavedDir()/TEXT("Screenshots");IFileManager::Get().MakeDirectory(*dir,true);TestTrue(TEXT("画像を保持"),FFileHelper::SaveArrayToFile(png,*(dir/TEXT("BoardObjects.png"))));}).Wait();}
END_DEFINE_SPEC(FGirlsSpireBoardObjectsSpec)
void FGirlsSpireBoardObjectsSpec::Define()
{
    It(TEXT("アトラス8地形と12敵を再利用し欠落時に退避する"),[this]{
        using namespace GirlsSpire;FBoardSprites sprites;
        for(int i=0;i<8;++i){const auto brush=sprites.Terrain(static_cast<EBoardSprite>(i));TestNotNull(TEXT("地形フレーム"),brush);TestTrue(TEXT("地形ブラシを再利用"),brush==sprites.Terrain(static_cast<EBoardSprite>(i)));}
        TestNull(TEXT("不正フレーム"),sprites.Terrain(EBoardSprite::None));TestNull(TEXT("範囲外"),sprites.Terrain(static_cast<EBoardSprite>(99)));
        const BattleRole roles[]={BattleRole::Standard,BattleRole::Archer,BattleRole::Gunner,BattleRole::Cavalry,BattleRole::Ninja,BattleRole::Oni,BattleRole::CurseMonk,BattleRole::Fox,BattleRole::Spider,BattleRole::Standard,BattleRole::FallenPrincess,BattleRole::CastleLord};
        for(int i=0;i<12;++i){BattleUnit unit;unit.Role=roles[i];unit.CreatesCover=i==9;TestEqual(TEXT("敵種の対応"),FBoardSprites::EnemyFrame(unit),i);TestNotNull(TEXT("敵の画像"),sprites.Enemy(unit));}
        Tile tile;TestTrue(TEXT("空き地"),FBoardSprites::ObjectFor(tile,0)==EBoardSprite::None);tile.Wall=true;TestTrue(TEXT("壁を木/石壁で表現"),FBoardSprites::ObjectFor(tile,0)==EBoardSprite::Tree && FBoardSprites::ObjectFor(tile,1)==EBoardSprite::Wall);
        tile={};tile.ObstacleHp=20;TestTrue(TEXT("破壊対象"),FBoardSprites::ObjectFor(tile,0)==EBoardSprite::Barricade);tile.ObstacleHp=0;TestTrue(TEXT("破壊後は消える"),FBoardSprites::ObjectFor(tile,0)==EBoardSprite::None);
        tile.Trap=true;TestTrue(TEXT("罠"),FBoardSprites::ObjectFor(tile,0)==EBoardSprite::Trap);tile={};tile.Elevation=1;TestTrue(TEXT("高低"),FBoardSprites::ObjectFor(tile,0)==EBoardSprite::Ledge);tile={};tile.Cover=1;TestTrue(TEXT("遮蔽"),FBoardSprites::ObjectFor(tile,0)==EBoardSprite::Cover);
        FBoardSprites missing(FPaths::ProjectSavedDir()/TEXT("Tests/no-object-atlas"));TestNull(TEXT("地形欠落"),missing.Terrain(EBoardSprite::Grass));TestNull(TEXT("敵欠落"),missing.Enemy(BattleUnit{}));
    });
    Describe(TEXT("実盤面"),[this]{
        BeforeEach([this]{FSlateApplication::Get().CloseToolTip();IAutomationDriverModule::Get().Enable();Driver=IAutomationDriverModule::Get().CreateDriver();Window=SNew(SWindow).ClientSize(FVector2D(1600,1000));FSlateApplication::Get().AddWindow(Window.ToSharedRef());});
        LatentBeforeEach(WaitForSlateLayout);
        for(int mode=0;mode<4;++mode)It(*FString::Printf(TEXT("独立駒と地形%d・破壊/罠消費と状態不変"),mode),EAsyncExecution::ThreadPool,FTimespan::FromSeconds(120),[this,mode]{
            using namespace GirlsSpire;std::vector<Tile> tiles(64);tiles[1*8+2].ObstacleHp=20;tiles[2*8+2].Wall=true;tiles[2*8+3].Wall=true;tiles[3*8+2].Trap=true;tiles[3*8+3].Cover=1;tiles[3*8+4].Elevation=1;
            BattleUnit hero;hero.At={1,1};hero.Role=BattleRole::Ninja;hero.Hp=hero.MaxHp=40;hero.Attack=20;hero.Movement=6;hero.Speed=99;
            BattleUnit enemy;enemy.Id=100;enemy.Side=Faction::Enemy;enemy.At={5,3};enemy.Role=BattleRole::Archer;enemy.Speed=0;
            auto battle=MakeShared<Battle>(8,8,tiles,std::vector<BattleUnit>{hero,enemy},42);
            auto sprites=mode==1?MakeShared<FBoardSprites>(FPaths::ProjectSavedDir()/TEXT("Tests/no-board-sprites")):MakeShared<FBoardSprites>();
            auto portraits=mode==3?MakeShared<FCharacterPortraits>(FPaths::ProjectSavedDir()/TEXT("Tests/no-fixed-portraits")):MakeShared<FCharacterPortraits>();
            Async(EAsyncExecution::TaskGraphMainThread,[this,battle,sprites,portraits]{Window->SetContent(SNew(SCombatScreen).Combat(battle).Sprites(sprites).Portraits(portraits));}).Wait();DrainSlateLayout();
            if(mode==2 && !Click("battle.cues.monochrome"))return;
            const auto before=battle->Checkpoint();TestTrue(TEXT("人物は独立オブジェクト"),Painted("battle.pawn.0") && Painted("battle.pawn.100"));
            TestTrue(TEXT("足元台座と手番枠"),Painted("battle.pawn.base.0") && Painted("battle.pawn.active.0"));
            TestEqual(TEXT("地面画像と欠落/単色退避"),Painted("battle.ground.0.0"),mode==0 || mode==3);
            for(const char* id:{"battle.object.2.1","battle.object.2.2","battle.object.3.2","battle.object.2.3","battle.object.3.3","battle.object.4.3"})TestEqual(TEXT("状態に対応した地形画像"),Painted(id),mode==0 || mode==3);
            TestEqual(TEXT("敵は専用スプライト"),Painted("battle.unit.art.100"),mode==0 || mode==3);
            TestEqual(TEXT("固定人物が欠落しても敵の外見に置き換えない"),Painted("battle.unit.art.0"),mode<2);
            TestEqual(TEXT("人物欠落/単色は陣営記号へ退避"),Painted("battle.pawn.fallback.0"),mode>=2);
            if(mode<2)Async(EAsyncExecution::TaskGraphMainThread,[this]{const auto size=ObjectLocalSize(Window.ToSharedRef(),"battle.unit.art.0");TestTrue(TEXT("揺れは縮小前の巨大テクスチャでなく駒の座標系で実行"),size.Y>0 && size.Y<200);}).Wait();
            TestTrue(TEXT("HPと地形の文字を維持"),Driver->FindElement(By::Id("battle.celllabel.1.1"))->GetText().ToString().Contains(TEXT("40")) && Driver->FindElement(By::Id("battle.celllabel.2.1"))->GetText().ToString().Contains(TEXT("障20")));
            TestTrue(TEXT("描画で全状態/RNG不変"),battle->Checkpoint()==before);if(mode==0)Capture();
            auto expected=*battle;TestTrue(TEXT("破壊の既存規則"),expected.Attack(expected.Snapshot().Revision,0,{2,1}));
            if(!TestTrue(TEXT("障害物をクリック"),ClickAndDrain(Driver->FindElement(By::Id("battle.cell.2.1")))))return;if(!Click("battle.confirm"))return;
            TestFalse(TEXT("破壊した柵が消える"),Painted("battle.object.2.1"));TestTrue(TEXT("破壊結果/RNG一致"),battle->Checkpoint()==expected.Checkpoint());
            TestTrue(TEXT("罠移動の既存規則"),expected.Move(expected.Snapshot().Revision,0,{2,3}));
            if(!TestTrue(TEXT("罠のマスへ移動"),ClickAndDrain(Driver->FindElement(By::Id("battle.cell.2.3")))))return;if(!Click("battle.move.confirm"))return;
            TestFalse(TEXT("発動した罠が消える"),Painted("battle.object.2.3"));TestTrue(TEXT("罠/HP/移動/乱数が既存規則と一致"),battle->Checkpoint()==expected.Checkpoint());
        });
        AfterEach([this]{Driver.Reset();if(Window){Window->SetContent(SNullWidget::NullWidget);FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());Window.Reset();}IAutomationDriverModule::Get().Disable();});
    });
}
