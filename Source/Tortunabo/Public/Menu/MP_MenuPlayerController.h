#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MP_MenuPlayerController.generated.h"

class UUserWidget;

UCLASS()
class TORTUNABO_API AMP_MenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMP_MenuPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> MainMenuWidgetClass;

private:
	UPROPERTY()
	TObjectPtr<UUserWidget> MainMenuWidget;
};

