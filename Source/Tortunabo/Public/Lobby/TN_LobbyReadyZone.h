#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerBox.h"
#include "TN_LobbyReadyZone.generated.h"

class ATN_HQGameMode;

UCLASS()
class TORTUNABO_API ATN_LobbyReadyZone : public ATriggerBox
{
	GENERATED_BODY()

public:
	ATN_LobbyReadyZone();

	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnZoneBeginOverlap(AActor* OverlappedActor, AActor* OtherActor);

	UFUNCTION()
	void OnZoneEndOverlap(AActor* OverlappedActor, AActor* OtherActor);

	void HandlePawnEnter(APawn* Pawn);
	void HandlePawnExit(APawn* Pawn);
	ATN_HQGameMode* ResolveHQGameMode() const;
};

