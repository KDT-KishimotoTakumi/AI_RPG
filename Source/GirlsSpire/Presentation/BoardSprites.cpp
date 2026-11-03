#include "BoardSprites.h"
#include "Misc/Paths.h"
#include "SpireStyle.h"
namespace {
TSharedPtr<FSlateBrush> Frame(const TSharedPtr<FSlateDynamicImageBrush>& atlas,const FBox2f& uv)
{
    auto brush=MakeShared<FSlateBrush>(*atlas);brush->SetUVRegion(uv);
    brush->SetImageSize(FVector2f(atlas->ImageSize.X*(uv.Max.X-uv.Min.X),atlas->ImageSize.Y*(uv.Max.Y-uv.Min.Y)));
    return brush;
}
}
FBoardSprites::FBoardSprites():FBoardSprites(FPaths::ProjectContentDir()/TEXT("Art/Objects")){}
FBoardSprites::FBoardSprites(FString root):Root(MoveTemp(root)){}
const FSlateBrush* FBoardSprites::Terrain(EBoardSprite sprite)
{
    const int index=static_cast<int>(sprite);if(index<0 || index>=8)return nullptr;
    if(TerrainFrames[index])return TerrainFrames[index].Get();
    if(!TerrainAtlas)TerrainAtlas=FSpireStyle::Backdrop(Root/TEXT("terrain-atlas-v1.png"));
    if(!TerrainAtlas)return nullptr;
    // 元画像の透明余白だけをUVで除き、各オブジェクトの足元を揃える。
    const FVector4f crops[]={FVector4f(.14f,.20f,.85f,.85f),FVector4f(.14f,.20f,.85f,.85f),FVector4f(.02f,.10f,.98f,.91f),FVector4f(0,.23f,1,.90f),
        FVector4f(.03f,.16f,.93f,.86f),FVector4f(.11f,.17f,.86f,.86f),FVector4f(.07f,.16f,.93f,.88f),FVector4f(.02f,.40f,.99f,.90f)};
    const auto c=crops[index];const FVector2f cell(index%4,index/4);
    TerrainFrames[index]=Frame(TerrainAtlas,FBox2f(FVector2f((cell.X+c.X)/4,(cell.Y+c.Y)/2),FVector2f((cell.X+c.Z)/4,(cell.Y+c.W)/2)));
    return TerrainFrames[index].Get();
}
const FSlateBrush* FBoardSprites::Enemy(const GirlsSpire::BattleUnit& unit)
{
    const int index=EnemyFrame(unit);if(EnemyFrames[index])return EnemyFrames[index].Get();
    if(!EnemyAtlas)EnemyAtlas=FSpireStyle::Backdrop(Root/TEXT("enemy-atlas-v1.png"));
    if(!EnemyAtlas)return nullptr;
    const float rows[]={0,.317f,.628f,1};const int row=index/4;const float column=static_cast<float>(index%4);
    EnemyFrames[index]=Frame(EnemyAtlas,FBox2f(FVector2f(column/4,rows[row]),FVector2f((column+1)/4,rows[row+1])));
    return EnemyFrames[index].Get();
}
EBoardSprite FBoardSprites::ObjectFor(const GirlsSpire::Tile& tile,int parity)
{
    if(tile.Wall)return parity%2==0?EBoardSprite::Tree:EBoardSprite::Wall;
    if(tile.ObstacleHp>0)return EBoardSprite::Barricade;
    if(tile.Trap)return EBoardSprite::Trap;
    if(tile.Elevation>0)return EBoardSprite::Ledge;
    if(tile.Cover>0)return EBoardSprite::Cover;
    return EBoardSprite::None;
}
int FBoardSprites::EnemyFrame(const GirlsSpire::BattleUnit& unit)
{
    using GirlsSpire::BattleRole;
    if(unit.CreatesCover)return 9;
    switch(unit.Role) {
    case BattleRole::Archer:return 1;case BattleRole::Gunner:return 2;case BattleRole::Cavalry:return 3;
    case BattleRole::Ninja:return 4;case BattleRole::Oni:return 5;case BattleRole::CurseMonk:return 6;
    case BattleRole::Fox:return 7;case BattleRole::Spider:return 8;case BattleRole::FallenPrincess:return 10;
    case BattleRole::CastleLord:return 11;default:return 0;
    }
}
