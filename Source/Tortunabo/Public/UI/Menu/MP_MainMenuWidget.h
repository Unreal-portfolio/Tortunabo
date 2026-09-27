#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "World/ProcMap/TN_ProcMapEnums.h"
#include "MP_MainMenuWidget.generated.h"

class UButton;
class UTextBlock;

/**
 * @brief Widget del menú principal (WBP_MainMenuWidget: botones Host / Find / Quit y log de status).
 *
 * Dos pasos con los mismos tres botones del Blueprint (mismo estilo visual; aquí solo cambian sus textos):
 *  - Principal: «Crear partida», «Unirse» y «Salir».
 *  - Al crear, el modo: «Cooperativo» (lobby del castillo y mapa procedural), «Carrera» (todos contra todos en la
 *    playa) y «Volver». El modo va a UMP_GameInstance::SelectedProcMode (HostSessionWithMode) y sobrevive al viaje.
 * Unirse no pregunta el modo: lo decide el anfitrión.
 *
 * Se suscribe al delegate OnStatusChanged del UMP_GameInstance para reflejar
 * estado de sesión y errores en StatusText.
 */
UCLASS()
class TORTUNABO_API UMP_MainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> HostButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> FindButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> QuitButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

private:
	/** Principal: pasa a elegir el modo. Eligiendo modo: Cooperativo. */
	UFUNCTION()
	void OnHostClicked();

	/** Principal: unirse a una partida. Eligiendo modo: Carrera. */
	UFUNCTION()
	void OnFindClicked();

	/** Principal: salir del juego. Eligiendo modo: volver al paso principal. */
	UFUNCTION()
	void OnQuitClicked();

	UFUNCTION()
	void OnGameInstanceStatusChanged(const FString& StatusMessage);

	void SetStatus(const FString& Message);

	/** Cambia de paso: textos de los tres botones y del estado. */
	void ShowModeChoice(bool bChoose);

	/** Crea la partida en ese modo (UMP_GameInstance::HostSessionWithMode). */
	void HostWithMode(ETNProcGameMode Mode);

	/** Estado del GameInstance (o el saludo si aún no hay nada). */
	FString BuildIdleStatus() const;

	/** true mientras se elige el modo de la partida que se va a crear. */
	bool bChoosingMode = false;
};
