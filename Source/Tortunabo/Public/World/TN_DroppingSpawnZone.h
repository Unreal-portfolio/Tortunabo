#pragma once

#include "CoreMinimal.h"
#include "World/TN_SpawnZoneBase.h"
#include "TN_DroppingSpawnZone.generated.h"

class ATN_SeagullDroppingActor;

/**
 * Zona de spawn periódico de cacas de gaviota (#12 — sistema independiente).
 *
 * Cada SpawnInterval segundos, elige aleatoriamente un jugador vivo dentro del
 * volumen y spawnea una caca encima de él. Si MaxConcurrent está en uso, se omite
 * el ciclo hasta que alguna caca impacte.
 *
 * Solo activo en servidor. No se replica. El volumen, timer, selección de
 * jugadores y clamp los aporta ATN_SpawnZoneBase.
 *
 * Uso:
 *   Colocar BP_DroppingSpawnZone en el nivel.
 *   Asignar DroppingClass → BP_SeagullDropping.
 *   Ajustar SpawnInterval, InitialDelay, MaxConcurrent.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_DroppingSpawnZone : public ATN_SpawnZoneBase
{
	GENERATED_BODY()

public:
	ATN_DroppingSpawnZone();

protected:
	/** Clase de caca a spawnear. Apuntar al BP hijo de ATN_SeagullDroppingActor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DroppingSpawnZone")
	TSubclassOf<ATN_SeagullDroppingActor> DroppingClass;

	virtual void TrySpawn() override;
};
