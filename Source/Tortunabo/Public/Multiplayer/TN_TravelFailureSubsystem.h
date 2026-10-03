#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Multiplayer/TN_TravelFailureDecisions.h"
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
 * Antes de viajar: ATN_HQGameMode y ATN_RunGameMode (los que viajan sin cortes) comprueban en CanServerTravel que el mapa
 * existe (CanServerTravelTo). Si no, el viaje no empieza: AGameModeBase::ProcessServerTravel no llega a mandar a los invitados
 * al mapa que falta (antes fallaban también ellos y volvían al menú por su cuenta) y el fallo se atiende aquí sin pasar por
 * UEngine::OnTravelFailure, que lo trataría como una desconexión. Lo que se escapa de esa comprobación (un mapa que existe pero
 * no carga, un viaje duro) sigue llegando por UEngine::OnTravelFailure.
 *
 * Todo lo del anfitrión que se queda espera al fotograma siguiente (ScheduleAfterFailure, FCoreDelegates::OnBeginFrame, antes
 * de UEngine::Tick y su TickWorldTravel):
 *  - El ServerTravel al lobby: en un viaje sin cortes el motor avisa del fallo DENTRO de AGameModeBase::ProcessServerTravel,
 *    cuando World->NextURL aún vale la URL fallida, y UWorld::ServerTravel no hace nada si NextURL no está vacío (devuelve true
 *    igualmente). En el fotograma siguiente NextURL ya está vacío; y como ServerTravel devuelve true aunque no haga nada, se comprueba que
 *    el viaje ha arrancado de verdad (DidTravelStart). Si no arranca, se oculta la pantalla de carga y se cae al menú. La
 *    pantalla de carga solo se enseña cuando el viaje ya está en marcha.
 *  - Seguir en la sesión (KeepHostInSession): UEngine::HandleTravelFailure se enlazó antes que este subsistema y Broadcast
 *    recorre los delegados del último al primero, así que corre DESPUÉS: llama a UEngine::HandleDisconnect, que deja pedido un
 *    viaje a «?closed» (la entrada por defecto, el menú) para el próximo tick y le quita ?Listen a la última URL; el LoadMap de
 *    ese viaje cancelaría incluso un viaje sin cortes recién arrancado. Hecho en el acto, el motor lo deshacía justo después
 *    (el anfitrión acababa en el menú y los invitados veían que se había cortado la conexión). En el fotograma siguiente se anula ese viaje
 *    y se devuelve el ?Listen. (El motor también llama a AGameMode::AbortMatch; en el lobby da igual, nada usa el estado de
 *    partida del motor, y al recargarlo el GameMode es uno nuevo.)
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

	/**
	 * Para AGameModeBase::CanServerTravel: false si el mapa de ServerTravel(URL, bAbsolute) no existe; entonces el fallo ya
	 * está atendido (como uno de UEngine::OnTravelFailure, pero sin la desconexión del motor) y el viaje no debe empezar.
	 */
	static bool CanServerTravelTo(UWorld* World, const FString& URL, bool bAbsolute);

	/** true si el paquete del mapa existe (o está cargado), como lo comprueba el motor al viajar. */
	static bool DoesTravelMapExist(FString MapPackage);

private:
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

	/** Mapa cargado: se reinicia la cuenta de fallos y, si no es el menú, se retira el aviso del fallo. */
	void HandlePostLoadMap(UWorld* LoadedWorld);

	/** Deja el aviso del fallo para el menú (lo enseña si se acaba allí). */
	void LeaveMenuNotice(UMP_GameInstance& GI);

	/** Deja para el fotograma siguiente lo que hace el anfitrión que se queda (ver arriba por qué no en el acto). */
	void ScheduleAfterFailure(UWorld* World, TNTravel::ETravelFailureAction Action);

	/** Al empezar el fotograma siguiente al fallo: el anfitrión sigue en su sesión y, si toca, pide el ServerTravel al lobby. */
	void HandleBeginFrameAfterFailure();

	/** ServerTravel al lobby del que se salió; si no arranca, al menú. */
	void TravelHostToLobby(UMP_GameInstance& GI, UWorld& World);

	/** Anula lo que esperaba al fotograma siguiente (otro fallo manda, o el subsistema se apaga). */
	void CancelAfterFailure();

	/** Cierra la sesión y lleva al menú, que enseña el aviso pendiente. */
	void SendToMenu(UMP_GameInstance& GI, UWorld* World);

	UMP_GameInstance* GetTNGameInstance() const;

	FDelegateHandle TravelFailureHandle;
	FDelegateHandle PostLoadMapHandle;

	/** Lo que espera al fotograma siguiente al fallo: el enlace a OnBeginFrame, el mundo y qué hacer. */
	FDelegateHandle AfterFailureHandle;
	TWeakObjectPtr<UWorld> AfterFailureWorld;
	TNTravel::ETravelFailureAction AfterFailureAction = TNTravel::ETravelFailureAction::StayInLobby;

	/** Fallos de viaje seguidos desde el último mapa cargado. */
	int32 FailureCount = 0;

	/** Hay un aviso de fallo esperando al menú. */
	bool bNoticePending = false;
};
