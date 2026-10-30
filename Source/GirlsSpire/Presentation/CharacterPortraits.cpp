#include "CharacterPortraits.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Misc/Paths.h"

namespace {
const TCHAR* const Names[]={TEXT("黒鉄 蓮"),TEXT("真田 茜"),TEXT("白峰 澪"),TEXT("朧"),TEXT("土御門 紫苑"),
    TEXT("鷹司 楓"),TEXT("雑賀 火乃香"),TEXT("柳生 千景"),TEXT("鈴鹿 小夜")};
const TCHAR* const Files[]={TEXT("hero-ren"),TEXT("akane"),TEXT("mio"),TEXT("oboro"),TEXT("shion"),
    TEXT("kaede"),TEXT("honoka"),TEXT("chikage"),TEXT("sayo")};
}
FCharacterPortraits::FCharacterPortraits():FCharacterPortraits(FPaths::ProjectContentDir()/TEXT("Art/Characters")) {}
FCharacterPortraits::FCharacterPortraits(FString root):Root(MoveTemp(root)) {}
const TCHAR* FCharacterPortraits::Name(int actor)
{
    return actor>=0 && actor<9?Names[actor]:TEXT("不明な人物");
}
TSharedPtr<FSlateDynamicImageBrush> FCharacterPortraits::Get(int actor)
{
    if(actor<0 || actor>=9)return nullptr;
    if(Images[actor])return Images[actor];
    const auto path=FPaths::ConvertRelativePathToFull(Root/(FString(Files[actor])+TEXT("-v1.png")));
    if(!FPaths::FileExists(path))return nullptr;
    const FName resource(*path);
    const auto size=FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(resource);
    if(size.X<=0 || size.Y<=0)return nullptr;
    Images[actor]=MakeShared<FSlateDynamicImageBrush>(resource,FVector2D(size.X,size.Y));
    return Images[actor];
}
