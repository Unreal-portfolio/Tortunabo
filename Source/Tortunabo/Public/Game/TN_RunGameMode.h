#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Core/TN_MatchFlowTypes.h"
#include "TN_RunGameMode.generated.h"

class APlayerController;
class APlayerStart;
class ATN_RescuePickup;
class ATN_CollectionZone;

/**
 * @brief GameMode de la fase Run (carrera). Orquesta el ciclo Countdown -> Race -> Results -> retorno al lobby.
 *
 * Responsabilidades:
 *  - Spawnea jugadores en LVL_Run tras Seamless Travel desde LVL_HQ.
 *  - Gestiona la línea de meta (MarkPlayerFinished) y las muertes (MarkPlayerDead).
 *  - Sistema DBNO (Down But Not Out) con bleedout timer e inmunidad post-revive.
 *  - Reloj de carrera replicado y countdown del pantalla de Resultados.
 *  - Sistema de puntuación: RankScore (podio) + ScorePickups + TimeBonus.
 *  - Retorno a LVL_HQ vía Seamless Travel cuando expira el timer de resultados.
 */
UCLASS()
class TORTUNABO_API ATN_RunGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ATN_RunGameMode();

	/** @brief Inicializa timers, reloj de carrera y bindings con CollectionZones presentes en el mapa. */
	virtual void BeginPlay() override;

	/** @brief Llamado por UE cuando un PlayerController NUEVO entra (no por seamless travel). */
	virtual void PostLogin(APlayerController* NewPlayer) override;

	/** @brief Limpieza al salir de un jugador: libera timers de DBNO/inmunidad y rescata pickups. */
	virtual void Logout(AController* Exiting) override;

	/** @brief Selecciona un PlayerStart libre evitando reusar el mismo en spawn paralelo. */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	/** @brief Hook de UE post-spawn del pawn: aplica cosméticos y reglas iniciales del jugador. */
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	/**
	 * @brief Seamless travel: limpia estado de espectador del PC ANTES de que UE intente respawnearlo.
	 * @note Evita que un jugador eliminado en la run anterior llegue al lobby ya en modo espectador.
	 */
	virtual void HandleSeamlessTravelPlayer(AController*& C) override;

	/**
	 * @brief Setup final tras Seamless Travel cuando todos los jugadores han llegado.
	 *        Lanza countdown si ya estamos completos, o espera con timeout.
	 */
	virtual void PostSeamlessTravel() override;

	/**
	 * @brief Marca a un jugador como finalizado (cruzó la meta) y le asigna el siguiente FinishRank.
	 * @param PlayerController Controller del jugador que terminó la carrera.
	 * @note Server-only. Si era el último jugador activo, dispara el flujo de Resultados.
	 */
	UFUNCTION(BlueprintCallable, Category = "Run")
	void MarkPlayerFinished(APlayerController* PlayerController);

	/**
	 * @brief Marca a un jugador como muerto, lo pasa a espectador y spawnea su RescuePickup.
	 * @param PlayerController Controller del jugador eliminado.
	 * @note Server-only. Guarda el pawn en DeadPlayerPawns para que el rescate lo pueda teletransportar.
	 */
	UFUNCTION(BlueprintCallable, Category = "Run")
	void MarkPlayerDead(APlayerController* PlayerController);

	/**
	 * @brief Callback server-side cuando una CollectionZone alcanza su RequiredCount.
	 *        Bindeado en BeginPlay vía OnZoneGoalReached. Suma GoalReachedBonusScore
	 *        al RaceScore de todos los jugadores activos (no eliminados).
	 * @param Zone CollectionZone que dispara el evento.
	 */
	void HandleCollectionZoneGoal(ATN_CollectionZone* Zone);

	/**
	 * @brief Pone a un jugador en estado DBNO (knockdown + bleedout timer).
	 * @param PlayerController Jugador que entra en DBNO.
	 * @note Server-only. Si todos los vivos están en DBNO, dispara CheckAllAliveDBNO -> muerte global.
	 */
	UFUNCTION(BlueprintCallable, Category = "Run|DBNO")
	void EnterDBNO(APlayerController* PlayerController);

	/**
	 * @brief Revive a un jugador que está actualmente en DBNO.
	 *        Llamado por el servidor cuando un compañero completa el canal de revive.
	 * @param PlayerController Jugador a revivir.
	 */
	UFUNCTION(BlueprintCallable, Category = "Run|DBNO")
	void RevivePlayer(APlayerController* PlayerController);

	/**
	 * @brief Devuelve el pawn cacheado de un jugador muerto (guardado antes del UnPossess).
	 * @param PlayerId ID del PlayerState del jugador eliminado.
	 * @return Pawn cacheado o nullptr si no hay registro.
	 * @note Usado por TN_RescuePickup::Interact para teletransportar el pawn al lugar del rescate.
	 */
	APawn* GetDeadPlayerPawn(int32 PlayerId) const;

	/**
	 * @brief Indica si el jugador tiene inmunidad post-revive activa.
	 * @param PC PlayerController a consultar.
	 * @return true mientras dure ReviveImmunitySeconds desde el revive.
	 * @note Consultado por TN_DeathZoneVolume para no iniciar el countdown
	 *       mientras el jugador recién revivido está protegido.
	 */
	bool IsPlayerReviveImmune(APlayerController* PC) const;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Run")
	float ResultsDurationSeconds = 8.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Run")
	FString LobbyMapPath = TEXT("/Game/Maps/Lobby/LVL_HQ");

	/** Segundos máximos esperando a que todos los jugadores reconecten tras el travel. */
	UPROPERTY(EditDefaultsOnly, Category = "Run|Staging")
	float WaitingForPlayersTimeoutSeconds = 15.0f;

	/** Segundos que un jugador DBNO tiene antes de morir definitivamente por bleedout. */
	UPROPERTY(EditDefaultsOnly, Category = "Run|DBNO", meta = (ClampMin = "3.0"))
	float DBNOBleedoutSeconds = 8.f;

	/** Invulnerabilidad breve tras un revive (evita re-muerte inmediata en death zones). */
	UPROPERTY(EditDefaultsOnly, Category = "Run|DBNO", meta = (ClampMin = "0.0"))
	float ReviveImmunitySeconds = 2.f;

	/**
	 * Clase de pickup de rescate que se spawnea al morir un jugador.
	 * Cuando un compañero interactúa con él, revive al muerto en esa posición.
	 * Asignar en BP_RunGameMode → Class Defaults.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Run|Death")
	TSubclassOf<ATN_RescuePickup> RescuePickupClass;

	// ── Sistema de puntuación final ─────────────────────────────────────────
	// RaceScore final = RankScore (podio) + ScorePickups recogidos en la run + TimeBonus.
	// RankScore: 1º=400, 2º=300, 3º=200, 4º=100, resto=50. Eliminados=0.
	// TimeBonus = max(0, (TimeBonusBaselineSeconds − FinishTime) × PointsPerSecond).

	/** Segundos baseline para el time bonus. Tiempos por debajo otorgan puntos. */
	UPROPERTY(EditDefaultsOnly, Category = "Run|Scoring", meta = (ClampMin = "10.0"))
	float TimeBonusBaselineSeconds = 120.f;

	/** Puntos por cada segundo bajo el baseline. 0 = sin time bonus. */
	UPROPERTY(EditDefaultsOnly, Category = "Run|Scoring", meta = (ClampMin = "0.0"))
	float TimeBonusPointsPerSecond = 5.f;

private:
	FTimerHandle ResultsTimerHandle;
	FTimerHandle ResultsCountdownTimerHandle;
	FTimerHandle WaitingTimeoutTimerHandle;
	FTimerHandle DBNOBleedoutTimerHandle;
	FTimerHandle RaceClockTimerHandle;
	float MatchStartServerTime = 0.f;
	int32 NextFinishRank = 1;
	int32 ResultsCountdownValue = 0;

	/** Cuántos jugadores esperamos del lobby (leído de GameInstance). */
	int32 ExpectedPlayersFromLobby = 1;

	/** true cuando ya transicionamos a InProgress. */
	bool bMatchStarted = false;

	/** Jugadores actualmente en DBNO con su tiempo de bleedout restante. */
	TMap<TWeakObjectPtr<APlayerController>, float> DBNOPlayers;

	/** Jugadores con inmunidad post-revive activa. */
	TSet<TWeakObjectPtr<APlayerController>> ReviveImmunePlayers;

	/** Handle de timer de inmunidad por jugador — permite cancelar en re-revive rápido. */
	TMap<TWeakObjectPtr<APlayerController>, FTimerHandle> ImmunityTimers;

	/** Rescue pickups spawnados para jugadores muertos. Key = PlayerId. */
	TMap<int32, TWeakObjectPtr<ATN_RescuePickup>> RescuePickups;

	/**
	 * Pawns de jugadores muertos. Key = PlayerId.
	 * Guardamos la referencia ANTES de que EnterSpectateMode haga UnPossess,
	 * porque tras entrar en espectador, PC->GetPawn() devuelve nullptr.
	 * RevivePlayer y TN_RescuePickup usan esto para recuperar el pawn.
	 */
	TMap<int32, TWeakObjectPtr<APawn>> DeadPlayerPawns;

	/** @brief Garantiza que el jugador tenga un pawn vivo en el mapa (spawnea si falta). */
	void EnsurePlayerSpawned(APlayerController* PlayerController);

	/** @brief Devuelve un PlayerStart cualquiera como fallback si no hay otros disponibles. */
	APlayerStart* EnsureFallbackPlayerStart();

	/** @brief Tick (1Hz) que decrementa el contador de Resultados y dispara FinishRoundAndReturnToLobby al llegar a 0. */
	void TickResultsCountdown();

	/** @brief Comprueba si la ronda terminó (todos los vivos cruzaron meta o murieron) y arranca Resultados. */
	void UpdateRoundProgressAndMaybeFinish();

	/** @brief Mueve a un jugador a modo espectador (UnPossess + spectate next alive). */
	void MovePlayerToSpectator(APlayerController* PlayerController) const;

	/** @brief Cierra la ronda: persiste scores, dispara Seamless Travel de vuelta a LVL_HQ. */
	void FinishRoundAndReturnToLobby();

	/** @brief Cambia el MatchFlowState en el GameState replicado + broadcast a listen-server. */
	void SetFlowState(ETNMatchFlowState NewState) const;

	/** @brief Tick compartido (0.1s) de todos los bleedouts DBNO activos. */
	void TickDBNOBleedout();

	/** @brief Si todos los jugadores vivos están en DBNO, los mata a todos (nadie puede revivir). */
	void CheckAllAliveDBNO();

	/** @brief Comprueba si llegaron todos los jugadores esperados y, si sí, arranca el countdown. */
	void TryStartMatch();

	/** @brief Timeout de staging: arranca la carrera aunque no hayan llegado todos. */
	void OnWaitingTimeout();

	/** @brief Tick periódico (0.5s) que actualiza ServerMatchElapsedTime en el GameState. */
	void TickRaceClock();
};
