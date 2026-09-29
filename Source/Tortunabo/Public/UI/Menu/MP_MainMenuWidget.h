#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Menu/TN_RoomMenuWidget.h"
#include "World/ProcMap/TN_ProcMapEnums.h"
#include "MP_MainMenuWidget.generated.h"

class UButton;
class UTextBlock;

/**
 * @brief Widget del menú principal (WBP_MainMenuWidget: botones Host / Find / Quit y log de status).
 *
 * Los tres botones del Blueprint (mismo estilo visual; aquí solo cambian sus textos): «Crear partida», «Unirse» y «Salir».
 * Crear y Unirse abren las pantallas de salas (UTN_RoomMenuWidget, montadas en código encima de este widget, que se
 * esconde mientras tanto; Docs/Salas.md):
 *  - Crear partida: modo (Cooperativo o Carrera), pública o privada, 4, 6 u 8 plazas, nombre al azar y, en la privada,
 *    su código. El modo va a UMP_GameInstance::SelectedProcMode (HostRoom) y sobrevive al viaje.
 *  - Unirse: con un código o de la lista de salas públicas. Unirse no toca el modo (lo decide el anfitrión).
 *
 * Se suscribe al delegate OnStatusChanged del UMP_GameInstance para reflejar estado de sesión y errores en StatusText, y
 * al llegar enseña el aviso que haya dejado la GameInstance (expulsado, sala cerrada o llena, el anfitrión se fue...).
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
	/** «Crear partida»: abre la pantalla de crear sala. */
	UFUNCTION()
	void OnHostClicked();

	/** «Unirse»: abre la pantalla de unirse (código y lista de salas públicas). */
	UFUNCTION()
	void OnFindClicked();

	/** «Salir»: cierra el juego. */
	UFUNCTION()
	void OnQuitClicked();

	UFUNCTION()
	void OnGameInstanceStatusChanged(const FString& StatusMessage);

	void SetStatus(const FString& Message);

	/** Abre una pantalla de salas (la crea la primera vez). */
	void OpenRooms(ETNRoomMenuPage Page);

	/** La pantalla de salas se abre o se cierra: este menú se esconde o vuelve (con el foco en su botón). */
	void HandleRoomsOpenChanged(bool bOpen);

	/** Estado del GameInstance (o el saludo si aún no hay nada). */
	FString BuildIdleStatus() const;

	/** Pantallas de salas (widget propio en la pantalla, encima de este). */
	UPROPERTY(Transient)
	TObjectPtr<UTN_RoomMenuWidget> RoomMenu;

	/** Visibilidad de este menú antes de abrir las salas (para devolverla al cerrarlas). */
	ESlateVisibility VisibilityBeforeRooms = ESlateVisibility::SelfHitTestInvisible;

	/** Botón que abrió las salas (el foco vuelve a él). */
	TWeakObjectPtr<UButton> RoomsOpener;
};
