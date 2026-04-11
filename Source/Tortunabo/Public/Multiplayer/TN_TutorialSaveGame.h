#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "TN_TutorialSaveGame.generated.h"

/**
 * Persists the player's tutorial completion state across sessions.
 * Stored locally on each machine — the listen-server's flag drives
 * first-time spawn routing in TN_HQGameMode.
 */
UCLASS()
class TORTUNABO_API UTN_TutorialSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/**
	 * true once the player has been spawned in the tutorial zone for the first time.
	 * Set to true immediately upon first spawn, so subsequent sessions use normal spawn.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Tutorial")
	bool bHasCompletedTutorial = false;
};
