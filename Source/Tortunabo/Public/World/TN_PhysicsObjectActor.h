#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_PhysicsObjectActor.generated.h"

class UStaticMeshComponent;

/**
 * Actor físico replicado con gestión automática de dormancia.
 *
 * Diseño:
 *   - Servidor simula la física (autoridad). Clientes reciben la posición
 *     replicada vía bReplicateMovement y NO simulan localmente.
 *   - En reposo: DORM_DormantAll — 0 bytes enviados.
 *   - Al recibir un golpe: FlushNetDormancy despierta el actor y empieza
 *     a replicar hasta que la velocidad baja del umbral → vuelve a dormir.
 *
 * Uso:
 *   Crear un BP hijo, asignar el StaticMesh y el material.
 *   No es necesario tocar la lógica en Blueprint.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_PhysicsObjectActor : public AActor
{
	GENERATED_BODY()

public:
	ATN_PhysicsObjectActor();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Physics")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Velocidad (UU/s) por debajo de la cual el actor vuelve a dormancy. */
	UPROPERTY(EditDefaultsOnly, Category = "Physics|Network", meta = (ClampMin = "1.0"))
	float SleepVelocityThreshold = 15.f;

	/** Intervalo (s) entre comprobaciones de velocidad tras un impacto. */
	UPROPERTY(EditDefaultsOnly, Category = "Physics|Network", meta = (ClampMin = "0.1"))
	float DormancyCheckInterval = 0.5f;

	/** Bloquear rotación alrededor del eje X (roll). Evita que el actor ruede de lado. */
	UPROPERTY(EditDefaultsOnly, Category = "Physics|Constraints")
	bool bLockRotationX = true;

	/** Bloquear rotación alrededor del eje Y (pitch). Evita que el actor vuelque hacia delante/atrás. */
	UPROPERTY(EditDefaultsOnly, Category = "Physics|Constraints")
	bool bLockRotationY = true;

	/**
	 * Bloquear rotación alrededor del eje Z (yaw). Evita que el actor spinee
	 * verticalmente cuando el jugador lo empuja oblicuo. Default: true → el
	 * cubo de empuje se comporta como caja slide-only.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Physics|Constraints")
	bool bLockRotationZ = true;

private:
	UFUNCTION()
	void OnMeshHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
	               UPrimitiveComponent* OtherComp, FVector NormalImpulse,
	               const FHitResult& Hit);

	void TryEnterDormancy();

	FTimerHandle DormancyCheckTimer;
};
