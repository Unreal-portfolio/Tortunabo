#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PhysicsVolume.h"
#include "TN_ProcWaterActors.generated.h"

class UBoxComponent;
class USphereComponent;
class UStaticMeshComponent;
class UStaticMesh;
class USoundBase;
class ATortugaCharacter;

/**
 * Agua nadable del mapa procedural. Un único PhysicsVolume con muchas cajas (las
 * zonas de agua profunda cubiertas por rectángulos). bPhysicsOnContact: el motor
 * consulta un punto en el centro de la cápsula, así que se nada cuando el centro
 * está bajo la superficie y se vadea en aguas someras.
 * LOCAL en todas las máquinas: el modo Swimming lo predicen servidor y cliente.
 */
UCLASS()
class TORTUNABO_API ATN_ProcWaterVolume : public APhysicsVolume
{
	GENERATED_BODY()

public:
	ATN_ProcWaterVolume(const FObjectInitializer& ObjectInitializer);

	/** Añade una caja de agua (centro y semiextensión en mundo). */
	void AddWaterBox(const FVector& Center, const FVector& Extent);

	int32 NumBoxes() const { return Boxes.Num(); }

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBoxComponent>> Boxes;
};

/** Corriente que arrastra a quien nada dentro. LOCAL en todas las máquinas. */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_ProcWaterCurrent : public AActor
{
	GENERATED_BODY()

public:
	ATN_ProcWaterCurrent();

	virtual void Tick(float DeltaTime) override;

	void Setup(const FVector& Extent, const FVector& InDirection, float InStrength);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Current")
	TObjectPtr<UBoxComponent> Box;

	/** Marcas flotantes que indican la dirección (greybox). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Current")
	TArray<TObjectPtr<UStaticMeshComponent>> Markers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Current")
	FVector Direction = FVector(1.f, 0.f, 0.f);

	/** Aceleración de arrastre (cm/s²). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Current", meta = (ClampMin = "0.0"))
	float Strength = 900.f;

private:
	float MarkerPhase = 0.f;
};

/** Remolino: atrae y hace girar a quien nada cerca; en el centro mata. LOCAL. */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_ProcWhirlpool : public AActor
{
	GENERATED_BODY()

public:
	ATN_ProcWhirlpool();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Whirlpool")
	TObjectPtr<USphereComponent> Area;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Whirlpool")
	TObjectPtr<UStaticMeshComponent> Disc;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Whirlpool", meta = (ClampMin = "100.0"))
	float Radius = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Whirlpool", meta = (ClampMin = "0.0"))
	float PullAcceleration = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Whirlpool", meta = (ClampMin = "0.0"))
	float SwirlAcceleration = 700.f;

	/** Segundos en el ojo del remolino antes de ahogarse. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Whirlpool", meta = (ClampMin = "0.1"))
	float SecondsInEyeToDie = 2.5f;

private:
	TMap<TWeakObjectPtr<ATortugaCharacter>, float> EyeTime;
};

/**
 * Tiburón/morena: patrulla su zona de agua y persigue a quien nade cerca.
 * Si alcanza a un nadador, lo mata. Replicado: lo mueve el servidor.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_ProcWaterPredator : public AActor
{
	GENERATED_BODY()

public:
	ATN_ProcWaterPredator();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Predator")
	TObjectPtr<USceneComponent> Root;

	/** Aleta visible sobre el agua: el aviso. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Predator")
	TObjectPtr<UStaticMeshComponent> Fin;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Predator")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Predator", meta = (ClampMin = "0.0"))
	float PatrolRadius = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Predator", meta = (ClampMin = "0.0"))
	float PatrolSpeed = 260.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Predator", meta = (ClampMin = "0.0"))
	float DetectRadius = 2000.f;

	/** Un poco más rápido que nadar: salirse del camino se paga. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Predator", meta = (ClampMin = "0.0"))
	float ChaseSpeed = 700.f;

	/** No se aleja más de esto de su casa. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Predator", meta = (ClampMin = "0.0"))
	float LeashRadius = 3200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Predator", meta = (ClampMin = "0.0"))
	float BiteRadius = 170.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Predator")
	TObjectPtr<USoundBase> BiteSound;

private:
	FVector Home = FVector::ZeroVector;
	float PatrolAngle = 0.f;
	TWeakObjectPtr<ATortugaCharacter> Target;
	float BiteCooldown = 0.f;

	ATortugaCharacter* FindSwimmerNear(const FVector& Where, float Range) const;
};

/**
 * Criatura flotante con comportamiento de medusa: pisarla por arriba hace rebotar;
 * tocarla nadando empuja y aturde un momento. Cada bioma pone su variante (medusa,
 * nenúfar gigante, boya, piedra pómez...). Replicada: el rebote lo decide el servidor.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_ProcWaterBouncer : public AActor
{
	GENERATED_BODY()

public:
	ATN_ProcWaterBouncer();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Servidor, antes de replicar: aspecto de la variante del bioma. */
	void SetVariant(UStaticMesh* Mesh, const FLinearColor& Color);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bouncer")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bouncer")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bouncer")
	TObjectPtr<USphereComponent> Contact;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bouncer", meta = (ClampMin = "0.0"))
	float BounceVelocity = 1150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bouncer", meta = (ClampMin = "0.0"))
	float StingPush = 750.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bouncer")
	TObjectPtr<USoundBase> BounceSound;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Variant)
	TObjectPtr<UStaticMesh> VariantMesh;

	UPROPERTY(ReplicatedUsing = OnRep_Variant)
	FLinearColor VariantColor = FLinearColor(0.9f, 0.5f, 0.9f);

	UFUNCTION()
	void OnRep_Variant();

	UFUNCTION()
	void OnContact(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastBounceFeedback();

	TMap<TWeakObjectPtr<AActor>, double> LastContact;
	float BobTime = 0.f;
	float SquishAlpha = 0.f;
};
