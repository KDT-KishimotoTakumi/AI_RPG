#pragma once
#include "CoreMinimal.h"
#include "Domain/Battle.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include <array>

enum class EBoardSprite { Grass, Paving, Tree, Wall, Barricade, Trap, Ledge, Cover, None };
class FBoardSprites {
public:
    FBoardSprites();
    explicit FBoardSprites(FString root);
    const FSlateBrush* Terrain(EBoardSprite sprite);
    const FSlateBrush* Enemy(const GirlsSpire::BattleUnit& unit);
    static EBoardSprite ObjectFor(const GirlsSpire::Tile& tile,int parity);
    static int EnemyFrame(const GirlsSpire::BattleUnit& unit);
private:
    FString Root;
    TSharedPtr<FSlateDynamicImageBrush> TerrainAtlas,EnemyAtlas;
    std::array<TSharedPtr<FSlateBrush>,8> TerrainFrames;
    std::array<TSharedPtr<FSlateBrush>,12> EnemyFrames;
};
