#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerBox.h"
#include "TN_LobbyReadyZone.generated.h"

class ATN_HQGameMode;

/**
 * @brief Zona ready del lobby. Cualquier pawn dentro marca a su jugador como ready.
 *
 * Listen-server-only: los overlaps se evalúan en autoridad y se reenvían al
 * ATN_HQGameMode vía SetPlayerReadyState. Cuando todos los jugadores conectados
 * están dentro de zonas ready, arranca el countdown.
 */
UCLASS()
class TORTUNABO_API ATN_LobbyReadyZone : public ATriggerBox
{
	GENERATED_BODY()

public:
	ATN_LobbyReadyZone();

	/** @brief Bindea los delegates de overlap del TriggerBox. */
	virtual void BeginPlay() override;

private:
	/** @brief Callback de overlap: si el actor es un pawn de jugador, llama a HandlePawnEnter. */
	UFUNCTION()
	void OnZoneBeginOverlap(AActor* OverlappedActor, AActor* OtherActor);

	/** @brief Callback de fin de overlap: dispara HandlePawnExit si el actor era un pawn. */
	UFUNCTION()
	void OnZoneEndOverlap(AActor* OverlappedActor, AActor* OtherActor);

	/** @brief Marca al PC dueño del pawn como ready en el HQ GameMode. */
	void HandlePawnEnter(APawn* Pawn);

	/** @brief Marca al PC dueño del pawn como no-ready en el HQ GameMode. */
	void HandlePawnExit(APawn* Pawn);

	/** @brief Resuelve el ATN_HQGameMode del mundo actual (server-only, devuelve nullptr en cliente). */
	ATN_HQGameMode* ResolveHQGameMode() const;
};
