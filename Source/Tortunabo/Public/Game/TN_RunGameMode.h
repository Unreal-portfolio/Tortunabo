#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Core/TN_MatchFlowTypes.h"
#include "TN_RunGameMode.generated.h"

class APlayerController;
class APlayerStart;

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
	float DBNOBleedoutSeconds = 15.f;

	/** Brief invulnerability after being revived (prevents instant re-death in death zones). */
	UPROPERTY(EditDefaultsOnly, Category = "Run|DBNO", meta = (ClampMin = "0.0"))
	float ReviveImmunitySeconds = 2.f;

private:
	FTimerHandle ResultsTimerHandle;
	FTimerHandle ResultsCountdownTimerHandle;
	FTimerHandle WaitingTimeoutTimerHandle;
	FTimerHandle DeferredTravelTimerHandle;
	FTimerHandle DBNOBleedoutTimerHandle;
	float MatchStartServerTime = 0.f;
	int32 NextFinishRank = 1;
	int32 ResultsCountdownValue = 0;
	FString PendingTravelURL;

	/** Cuántos jugadores esperamos del lobby (leído de GameInstance). */
	int32 ExpectedPlayersFromLobby = 1;

	/** true cuando ya transicionamos a InProgress. */
	bool bMatchStarted = false;

	/** Players currently in DBNO with their remaining bleedout time. */
	TMap<TWeakObjectPtr<APlayerController>, float> DBNOPlayers;

	/** Players with active post-revive immunity timers. */
	TSet<TWeakObjectPtr<APlayerController>> ReviveImmunePlayers;

	void EnsurePlayerSpawned(APlayerController* PlayerController);
	APlayerStart* EnsureFallbackPlayerStart();
	void TickResultsCountdown();
	void UpdateRoundProgressAndMaybeFinish();
	void MovePlayerToSpectator(APlayerController* PlayerController) const;
	void FinishRoundAndReturnToLobby();
	void ExecuteDeferredTravel();
	void SetFlowState(ETNMatchFlowState NewState) const;

	/** Tick all DBNO bleedout timers (shared, 0.1s interval). */
	void TickDBNOBleedout();

	/** If all alive players are in DBNO, kill them all (no one can revive). */
	void CheckAllAliveDBNO();

	/** Comprueba si ya llegaron todos los jugadores esperados y arranca la carrera. */
	void TryStartMatch();

	/** Timeout: arranca la carrera aunque no hayan llegado todos. */
	void OnWaitingTimeout();
};

