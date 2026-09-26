#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Player/MP_GamePlayerController.h"
#include "World/ProcMap/TN_ProcMapEnums.h"
#include "TN_TerrainViewGameMode.generated.h"

class ATN_ProcMapGenerator;

/**
 * Controlador del nivel de solo terreno: la tortuga de siempre, sin ninguna interfaz. Consola: TNRegen [semilla]
 * vuelve a generar el mapa (0 = semilla aleatoria) y deja a todos en la salida.
 */
UCLASS()
class TORTUNABO_API ATN_TerrainViewPlayerController : public AMP_GamePlayerController
{
	GENERATED_BODY()

public:
	ATN_TerrainViewPlayerController();

	UFUNCTION(Exec)
	void TNRegen(int32 Seed = 0);

private:
	UFUNCTION(Server, Reliable)
	void ServerRegen(int32 Seed);
};

/**
 * Modo de LVL_ProcMap_Terrain: genera el mapa procedural en modo terreno (ATN_ProcMapGenerator::bTerrainOnly: el
 * terreno y lo integrado en el camino, sin vegetación, fauna ni decoración) y deja a las tortugas en la salida, sin
 * HUD, tormenta, rondas ni meta. Sirve para comparar generadores en igualdad de condiciones.
 */
UCLASS()
class TORTUNABO_API ATN_TerrainViewGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ATN_TerrainViewGameMode();

	virtual void BeginPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	/** Genera otra vez (Seed 0 = aleatoria) y lleva a todos a la salida cuando esté listo. */
	void Regenerate(int32 Seed);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "ProcMap")
	ETNProcGameMode Mode = ETNProcGameMode::Coop;

	UPROPERTY(EditDefaultsOnly, Category = "ProcMap")
	ETNProcDifficulty Difficulty = ETNProcDifficulty::Normal;

	/** Semilla fija (0 = aleatoria en cada partida). */
	UPROPERTY(EditDefaultsOnly, Category = "ProcMap")
	int32 FixedSeed = 0;

private:
	UPROPERTY(Transient)
	TObjectPtr<ATN_ProcMapGenerator> Generator;

	TArray<TWeakObjectPtr<APlayerController>> Waiting;
	FTimerHandle PollTimer;

	ATN_ProcMapGenerator* EnsureGenerator();
	void PollReady();
};
