#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MP_MenuPlayerController.generated.h"

class UUserWidget;

/**
 * @brief PlayerController del mapa de menú principal.
 *
 * Crea y muestra el widget MainMenuWidget (MP_MainMenuWidget) en BeginPlay
 * y lo destruye en EndPlay. No procesa input de gameplay.
 */
UCLASS()
class TORTUNABO_API AMP_MenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMP_MenuPlayerController();

protected:
	/** @brief Crea el MainMenuWidget, lo añade al viewport y activa cursor del ratón. */
	virtual void BeginPlay() override;

	/** @brief Destruye el MainMenuWidget al salir del mapa. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> MainMenuWidgetClass;

private:
	UPROPERTY()
	TObjectPtr<UUserWidget> MainMenuWidget;
};
