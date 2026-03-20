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

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Run")
	float ResultsDurationSeconds = 8.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Run")
	FString LobbyMapPath = TEXT("/Game/Maps/Lobby/LVL_HQ");

	/** Segundos máximos esperando a que todos los jugadores reconecten tras el travel. */
	UPROPERTY(EditDefaultsOnly, Category = "Run|Staging")
	float WaitingForPlayersTimeoutSeconds = 15.0f;

private:
	FTimerHandle ResultsTimerHandle;
	FTimerHandle ResultsCountdownTimerHandle;
	FTimerHandle WaitingTimeoutTimerHandle;
	FTimerHandle DeferredTravelTimerHandle;
	float MatchStartServerTime = 0.f;
	int32 NextFinishRank = 1;
	int32 ResultsCountdownValue = 0;
	FString PendingTravelURL;

	/** Cuántos jugadores esperamos del lobby (leído de GameInstance). */
	int32 ExpectedPlayersFromLobby = 1;

	/** true cuando ya transicionamos a InProgress. */
	bool bMatchStarted = false;

	void EnsurePlayerSpawned(APlayerController* PlayerController);
	APlayerStart* EnsureFallbackPlayerStart();
	void TickResultsCountdown();
	void UpdateRoundProgressAndMaybeFinish();
	void MovePlayerToSpectator(APlayerController* PlayerController) const;
	void FinishRoundAndReturnToLobby();
	void ExecuteDeferredTravel();
	void SetFlowState(ETNMatchFlowState NewState) const;

	/** Comprueba si ya llegaron todos los jugadores esperados y arranca la carrera. */
	void TryStartMatch();

	/** Timeout: arranca la carrera aunque no hayan llegado todos. */
	void OnWaitingTimeout();
};

