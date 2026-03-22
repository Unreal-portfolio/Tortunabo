#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerVolume.h"
#include "TN_DeathZoneVolume.generated.h"

class APlayerController;
class ATN_RunGameMode;

UCLASS()
class TORTUNABO_API ATN_DeathZoneVolume : public ATriggerVolume
{
	GENERATED_BODY()

public:
	ATN_DeathZoneVolume();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeathZone", meta = (ClampMin = "0.1"))
	float SecondsInsideToDie = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeathZone")
	bool bDestroyOnlyDuringRun = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeathZone", meta = (ClampMin = "0.05"))
	float CountdownTickInterval = 0.1f;

private:
	UFUNCTION()
	void OnZoneBeginOverlap(AActor* OverlappedActor, AActor* OtherActor);

	UFUNCTION()
	void OnZoneEndOverlap(AActor* OverlappedActor, AActor* OtherActor);

	void HandlePlayerDeath(APlayerController* PlayerController);
	/** Single shared timer that ticks ALL players inside the zone at once. */
	void TickAllCountdowns();
	ATN_RunGameMode* ResolveRunGameMode() const;

	/** Single shared timer handle — replaces N per-player timers. */
	FTimerHandle SharedCountdownTimerHandle;

	TMap<TWeakObjectPtr<APlayerController>, float> PendingDeathRemaining;
};

