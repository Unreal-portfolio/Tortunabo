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

	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void SetPlayerReadyState(APlayerController* PlayerController, bool bReady);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	int32 LobbyExpectedPlayers = 4;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	int32 CountdownStartValue = 3;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	float CinematicDelaySeconds = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	FString MatchMapPath = TEXT("/Game/Maps/Run/LVL_Run");

private:
	FTimerHandle CountdownTimerHandle;
	FTimerHandle TravelTimerHandle;
	bool bCountdownRunning = false;
	int32 CurrentCountdownValue = 0;

	void RefreshLobbyState();
	void StartCountdown();
	void TickCountdown();
	void ResetCountdown();
	void BeginMatchTravel();
	void EnsurePlayerSpawned(APlayerController* PlayerController);
	APlayerStart* EnsureFallbackPlayerStart();
	void SetFlowState(ETNMatchFlowState NewState) const;
};

