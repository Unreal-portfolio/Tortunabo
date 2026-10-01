// Proyectiles de la torreta del buggy y sus efectos (charco de alga, explosión, ráfagas cosméticas). Replicados: el
// servidor decide los impactos y los clientes ven el actor.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Rally/TN_RallyVehicle.h"
#include "TN_RallyProjectile.generated.h"

class ATN_Buggy;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/** Proyectil de una munición: coco, alga, burbuja, mortero o tinta (Docs/Rally_MVP.md). */
UCLASS()
class TORTUNABO_API ATN_RallyProjectile : public AActor
{
	GENERATED_BODY()

public:
	ATN_RallyProjectile();

	/** Antes de FinishSpawning: munición, velocidad inicial y buggy que dispara (no se impacta a sí mismo al salir). */
	void Init(ETNRallyAmmo InAmmo, const FVector& Velocity, ATN_Buggy* FiredBy);

	ETNRallyAmmo GetAmmo() const { return Ammo; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void OnRep_Ammo();
	void ApplyLook();

	UFUNCTION()
	void OnSphereHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void OnSphereOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

	/** Solo servidor: aplica el efecto de la munición en Where, sobre HitBuggy si lo hay, y destruye el proyectil (salvo la burbuja contra el suelo). */
	void Impact(ATN_Buggy* HitBuggy, const FVector& Where);
	void StopOwnerIgnore();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USphereComponent> Sphere;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UProjectileMovementComponent> Movement;

	UPROPERTY(ReplicatedUsing = OnRep_Ammo)
	ETNRallyAmmo Ammo = ETNRallyAmmo::Coco;

	UPROPERTY(Transient)
	TWeakObjectPtr<ATN_Buggy> Shooter;

	bool bImpacted = false;
	FTimerHandle OwnerIgnoreTimer;
};

/** Charco de alga: 6 m durante 5 s; agarre ×0,5 y velocidad máxima ×0,6 a cualquier buggy dentro (servidor). */
UCLASS()
class TORTUNABO_API ATN_RallyAlgaPuddle : public AActor
{
	GENERATED_BODY()

public:
	ATN_RallyAlgaPuddle();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Buggies cuyo escudo ya ha anulado este charco. */
	TSet<TWeakObjectPtr<ATN_Buggy>> Immune;
	float CheckAccumulator = 0.f;
};

UENUM()
enum class ETNRallyBurstKind : uint8
{
	CocoHit,
	Explosion,
	Ink,
	BubblePop,
	Shield
};

/** Ráfaga cosmética corta (esfera que crece y se desvanece), replicada al aparecer. */
UCLASS()
class TORTUNABO_API ATN_RallyBurstFX : public AActor
{
	GENERATED_BODY()

public:
	ATN_RallyBurstFX();

	/** Solo servidor: crea la ráfaga en Where con el radio final RadiusCm. */
	static ATN_RallyBurstFX* Spawn(UWorld* World, ETNRallyBurstKind Kind, const FVector& Where, float RadiusCm);

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnRep_Look();
	void ApplyLook();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(ReplicatedUsing = OnRep_Look)
	ETNRallyBurstKind Kind = ETNRallyBurstKind::CocoHit;

	UPROPERTY(ReplicatedUsing = OnRep_Look)
	float RadiusCm = 100.f;

	float Age = 0.f;
};
