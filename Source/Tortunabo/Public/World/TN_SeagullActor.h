#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_SeagullActor.generated.h"

class UCapsuleComponent;
class UStaticMeshComponent;

/**
 * @deprecated Usar ATN_EnemySeagull + ATN_SeagullSpawnZone en su lugar (#11).
 *
 * Gaviota de carril estática — ya no se usa en el proyecto.
 * Conservada temporalmente para no romper assets existentes. No crear nuevas instancias.
 *
 * El sistema activo es ATN_EnemySeagull (gaviota dinámica que sigue al jugador,
 * detecta cubierta, y pica en picado físico) spawneada por ATN_SeagullSpawnZone.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_SeagullActor : public AActor
{
	GENERATED_BODY()

public:
	ATN_SeagullActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	// ── Componentes ──────────────────────────────────────────────────────────

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Seagull")
	TObjectPtr<USceneComponent> Root;

	/** Cuerpo visual del actor (cilindro o cualquier mesh). Sin colisión. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Seagull")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** Zona de peligro: trigger de overlap para detectar jugadores. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Seagull")
	TObjectPtr<UCapsuleComponent> DangerZone;

	/** El cubo "gaviota" que baja al picar. Empieza en SeagullRestHeight. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Seagull")
	TObjectPtr<UStaticMeshComponent> SeagullMesh;

	// ── Propiedades configurables ─────────────────────────────────────────────

	/** Rango total de movimiento a ambos lados del origen (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seagull|Movement")
	float MoveRange = 500.f;

	/** Velocidad de desplazamiento lateral (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seagull|Movement")
	float MoveSpeed = 200.f;

	/** Altura de reposo del cubo sobre el root (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seagull|Strike")
	float SeagullRestHeight = 800.f;

	/** Tiempo para bajar completamente al objetivo (s). Cuanto menor, más rápido. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seagull|Strike",
		meta = (ClampMin = "0.01"))
	float StrikeDuration = 0.12f;

	/** Tiempo para volver a subir (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seagull|Strike",
		meta = (ClampMin = "0.01"))
	float RiseDuration = 0.5f;

	/** Segundos que un jugador debe estar en la zona antes de que la gaviota pique. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seagull|Strike",
		meta = (ClampMin = "0.1"))
	float DangerTimeToStrike = 1.f;

	/** Radio de la cápsula de peligro (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seagull|Zone")
	float DangerCapsuleRadius = 150.f;

	/** Semialtura de la cápsula de peligro (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seagull|Zone")
	float DangerCapsuleHalfHeight = 100.f;

private:
	// ── Movimiento lateral (local en todas las máquinas) ──────────────────────

	/** Posición mundial de inicio: se sincroniza del host a todos los clientes. */
	UPROPERTY(ReplicatedUsing = OnRep_InitialLocation)
	FVector InitialLocation = FVector::ZeroVector;

	UFUNCTION()
	void OnRep_InitialLocation();

	/** Offset actual desde InitialLocation a lo largo del eje X local. */
	float CurrentOffset  = 0.f;
	float MoveDirection  = 1.f; // +1 o -1
	bool  bPositionSynced = false;

	// Posición inicial replicada vía DOREPLIFETIME + OnRep_InitialLocation.
	// No se necesita Multicast adicional — el OnRep cubre clientes conectados y JIP.

	// ── Strike ────────────────────────────────────────────────────────────────

	/** Countdown por jugador (segundos) antes de picar. Solo en servidor. */
	TMap<TWeakObjectPtr<APlayerController>, float> PendingStrikeRemaining;

	FTimerHandle StrikeTickHandle;

	void TickStrikeCountdowns();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastDoStrike(FVector TargetWorldLocation);

	// Estado de animación del cubo (local en cada máquina)
	bool  bIsStriking     = false;
	float StrikeAlpha     = 0.f;  // 0 = arriba, 1 = abajo
	float StrikeTargetZ   = 0.f;  // Z mundial objetivo (local, sin replicar)
	bool  bStrikeGoingDown = true;

	// ── Overlap ───────────────────────────────────────────────────────────────

	UFUNCTION()
	void OnDangerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnDangerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** Captura posición inicial (diferida un tick para ChildActorComponent). */
	void DeferredCaptureInitialLocation();
};
