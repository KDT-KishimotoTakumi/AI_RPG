#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include <memory>
#include "GameEntry.generated.h"

class SRunScreen;
class UGameViewportClient;
namespace GirlsSpire {class DiskSlotFiles;class SaveTransaction;}
namespace GirlsSpire {FSimpleDelegate GameExitAction(bool gameWorld,bool inEditor);}

UCLASS()
class GIRLSSPIRE_API AGirlsSpireController : public APlayerController
{
    GENERATED_BODY()
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type reason) override;
private:
    std::shared_ptr<GirlsSpire::DiskSlotFiles> Files;
    std::shared_ptr<GirlsSpire::SaveTransaction> Service;
    TSharedPtr<SRunScreen> Screen;
    TWeakObjectPtr<UGameViewportClient> OwnerViewport;
};

UCLASS()
class GIRLSSPIRE_API AGirlsSpireGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AGirlsSpireGameMode();
};
