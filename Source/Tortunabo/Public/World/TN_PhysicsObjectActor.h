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
	float DormancyCheckInterval = 1.5f;

	/** Velocidad máxima (cm/s) que puede alcanzar el objeto tras un impacto.
	 *  Evita tunneling a través de paredes cuando el personaje empuja a alta velocidad. */
	UPROPERTY(EditDefaultsOnly, Category = "Physics|Network", meta = (ClampMin = "100.0"))
	float MaxPushVelocity = 1200.f;

	/** Velocidad (cm/s) aplicada cuando una tortuga empuja el objeto.
	 *  El CMC tiene bPushesRigidBodies=false → el impulso se aplica aquí, en OnMeshHit,
	 *  de forma controlada y segura para CCD. */
	UPROPERTY(EditDefaultsOnly, Category = "Physics|Network", meta = (ClampMin = "50.0"))
	float CharacterPushVelocity = 400.f;

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

	/**
	 * Detecta clipping contra geometría estática: si el objeto queda atrapado
	 * entre dos superficies (penetración en direcciones opuestas en ≥2 ejes),
	 * se destruye con un "poof" en lugar de quedarse vibrando dentro de paredes.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Physics|Crush")
	bool bEnableCrushDetection = true;

	/** Intervalo (s) entre chequeos de crush. 0 = solo al detenerse. */
	UPROPERTY(EditDefaultsOnly, Category = "Physics|Crush", meta = (ClampMin = "0.2"))
	float CrushCheckInterval = 2.0f;

	/** VFX opcional al destruirse por crush. */
	UPROPERTY(EditDefaultsOnly, Category = "Physics|Crush")
	TObjectPtr<class UNiagaraSystem> CrushPoofVFX;

	/** Sonido opcional al destruirse por crush. */
	UPROPERTY(EditDefaultsOnly, Category = "Physics|Crush")
	TObjectPtr<class USoundBase> CrushPoofSound;

private:
	UFUNCTION()
	void OnMeshHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
	               UPrimitiveComponent* OtherComp, FVector NormalImpulse,
	               const FHitResult& Hit);

	void TryEnterDormancy();
	void CheckForCrush();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastCrushPoof();

	FTimerHandle DormancyCheckTimer;
	FTimerHandle CrushCheckTimer;
};
