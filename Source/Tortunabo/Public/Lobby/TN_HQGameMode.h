#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Core/TN_MatchFlowTypes.h"
#include "TN_HQGameMode.generated.h"

class APlayerController;
class APlayerStart;

UCLASS()
class TORTUNABO_API ATN_HQGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ATN_HQGameMode();

	virtual void BeginPlay() override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	/** Seamless travel: limpiar estado espectador ANTES de que UE intente spawnear. */
	virtual void HandleSeamlessTravelPlayer(AController*& C) override;

	/** Seamless travel: setup final después de que todos los jugadores viajaron. */
	virtual void PostSeamlessTravel() override;

	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void SetPlayerReadyState(APlayerController* PlayerController, bool bReady);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	int32 LobbyExpectedPlayers = 4;

	/**
	 * PlayerStartTag value used to identify spawn points inside the tutorial zone.
	 * Place at least one APlayerStart in LVL_HQ with this tag to enable tutorial routing.
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
	 * Set to true in BeginPlay when the server's GameInstance reports first-time play.
	 * Used by ChoosePlayerStart_Implementation to route all players to the tutorial zone.
	 * Cleared once the tutorial save flag is written, so the next HQ visit spawns normally.
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

	void RefreshLobbyState();
	void StartCountdown();
	void TickCountdown();
	void ResetCountdown();
	void BeginMatchTravel();
	void EnsurePlayerSpawned(APlayerController* PlayerController);
	APlayerStart* EnsureFallbackPlayerStart();
	void SetFlowState(ETNMatchFlowState NewState) const;
};

