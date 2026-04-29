#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_SeagullSpawnZone.generated.h"

class UBoxComponent;
class ATN_EnemySeagull;
class ATortugaCharacter;

/**
 * Zona de spawn de gaviotas enemigas dinámicas (#11).
 *
 * Uso:
 *   Colocar en el mundo y asignar SeagullClass (BP_EnemySeagull).
 *   El servidor elige aleatoriamente un jugador vivo dentro del volumen
 *   cada SpawnInterval segundos y spawnea una gaviota encima de él.
 *
 * La gaviota puede salir de esta zona siguiendo al jugador — la zona
 * solo controla quién puede ser elegido como objetivo inicial.
 * La lógica de escape/retirada vive en ATN_EnemySeagull.
 *
 * Solo activa si hay al menos un jugador vivo dentro del volumen.
 * Respeta MaxConcurrentSeagulls: no spawnea si ya hay esa cantidad
 * de gaviotas activas originadas por esta zona.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_SeagullSpawnZone : public AActor
{
	GENERATED_BODY()

public:
	ATN_SeagullSpawnZone();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/** Volumen que define el área de detección de jugadores para el spawn. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SeagullSpawnZone")
	TObjectPtr<UBoxComponent> SpawnVolume;

	/** Clase de gaviota a spawnear. Apuntar al BP hijo de ATN_EnemySeagull. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SeagullSpawnZone")
	TSubclassOf<ATN_EnemySeagull> SeagullClass;

	/** Intervalo de tiempo entre intentos de spawn (s). Editable por instancia. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SeagullSpawnZone", meta = (ClampMin = "1.0"))
	float SpawnInterval = 15.f;

	/** Retardo inicial antes del primer intento de spawn (s). Editable por instancia. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SeagullSpawnZone", meta = (ClampMin = "0.0"))
	float InitialDelay = 3.f;

	/** Número máximo de gaviotas activas originadas por esta zona simultáneamente. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SeagullSpawnZone", meta = (ClampMin = "1", ClampMax = "20"))
	int32 MaxConcurrentSeagulls = 3;

private:
	FTimerHandle SpawnTimerHandle;

	/** Gaviotas activas spawneadas por esta zona (punteros débiles — se limpian solos). */
	TArray<TWeakObjectPtr<ATN_EnemySeagull>> ActiveSeagulls;

	/** Intenta spawnear una gaviota sobre un jugador aleatorio dentro de la zona. */
	void TrySpawnSeagull();

	/** Devuelve jugadores vivos dentro del volumen de esta zona. */
	void GetPlayersInsideZone(TArray<ATortugaCharacter*>& OutPlayers) const;

	/** Elimina entradas caducadas de ActiveSeagulls. */
	void PruneActiveSeagulls();

	/** Clampea WorldLoc al AABB XY del SpawnVolume. Preserva Z. */
	FVector ClampXYToVolume(const FVector& WorldLoc) const;
};
