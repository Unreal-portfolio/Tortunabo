#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_DroppingSpawnZone.generated.h"

class UBoxComponent;
class ATN_SeagullDroppingActor;
class ATortugaCharacter;

/**
 * Zona de spawn periódico de cacas de gaviota (#12 — sistema independiente).
 *
 * Cada SpawnInterval segundos, elige aleatoriamente un jugador vivo dentro del
 * volumen y spawnea una caca encima de él (SpawnDroppingOnPlayer).
 * Si MaxConcurrentDrops está en uso, se omite el ciclo hasta que alguna caca impacte.
 *
 * Solo activo en servidor. No se replica.
 *
 * Uso:
 *   Colocar BP_DroppingSpawnZone en el nivel.
 *   Asignar DroppingClass → BP_SeagullDropping.
 *   Ajustar SpawnInterval, InitialDelay, MaxConcurrentDrops.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_DroppingSpawnZone : public AActor
{
	GENERATED_BODY()

public:
	ATN_DroppingSpawnZone();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/** Volumen que define el área de detección de jugadores. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DroppingSpawnZone")
	TObjectPtr<UBoxComponent> SpawnVolume;

	/** Clase de caca a spawnear. Apuntar al BP hijo de ATN_SeagullDroppingActor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DroppingSpawnZone")
	TSubclassOf<ATN_SeagullDroppingActor> DroppingClass;

	/** Intervalo entre intentos de spawn (s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DroppingSpawnZone",
		meta = (ClampMin = "1.0"))
	float SpawnInterval = 8.f;

	/** Retardo inicial antes del primer intento (s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DroppingSpawnZone",
		meta = (ClampMin = "0.0"))
	float InitialDelay = 3.f;

	/** Número máximo de cacas activas simultáneas de esta zona. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DroppingSpawnZone",
		meta = (ClampMin = "1", ClampMax = "8"))
	int32 MaxConcurrentDrops = 2;

private:
	FTimerHandle SpawnTimerHandle;

	/** Cacas activas spawneadas por esta zona. */
	TArray<TWeakObjectPtr<ATN_SeagullDroppingActor>> ActiveDroppings;

	void TrySpawnDropping();
	void GetPlayersInsideZone(TArray<ATortugaCharacter*>& OutPlayers) const;
	void PruneActiveDroppings();

	/** Clampea WorldLoc al AABB XY del SpawnVolume. Preserva Z. */
	FVector ClampXYToVolume(const FVector& WorldLoc) const;
};
