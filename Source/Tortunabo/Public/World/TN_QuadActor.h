#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_QuadActor.generated.h"

class UStaticMeshComponent;
class UCapsuleComponent;
class ATortugaCharacter;

/**
 * Quad gigante que cruza el mapa matando con sus ruedas.
 *
 * QuadMesh: puramente visual, sin colisión con pawns.
 * WheelLeft / WheelRight: UCapsuleComponent orientadas horizontalmente.
 *   Cualquier TortugaCharacter que las toque muere (RequestKill).
 *
 * Movimiento: línea recta de SpawnLocation a EndLocation a velocidad constante.
 * Al llegar al destino se autodestruye.
 * Spawneado periódicamente por ATN_QuadSpawner.
 *
 * Red: bReplicateMovement=true, bAlwaysRelevant=true.
 *   El servidor mueve el actor; los clientes reciben posición replicada.
 *   El kill se aplica solo en servidor (HasAuthority guard en overlap).
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_QuadActor : public AActor
{
	GENERATED_BODY()

public:
	ATN_QuadActor();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/**
	 * Inicializa el viaje del quad. Llamado por ATN_QuadSpawner tras spawnear.
	 * @param InEndLocation  Posición world donde el quad se autodestruye.
	 * @param InSpeed        Velocidad de viaje (cm/s).
	 */
	void InitializeTravel(const FVector& InEndLocation, float InSpeed);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quad")
	TObjectPtr<UStaticMeshComponent> QuadMesh;

	/** Rueda izquierda — kill collider horizontal. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quad")
	TObjectPtr<UCapsuleComponent> WheelLeft;

	/** Rueda derecha — kill collider horizontal. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quad")
	TObjectPtr<UCapsuleComponent> WheelRight;

	/** Velocidad por defecto (cm/s). Sobreescrita por InitializeTravel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quad",
		meta = (ClampMin = "100.0"))
	float TravelSpeed = 800.f;

	/**
	 * Offset lateral (Y local) de cada rueda desde el centro del quad (cm).
	 * Ajustar para que coincida con el mesh en el Blueprint hijo.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quad")
	float WheelLateralOffset = 280.f;

	/** Radio de la cápsula de rueda (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quad",
		meta = (ClampMin = "20.0"))
	float WheelCapsuleRadius = 90.f;

	/** Mitad de altura de la cápsula de rueda (cm). Controla el largo de la huella. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quad",
		meta = (ClampMin = "20.0"))
	float WheelCapsuleHalfHeight = 150.f;

private:
	FVector EndLocation;
	bool bTraveling = false;

	UFUNCTION()
	void OnWheelOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);
};

// ─────────────────────────────────────────────────────────────────────────────

/**
 * Spawner que crea ATN_QuadActor periódicamente.
 *
 * Coloca este actor en el nivel donde el quad debe aparecer.
 * Ajusta EndLocation (en world space) al otro extremo de la pista.
 * Solo activo en servidor.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_QuadSpawner : public AActor
{
	GENERATED_BODY()

public:
	ATN_QuadSpawner();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/** Clase del quad a spawnear. Asignar BP_QuadActor en el Editor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	TSubclassOf<ATN_QuadActor> QuadClass;

	/** Posición world donde el quad se autodestruye. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	FVector EndLocation = FVector(5000.f, 0.f, 0.f);

	/** Velocidad del quad (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner",
		meta = (ClampMin = "100.0"))
	float QuadSpeed = 800.f;

	/** Tiempo entre spawns (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner",
		meta = (ClampMin = "1.0"))
	float SpawnInterval = 20.f;

	/** Delay antes del primer spawn (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner",
		meta = (ClampMin = "0.0"))
	float InitialDelay = 3.f;

private:
	FTimerHandle SpawnTimerHandle;

	void SpawnQuad();
};
