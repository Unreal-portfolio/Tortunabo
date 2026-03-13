#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Core/TN_MatchFlowTypes.h"
#include "TN_HQGameMode.generated.h"

class APlayerController;

UCLASS()
class TORTUNABO_API ATN_HQGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ATN_HQGameMode();

	virtual void BeginPlay() override;
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
	FString MatchMapPath = TEXT("/Engine/Maps/Templates/OpenWorld");

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
	void SetFlowState(ETNMatchFlowState NewState) const;
};

