#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TN_LoadingScreenWidget.generated.h"

class UOverlay;
class UTextBlock;

/**
 * @brief Loading screen mostrado entre transiciones de mapa (host -> lobby, lobby -> run, etc.).
 *        Gestionado por UMP_GameInstance::ShowLoadingScreen/HideLoadingScreen.
 */
UCLASS()
class TORTUNABO_API UTN_LoadingScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** @brief Cambia el mensaje de estado mostrado (ej. "Conectando...", "Cargando carrera..."). */
	UFUNCTION(BlueprintCallable, Category = "Loading")
	void SetStatusMessage(const FText& NewMessage);

protected:
	virtual void NativeConstruct() override;

private:
	void EnsureRuntimeLayout();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UOverlay> RootOverlay;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;
};

