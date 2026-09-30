#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TN_TravelFailureSubsystem.generated.h"

class UMP_GameInstance;
class UWorld;

/**
 * @brief Qué pasa cuando un viaje de mapa falla (UEngine::OnTravelFailure: mapa sin cocinar, paquete que falta, URL mala).
 *
 * Sin esto, un ServerTravel fallido dejaba al invitado colgado en la pantalla de carga y sin mensaje (F_gaps_steam N-E).
 * La regla es TNTravel::DecideTravelFailure (tests Tortunabo.Net.TravelFailure):
 *  - El anfitrión, en el primer fallo, recarga el lobby del que salió (ServerTravel); la pantalla de carga lo dice.
 *  - El anfitrión que ya está en un lobby en pie (sin cuenta atrás ni pausa antes de viajar) no viaja: no hay a dónde volver.
 *  - Un invitado, una partida sin red o un segundo fallo seguido: sesión cerrada y al menú, que enseña el aviso.
 *  - Ya en el menú: el aviso, sin viajar, y se cierra la sesión que quedara.
 * Si el anfitrión llega al lobby, el aviso del menú se retira (no saldría horas después). Vive aparte de UMP_GameInstance
 * y solo usa su API pública.
 *
 * Por qué el ServerTravel al lobby espera un tick: en un viaje sin cortes (el de HQ y Run) el motor avisa del fallo DENTRO de
 * AGameModeBase::ProcessServerTravel, cuando World->NextURL aún vale la URL fallida (se vacía al volver), y UWorld::ServerTravel
 * no hace nada si NextURL no está vacío (devuelve true igualmente). Pedido en el acto, el viaje al lobby se perdía y el
 * anfitrión no volvía al lobby (ver abajo adónde lo mandaba el motor). Un tick después NextURL ya está vacío; y como
 * ServerTravel devuelve true aunque no haga nada, se comprueba que el viaje ha arrancado de verdad (DidTravelStart). Si no
 * arranca, se oculta la pantalla de carga y se cae al menú. La pantalla de carga solo se enseña cuando el viaje ya está en marcha.
 *
 * Y el motor ya ha decidido mandar al anfitrión al menú: UEngine::HandleTravelFailure, enlazado antes que este subsistema,
 * llama a UEngine::HandleDisconnect, que deja pedido un viaje a «?closed» (la entrada por defecto) para el próximo tick y le
 * quita ?Listen a la última URL; el LoadMap de ese viaje cancelaría incluso un viaje sin cortes recién arrancado. Por eso, cuando
 * el anfitrión se queda en su sesión (lobby en pie o lobby recargado), este subsistema anula ese viaje y devuelve el ?Listen
 * (KeepHostInSession). (El motor también llama a AGameMode::AbortMatch; en el lobby da igual, nada usa el estado de partida
 * del motor, y al recargarlo el GameMode es uno nuevo.)
 *
 * El lobby "en pie": ATN_HQGameMode::BeginMatchTravel destruye las tortugas ANTES de pedir el viaje y el castillo no les quita
 * el «listo» (Docs/Pantalla_Carga.md), así que tras un lanzamiento fallido el lobby no se puede jugar: se recarga con el mismo
 * ServerTravel con el que se vuelve de una partida (PostSeamlessTravel lo deja todo como nuevo). Solo un lobby sin lanzamiento
 * (nada pasó allí: el fallo vino de otro sitio) se deja como está.
 *
 * Viaje duro (PIE, o ?NoSeamlessTravel): el motor ya ha cerrado el driver de red (los invitados pierden la conexión), destruido
 * el mundo y los controladores y cargado el mapa por defecto (el menú) cuando avisa, y UMP_GameInstance::HandlePostLoadMap ya
 * ha olvidado la sala (ResetRoomState). No queda partida a la que volver: volver al lobby sería alojar una sala nueva desde el
 * menú, sin los datos de la sala y con la carrera por el puerto de escucha de Steam de OnCreateSessionComplete. Por eso el
 * anfitrión se queda en el menú con el aviso y la sesión de Steam que quedara se cierra (sin invitados sería una sala fantasma).
 * En el juego empaquetado HQ y Run viajan sin cortes, así que solo lo recorren PIE y el viaje del menú al lobby.
 */
UCLASS()
class TORTUNABO_API UTN_TravelFailureSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Solo con UMP_GameInstance (la del juego). */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

	/** Mapa cargado: se reinicia la cuenta de fallos y, si no es el menú, se retira el aviso del fallo. */
	void HandlePostLoadMap(UWorld* LoadedWorld);

	/** Un tick después del fallo: el anfitrión pide el ServerTravel al lobby (ver arriba por qué no en el acto). */
	bool TickLobbyTravel(float DeltaTime);

	/** Anula el viaje al lobby que esperaba su tick (otro fallo manda, o el subsistema se apaga). */
	void CancelLobbyTravel();

	/** Cierra la sesión y lleva al menú, que enseña el aviso pendiente. */
	void SendToMenu(UMP_GameInstance& GI, UWorld* World);

	UMP_GameInstance* GetTNGameInstance() const;

	FDelegateHandle TravelFailureHandle;
	FDelegateHandle PostLoadMapHandle;

	/** Tick del viaje al lobby que espera y el mundo desde el que se pedirá. */
	FTSTicker::FDelegateHandle LobbyTravelTicker;
	TWeakObjectPtr<UWorld> LobbyTravelWorld;

	/** Fallos de viaje seguidos desde el último mapa cargado. */
	int32 FailureCount = 0;

	/** Hay un aviso de fallo esperando al menú. */
	bool bNoticePending = false;
};
