#include "ComboCatalog.h"
#include <algorithm>
namespace GirlsSpire {
const std::vector<ComboRecipeDefinition>& ComboCatalog()
{
    static const std::vector<ComboRecipeDefinition> recipes={
        {2,"2人・隣接連携","起→結 / 2人が隣接",{Opening,Ending}},
        {3,"3人・同一対象連携","起→転→結 / 後半2人が同じ敵を攻撃",{Opening,Twist,Ending}},
        {5,"5人・集合連携","起→承→承→転→結 / 全員が結から4マス以内",{Opening,Continuation,Continuation,Twist,Ending}},
        {101,"直線の追撃","承→結 / 共通の敵・同じ行か列・距離2以上",{Continuation,Ending}},
        {102,"交差の追撃","承→結 / 別の敵・2人が上下左右に隣接",{Continuation,Ending}},
        {103,"対面の挟撃","転→結 / 共通の敵を上下または左右から隣接して挟む",{Twist,Ending}},
        {104,"陽動の受け渡し","転→結 / 別の敵・同じ行か列・距離2～4",{Twist,Ending}},
        {105,"角の包囲","結→結 / 共通の敵に2人が隣接・2人は斜め隣接",{Ending,Ending}},
        {106,"横陣の受け渡し","承→転→結 / 最初と最後が共通の敵・3人が同じ行",{Continuation,Twist,Ending}},
        {107,"縦陣の受け渡し","承→転→結 / 最初と最後が共通の敵・3人が同じ列",{Continuation,Twist,Ending}},
        {108,"鉤形の追撃","転→承→結 / 後半2人が共通の敵・3人がL字",{Twist,Continuation,Ending}},
        {109,"横列の畳み掛け","転→承→結 / 後半2人が共通の敵・3人が同じ行",{Twist,Continuation,Ending}},
        {110,"三方面の攻勢","承→承→結 / 3行動が互いに別の敵を攻撃",{Continuation,Continuation,Ending}},
        {111,"集中からの転進","転→転→結 / 前半2人が共通の敵・最後は双方と別の敵",{Twist,Twist,Ending}},
        {112,"二段の仕込み","承→承→転→転→結 / 5人全員が共通の敵",{Continuation,Continuation,Twist,Twist,Ending}},
        {113,"交互の攻勢","承→転→承→転→結 / 5人全員が共通の敵",{Continuation,Twist,Continuation,Twist,Ending}},
        {114,"交互の崩し","転→承→転→承→結 / 5人全員が共通の敵",{Twist,Continuation,Twist,Continuation,Ending}},
        {115,"先手の崩し","転→転→承→承→結 / 5人全員が共通の敵",{Twist,Twist,Continuation,Continuation,Ending}},
        {116,"連撃からの崩し","承→承→承→転→結 / 5人全員が共通の敵",{Continuation,Continuation,Continuation,Twist,Ending}},
        {117,"崩して繋ぐ総攻め","転→承→承→承→結 / 5人全員が共通の敵",{Twist,Continuation,Continuation,Continuation,Ending}}
    };
    return recipes;
}
const ComboRecipeDefinition* FindComboRecipe(int id)
{
    const auto& recipes=ComboCatalog();
    const auto at=std::find_if(recipes.begin(),recipes.end(),[id](const auto& recipe){return recipe.Id==id;});
    return at==recipes.end()?nullptr:&*at;
}
}
