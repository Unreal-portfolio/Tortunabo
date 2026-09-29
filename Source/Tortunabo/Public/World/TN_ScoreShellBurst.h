#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_ScoreShellBurst.generated.h"

class UPointLightComponent;
class USceneComponent;

/**
 * Estallido de una concha de puntos recogida, solo visual y local (no se replica): lo crea cada máquina con pantalla
 * al recibir ATN_CoopPlayerState::MulticastScoreShellCollected, así que todos ven el destello y oyen el «¡plin!» en
 * el sitio aunque la concha ya se haya destruido.
 *  - Destello: bola blanda del color de la concha que se abre y se apaga en 0,25 s y un fogonazo de luz sin sombras.
 *  - Anillo que se abre en horizontal, chispas que saltan hacia arriba y caen y, en las grandes y las reinas,
 *    estrellitas que suben despacio.
 *  - «¡Plin!» sintetizado según el tamaño (UTN_ScoreShellSynthComponent, 3D).
 * Todo crece con el tamaño de la concha y el actor se borra solo en 1,2-3 s. Nada en un servidor dedicado ni a más de
 * 150 m de la cámara local.
 */
UCLASS(NotBlueprintable)
class TORTUNABO_API ATN_ScoreShellBurst : public AActor
{
	GENERATED_BODY()

public:
	ATN_ScoreShellBurst();

	/** Crea el estallido de una concha de tamaño Tier (TNScoreShells::ETier, 0-3) en Location. Null si no hace falta. */
	static ATN_ScoreShellBurst* SpawnAt(UWorld* World, const FVector& Location, uint8 Tier);

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void StartBurst(uint8 InTier);

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> BurstRoot;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> Flash;

	float Age = 0.f;
	float Life = 1.5f;
	float FlashLumens = 0.f;
};
