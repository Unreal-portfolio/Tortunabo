#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Core/TN_MatchFlowTypes.h"
#include "TN_RunGameMode.generated.h"

class APlayerController;
class APlayerStart;
class ATN_RescuePickup;

UCLASS()
class TORTUNABO_API ATN_RunGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ATN_RunGameMode();

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	/** Seamless travel: limpiar estado espectador ANTES de que UE intente spawnear. */
	virtual void HandleSeamlessTravelPlayer(AController*& C) override;

	/** Seamless travel: setup final después de que todos los jugadores viajaron. */
	virtual void PostSeamlessTravel() override;

	UFUNCTION(BlueprintCallable, Category = "Run")
	void MarkPlayerFinished(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Run")
	void MarkPlayerDead(APlayerController* PlayerController);

	/** Put a player into Down But Not Out state (knocked + bleedout timer). */
	UFUNCTION(BlueprintCallable, Category = "Run|DBNO")
	void EnterDBNO(APlayerController* PlayerController);

	/**
	 * Revive a player that is currently in DBNO state.
	 * Called by the server when a teammate successfully completes a revive channel.
	 */
	UFUNCTION(BlueprintCallable, Category = "Run|DBNO")
	void RevivePlayer(APlayerController* PlayerController);

	/**
	 * Devuelve el pawn guardado de un jugador muerto (almacenado antes del UnPossess).
	 * Usado por TN_RescuePickup::Interact para teletransportar el pawn.
	 */
	APawn* GetDeadPlayerPawn(int32 PlayerId) const;

	/**
	 * true si el jugador tiene inmunidad post-revive activa.
	 * Consultado por TN_DeathZoneVolume para no iniciar el countdown
	 * mientras el jugador recién revivido está protegido.
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

	/** How many seconds a DBNO player has before bleeding out and dying for real. */
	UPROPERTY(EditDefaultsOnly, Category = "Run|DBNO", meta = (ClampMin = "3.0"))
	float DBNOBleedoutSeconds = 8.f;

	/** Brief invulnerability after being revived (prevents instant re-death in death zones). */
	UPROPERTY(EditDefaultsOnly, Category = "Run|DBNO", meta = (ClampMin = "0.0"))
	float ReviveImmunitySeconds = 2.f;

	/**
	 * Clase de pickup de rescate que se spawnea al morir un jugador.
	 * Cuando un compañero interactúa con él, revive al muerto en esa posición.
	 * Asignar en BP_RunGameMode → Class Defaults.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Run|Death")
	TSubclassOf<ATN_RescuePickup> RescuePickupClass;

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

	/** Players currently in DBNO with their remaining bleedout time. */
	TMap<TWeakObjectPtr<APlayerController>, float> DBNOPlayers;

	/** Players with active post-revive immunity timers. */
	TSet<TWeakObjectPtr<APlayerController>> ReviveImmunePlayers;

	/** Active immunity timer handle per player — allows cancellation on rapid re-revive. */
	TMap<TWeakObjectPtr<APlayerController>, FTimerHandle> ImmunityTimers;

	/** Rescue pickups spawned for dead players. Key = PlayerId. */
	TMap<int32, TWeakObjectPtr<ATN_RescuePickup>> RescuePickups;

	/**
	 * Pawns de jugadores muertos. Key = PlayerId.
	 * Guardamos la referencia ANTES de que EnterSpectateMode haga UnPossess,
	 * porque tras entrar en espectador, PC->GetPawn() devuelve nullptr.
	 * RevivePlayer y TN_RescuePickup usan esto para recuperar el pawn.
	 */
	TMap<int32, TWeakObjectPtr<APawn>> DeadPlayerPawns;

	void EnsurePlayerSpawned(APlayerController* PlayerController);
	APlayerStart* EnsureFallbackPlayerStart();
	void TickResultsCountdown();
	void UpdateRoundProgressAndMaybeFinish();
	void MovePlayerToSpectator(APlayerController* PlayerController) const;
	void FinishRoundAndReturnToLobby();
	void SetFlowState(ETNMatchFlowState NewState) const;

	/** Tick all DBNO bleedout timers (shared, 0.1s interval). */
	void TickDBNOBleedout();

	/** If all alive players are in DBNO, kill them all (no one can revive). */
	void CheckAllAliveDBNO();

	/** Comprueba si ya llegaron todos los jugadores esperados y arranca la carrera. */
	void TryStartMatch();

	/** Timeout: arranca la carrera aunque no hayan llegado todos. */
	void OnWaitingTimeout();

	/** Tick periódico (0.5s) que actualiza ServerMatchElapsedTime en el GameState. */
	void TickRaceClock();
};

