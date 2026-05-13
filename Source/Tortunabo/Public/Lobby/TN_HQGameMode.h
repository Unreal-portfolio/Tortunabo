#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Core/TN_MatchFlowTypes.h"
#include "TN_HQGameMode.generated.h"

class APlayerController;
class APlayerStart;

/**
 * @brief GameMode del lobby HQ (LVL_HQ). Gestiona ready-up, countdown y travel al mapa Run.
 *
 * Responsabilidades:
 *  - Spawn de jugadores (con desvío al tutorial la primera partida del listen-server).
 *  - Lectura del estado ready (ATN_LobbyReadyZone) y countdown cuando todos los conectados están listos.
 *  - Reseteo del countdown si alguien sale de la zona o se desconecta.
 *  - Seamless Travel hacia LVL_Run con persistencia de PendingTravelPlayerCount en GameInstance.
 */
UCLASS()
class TORTUNABO_API ATN_HQGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ATN_HQGameMode();

	/** @brief Inicializa estado y consulta al GameInstance si toca enrutar al tutorial. */
	virtual void BeginPlay() override;

	/** @brief Selecciona un PlayerStart: el del tutorial si bShouldUseTutorialStart, si no uno libre. */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	/** @brief Hook post-spawn: aplica cosméticos guardados (helmet/skin) al pawn recién creado. */
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	/** @brief Login de jugador nuevo (no por seamless travel): registra como conectado. */
	virtual void PostLogin(APlayerController* NewPlayer) override;

	/** @brief Logout: actualiza ConnectedPlayers en GameState y resetea countdown si quedan pocos. */
	virtual void Logout(AController* Exiting) override;

	/**
	 * @brief Seamless travel: limpia estado de espectador del PC ANTES de que UE intente respawnearlo.
	 * @note Evita que un jugador eliminado en la run anterior llegue al lobby ya en modo espectador.
	 */
	virtual void HandleSeamlessTravelPlayer(AController*& C) override;

	/**
	 * @brief Setup final tras seamless travel: re-aplica cosméticos a todos los jugadores que han vuelto.
	 *        Usa retry timer (5 × 0.1s) para cubrir la race PlayerState/Pawn possession.
	 */
	virtual void PostSeamlessTravel() override;

	/**
	 * @brief Marca el estado ready/not-ready de un jugador y refresca el countdown del lobby.
	 * @param PlayerController Jugador cuyo estado cambia.
	 * @param bReady true = entrar en zona ready; false = salir.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void SetPlayerReadyState(APlayerController* PlayerController, bool bReady);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	int32 LobbyExpectedPlayers = 4;

	/** Mínimo de jugadores conectados para que el countdown pueda arrancar. Default=1 para pruebas en solitario. */
	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	int32 LobbyMinPlayersForStart = 1;

	/**
	 * PlayerStartTag usado para identificar spawn points dentro de la zona de tutorial.
	 * Coloca al menos un APlayerStart en LVL_HQ con este tag para habilitar el enrutado al tutorial.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Tutorial")
	FName TutorialStartTag = TEXT("TutorialStart");

	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	int32 CountdownStartValue = 3;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	float CinematicDelaySeconds = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	FString MatchMapPath = TEXT("/Game/Maps/Run/LVL_Run");

private:
	/**
	 * Puesto a true en BeginPlay cuando el GameInstance del servidor reporta primera partida.
	 * ChoosePlayerStart_Implementation lo usa para enrutar a TODOS los jugadores al tutorial.
	 * Se limpia una vez se persiste el flag, de modo que la siguiente visita al HQ spawnee normal.
	 */
	bool bShouldUseTutorialStart = false;

	FTimerHandle CountdownTimerHandle;
	FTimerHandle TravelTimerHandle;
	bool bCountdownRunning = false;
	int32 CurrentCountdownValue = 0;

	/**
	 * Evalúa el flag de tutorial en el GameInstance UNA SOLA VEZ por sesión de HQ.
	 * Si es la primera partida activa bShouldUseTutorialStart y persiste el flag.
	 * Idempotente: llamadas posteriores no hacen nada si ya está activado.
	 */
	void CheckAndSetTutorialFlag();

	/** @brief Recalcula ConnectedPlayers/PlayersInStartZone y decide si arrancar/parar el countdown. */
	void RefreshLobbyState();

	/** @brief Arranca el countdown si están las condiciones (todos ready, mínimo conectados). */
	void StartCountdown();

	/** @brief Tick 1Hz del countdown. Al llegar a 0 dispara BeginMatchTravel. */
	void TickCountdown();

	/** @brief Para el countdown y vuelve al estado WaitingForPlayers (alguien dejó la zona ready). */
	void ResetCountdown();

	/** @brief Persiste PendingTravelPlayerCount en GameInstance y hace ServerTravel a MatchMapPath. */
	void BeginMatchTravel();

	/** @brief Spawnea el pawn del jugador si todavía no lo tiene (cubre re-entries tras travel). */
	void EnsurePlayerSpawned(APlayerController* PlayerController);

	/** @brief Devuelve un PlayerStart libre como fallback si no hay otros candidatos. */
	APlayerStart* EnsureFallbackPlayerStart();

	/** @brief Cambia el MatchFlowState replicado + dispara broadcast manual a listen-server. */
	void SetFlowState(ETNMatchFlowState NewState) const;
};
