#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/TN_FinishLineVolume.h"
#include "TN_ProcTraversalActors.generated.h"

class UBoxComponent;
class UCapsuleComponent;
class UStaticMeshComponent;
class UNiagaraComponent;
class USoundBase;
class ACharacter;

/**
 * Géiser: lanza a quien lo pisa hasta un punto de aterrizaje (cima de torre
 * colosal o borde del escalón). Un sentido: no hay forma de volver a bajar sin
 * caerse. Actor LOCAL en todas las máquinas: el servidor y el cliente dueño
 * aplican el mismo impulso balístico y la predicción de movimiento cuadra.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_ProcGeyser : public AActor
{
	GENERATED_BODY()

public:
	ATN_ProcGeyser();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Punto de aterrizaje en mundo (suelo). */
	void SetTarget(const FVector& InTarget) { Target = InTarget; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Geyser")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Geyser")
	TObjectPtr<UCapsuleComponent> Trigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Geyser")
	TObjectPtr<UStaticMeshComponent> BaseMesh;

	/** Columna de agua (greybox): se estira y encoge en bucle. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Geyser")
	TObjectPtr<UStaticMeshComponent> ColumnMesh;

	/** VFX opcional (asignar un sistema Niagara en el BP hijo). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Geyser")
	TObjectPtr<UNiagaraComponent> SprayVFX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geyser")
	TObjectPtr<USoundBase> LaunchSound;

	/** Altura extra de la parábola sobre el punto más alto (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geyser", meta = (ClampMin = "0.0"))
	float ApexExtra = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geyser")
	FVector Target = FVector::ZeroVector;

private:
	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void Launch(ACharacter* Character);

	TMap<TWeakObjectPtr<ACharacter>, double> LastLaunchTime;
	float PulseTime = 0.f;
};

/**
 * Tobogán-cascada: la bajada es terreno con pendiente no caminable; este actor
 * añade el empuje ladera abajo y marca al personaje como inmune a la caída hasta
 * que aterriza. LOCAL en todas las máquinas (mismo motivo que el géiser).
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_ProcSlideZone : public AActor
{
	GENERATED_BODY()

public:
	ATN_ProcSlideZone();

	virtual void Tick(float DeltaTime) override;

	/** Crea los volúmenes a lo largo de la bajada (puntos en mundo, de arriba abajo). */
	void InitFromPoints(const TArray<FVector>& Points, float Width);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slide")
	TObjectPtr<USceneComponent> Root;

	/** Aceleración extra ladera abajo (cm/s²). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide", meta = (ClampMin = "0.0"))
	float BoostAcceleration = 700.f;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBoxComponent>> Segments;

	TArray<FVector> SegmentDirs;

	UFUNCTION()
	void OnSegmentOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};

/**
 * Volumen que mata al entrar (fondo de zanjas, lava). LOCAL en todas las
 * máquinas; solo el servidor ejecuta la muerte.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_ProcKillVolume : public AActor
{
	GENERATED_BODY()

public:
	ATN_ProcKillVolume();

	virtual void BeginPlay() override;

	void SetExtent(const FVector& Extent);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kill")
	TObjectPtr<UBoxComponent> Box;

private:
	UFUNCTION()
	void OnBoxOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};

/** Meta del mapa procedural: la línea de meta de siempre con tamaño ajustable. */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_ProcFinishVolume : public ATN_FinishLineVolume
{
	GENERATED_BODY()

public:
	void SetExtent(const FVector& Extent);
};
