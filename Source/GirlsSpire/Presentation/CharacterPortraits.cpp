#include "CharacterPortraits.h"
#include "Domain/CharacterCatalog.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Misc/Paths.h"

FCharacterPortraits::FCharacterPortraits():FCharacterPortraits(FPaths::ProjectContentDir()/TEXT("Art/Characters")) {}
FCharacterPortraits::FCharacterPortraits(FString root):Root(MoveTemp(root)) {}
const TCHAR* FCharacterPortraits::Name(int actor)
{
    if(!GirlsSpire::FindCharacter(actor))return TEXT("不明な人物");
    // 返した文字列の寿命と既存APIを維持し、画像の読込とは独立して名前を参照する。
    static const std::array<FString,9> names=[] {
        std::array<FString,9> values;
        for(int id=0;id<9;++id)values[id]=UTF8_TO_TCHAR(GirlsSpire::FindCharacter(id)->Name.data());
        return values;
    }();
    return *names[actor];
}
TSharedPtr<FSlateDynamicImageBrush> FCharacterPortraits::Get(int actor)
{
    const auto definition=GirlsSpire::FindCharacter(actor);if(!definition)return nullptr;
    if(Images[actor])return Images[actor];
    const auto path=FPaths::ConvertRelativePathToFull(Root/(FString(UTF8_TO_TCHAR(definition->Portrait.data()))+TEXT("-v1.png")));
    if(!FPaths::FileExists(path))return nullptr;
    const FName resource(*path);
    const auto size=FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(resource);
    if(size.X<=0 || size.Y<=0)return nullptr;
    Images[actor]=MakeShared<FSlateDynamicImageBrush>(resource,FVector2D(size.X,size.Y));
    return Images[actor];
}
