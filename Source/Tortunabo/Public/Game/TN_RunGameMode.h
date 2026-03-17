#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Core/TN_MatchFlowTypes.h"
#include "TN_RunGameMode.generated.h"

class APlayerController;

UCLASS()
class TORTUNABO_API ATN_RunGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ATN_RunGameMode();

	virtual void BeginPlay() override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	UFUNCTION(BlueprintCallable, Category = "Run")
	void MarkPlayerFinished(APlayerController* PlayerController);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Run")
	float ResultsDurationSeconds = 8.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Run")
	FString LobbyMapPath = TEXT("/Game/Maps/Lobby/LVL_HQ");

private:
	FTimerHandle ResultsTimerHandle;
	FTimerHandle ResultsCountdownTimerHandle;
	float MatchStartServerTime = 0.f;
	int32 NextFinishRank = 1;
	int32 ResultsCountdownValue = 0;

	void TickResultsCountdown();
	void FinishRoundAndReturnToLobby();
	void SetFlowState(ETNMatchFlowState NewState) const;
};

