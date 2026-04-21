#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/TN_InventoryTypes.h"
#include "TN_ThrowableItemActor.generated.h"

class UProjectileMovementComponent;
class UStaticMeshComponent;
class ATortugaCharacter;
class ATN_PickupInteractableBase;

/**
 * Datos de lanzamiento agrupados en un único struct replicado.
 * Antes eran 3 propiedades separadas con el mismo OnRep, lo que causaba
 * 3 llamadas innecesarias al notificador. Con el struct se hace una sola.
 */
USTRUCT()
struct FTN_ThrowLaunchData
{
	GENERATED_BODY()

	UPROPERTY()
	FVector SpawnLocation = FVector::ZeroVector;

	UPROPERTY()
	FVector LaunchVelocity = FVector::ZeroVector;

	/**
	 * Escala del mesh del proyectil. Se toma de SourceItem.EquippedMeshScale en el servidor
	 * y se replica junto con los demás datos de lanzamiento para que todos los clientes
	 * vean la bola al mismo tamaño sin necesidad de una propiedad adicional en el actor.
	 */
	UPROPERTY()
	FVector MeshScale = FVector::OneVector;

	/**
	 * Mesh del proyectil. Se toma de SourceItem.EquippedMesh en el servidor y se replica
	 * dentro del struct para que los clientes que se unan tarde (JIP) vean el mesh correcto
	 * en lugar del placeholder gigante del actor por defecto.
	 */
	UPROPERTY()
	TObjectPtr<UStaticMesh> EquippedMesh = nullptr;

	/** true cuando los datos están listos para aplicarse en clientes. */
	UPROPERTY()
	bool bReady = false;
};

UCLASS()
class TORTUNABO_API ATN_ThrowableItemActor : public AActor
{
	GENERATED_BODY()

public:
	ATN_ThrowableItemActor();

	virtual void BeginPlay() override;
	virtual void LifeSpanExpired() override;

	UFUNCTION(BlueprintCallable, Category = "Throwable")
	void InitializeThrow(const FVector& SpawnLocation, const FVector& InitialVelocity);

	UFUNCTION(BlueprintCallable, Category = "Throwable")
	void SetSourceItem(const FTN_InventoryItem& Item);

protected:
	/** Root del actor. Al ser UStaticMeshComponent (UPrimitiveComponent), el
	 *  ProjectileMovement mueve el actor completo (no solo un hijo relativo). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Throwable")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Throwable")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable")
	float MaxLifeSeconds = 8.0f;


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Knockback", meta = (ClampMin = "0.1"))
	float KnockbackDuration = 2.0f;

	/**
	 * Velocidad mínima (cm/s) de la bola para noquear al jugador impactado.
	 * Si la bola va más lenta, rebota sin noquear.
	 * Default 600 cm/s — ajustar en BP.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Knockback", meta = (ClampMin = "0.0"))
	float MinKnockdownSpeed = 600.0f;

	/** Elasticidad al rebotar (0=sin rebote, 1=rebote perfecto). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Physics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Bounciness = 0.55f;

	/** Fricción al rodar por el suelo. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Physics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RollingFriction = 0.25f;

protected:
	/**
	 * Aplica los datos de lanzamiento (escala, posición, velocidad) cuando están listos.
	 * Virtual para que subclases puedan override y capturar OriginalScale tras la escala real.
	 * Solo debe llamarse una vez gracias a bLaunchApplied.
	 */
	virtual void ApplyLaunchDataIfReady();


private:
	// Datos del ítem — servidor únicamente, sin replicar
	FTN_InventoryItem SourceItem;

	/** Datos de lanzamiento: almacenados localmente tras el Multicast. Sin replicar. */
	FTN_ThrowLaunchData ThrowData;

	bool bLaunchApplied  = false;

	/** Players already knocked down by this throw — prevents duplicate knockdowns. */
	TSet<TWeakObjectPtr<ATortugaCharacter>> AlreadyHitPlayers;

	/**
	 * Distribuye los parámetros de lanzamiento a TODAS las máquinas (Reliable).
	 * Cada máquina simula el ProjectileMovement localmente desde las mismas
	 * condiciones iniciales → trayectoria completamente fluida sin updates de red.
	 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_InitializeThrow(FVector Origin, FVector Velocity,
	                               UStaticMesh* MeshAsset, FVector Scale);

	/**
	 * Notifica la posición final a todos los clientes antes de destruir el actor.
	 * Sincroniza visualmente las simulaciones locales al punto de parada del servidor.
	 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_BallStopped(FVector FinalLocation);

	/** Golpe contra jugador → knockdown + spawn pickup. Superficie → rebota, no destruir. */
	UFUNCTION()
	void OnMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	               UPrimitiveComponent* OtherComp, FVector NormalImpulse,
	               const FHitResult& Hit);

	/** Proyectil se detuvo completamente → spawn pickup en posición final. */
	UFUNCTION()
	void OnProjectileStopped(const FHitResult& ImpactResult);

protected:
	/** Spawna un pickup en la posición dada. Accesible para clases hijas. */
	void SpawnPickupAtLocation(const FVector& Location);

	/** Aplica IgnoreActorWhenMoving sobre el Instigator — solo relevante en servidor. */
	void IgnoreInstigatorCollision();

	bool bPickupSpawned  = false;
};
