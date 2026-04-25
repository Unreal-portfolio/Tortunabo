#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/ITN_EnemyTargetInterface.h"
#include "TN_CrabActor.generated.h"

class USkeletalMeshComponent;
class USphereComponent;
class UAnimMontage;
class USoundBase;
class ATortugaCharacter;

UENUM(BlueprintType)
enum class ETNCrabState : uint8
{
	Patrol   UMETA(DisplayName = "Patrol"),
	Chase    UMETA(DisplayName = "Chase"),
	Attack   UMETA(DisplayName = "Attack"),
	Cooldown UMETA(DisplayName = "Cooldown"),
};

/**
 * Cangrejo enemigo: patrulla entre puntos, persigue al jugador más cercano
 * dentro del radio de detección y aplica knockdown al alcanzarlo.
 *
 * Servidor autoritario — tick desactivado en clientes.
 * CrabState replicado para animaciones. Las montages y sonidos se asignan
 * en el Blueprint hijo y se reproducen directamente en C++ sin lógica BP.
 * Compatible con chunks: PatrolPoints son offsets relativos al spawn.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_CrabActor : public AActor, public ITN_EnemyTargetInterface
{
	GENERATED_BODY()

public:
	ATN_CrabActor();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ── ITN_EnemyTargetInterface ────────────────────────────────────────────────
	virtual void ApplyStun(float Duration) override;
	virtual void ApplyBlind(float Duration) override;
	virtual bool IsStunned() const override { return StunRemaining > 0.f; }
	virtual bool IsBlinded() const override { return BlindRemaining > 0.f; }

protected:
	// ── Componentes ──────────────────────────────────────────────────────────────
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crab")
	TObjectPtr<USkeletalMeshComponent> CrabMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crab")
	TObjectPtr<USphereComponent> DetectionSphere;

	// ── Movimiento ───────────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crab|Movement")
	float PatrolSpeed = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crab|Movement")
	float ChaseSpeed = 450.f;

	// ── Detección ────────────────────────────────────────────────────────────────
	/** Radio de la esfera de detección para iniciar persecución. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crab|Detection",
		meta = (ClampMin = "100.0"))
	float DetectionRadius = 800.f;

	/**
	 * Distancia máxima desde el SpawnLocation antes de abandonar la persecución.
	 * Evita que el cangrejo se aleje indefinidamente del área de patrulla.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crab|Detection",
		meta = (ClampMin = "100.0"))
	float MaxChaseDistance = 2000.f;

	// ── Ataque ───────────────────────────────────────────────────────────────────
	/** Distancia a la que se aplica el knockdown. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crab|Attack",
		meta = (ClampMin = "50.0"))
	float AttackRadius = 120.f;

	/** Duración del knockdown (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crab|Attack")
	float KnockdownDuration = 2.5f;

	/** Magnitud del impulso al aplicar knockdown (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crab|Attack")
	float KnockdownImpulse = 600.f;

	/** Cooldown tras atacar antes de volver a patrullar (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crab|Attack")
	float CooldownDuration = 2.f;

	// ── Patrulla ─────────────────────────────────────────────────────────────────
	/**
	 * Puntos de patrulla como offsets relativos al SpawnLocation.
	 * Si está vacío el cangrejo permanece quieto.
	 * Usar offsets para compatibilidad con chunks spawneados en runtime.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crab|Patrol")
	TArray<FVector> PatrolPoints;

	// ── Animaciones (asignar en el BP hijo) ──────────────────────────────────────

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crab|Animation")
	TObjectPtr<UAnimMontage> PatrolMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crab|Animation")
	TObjectPtr<UAnimMontage> ChaseMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crab|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crab|Animation")
	TObjectPtr<UAnimMontage> CooldownMontage;

	// ── Audio (asignar en el BP hijo) ─────────────────────────────────────────────

	UPROPERTY(EditDefaultsOnly, Category = "Crab|Audio")
	TObjectPtr<USoundBase> PatrolSound;

	UPROPERTY(EditDefaultsOnly, Category = "Crab|Audio")
	TObjectPtr<USoundBase> ChaseSound;

	UPROPERTY(EditDefaultsOnly, Category = "Crab|Audio")
	TObjectPtr<USoundBase> AttackSound;

	UPROPERTY(EditDefaultsOnly, Category = "Crab|Audio")
	TObjectPtr<USoundBase> CooldownSound;

	// ── Estado replicado ─────────────────────────────────────────────────────────
	UPROPERTY(ReplicatedUsing = OnRep_CrabState, BlueprintReadOnly, Category = "Crab")
	ETNCrabState CrabState = ETNCrabState::Patrol;

	/** Tiempo restante de stun (s). Replicado para VFX cliente. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Crab|Stun")
	float StunRemaining = 0.f;

	/** Tiempo restante de ceguera (s). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Crab|Stun")
	float BlindRemaining = 0.f;

	/** VFX/sonido al ser aturdido. Asignar en BP. */
	UPROPERTY(EditDefaultsOnly, Category = "Crab|Stun")
	TObjectPtr<class UNiagaraSystem> StunVFX;

	UPROPERTY(EditDefaultsOnly, Category = "Crab|Stun")
	TObjectPtr<class USoundBase> StunSound;

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayStunEffect(float Duration);

private:
	FVector SpawnLocation;
	TArray<FVector> WorldPatrolPoints;
	int32 CurrentPatrolIndex = 0;

	TWeakObjectPtr<ATortugaCharacter> ChaseTarget;
	float CooldownRemaining = 0.f;

	// Deferred init para compatibilidad con chunks
	FTimerHandle InitTimerHandle;
	void InitializePatrolPoints();

	void SetCrabState(ETNCrabState NewState);
	void TickPatrol(float DeltaTime);
	void TickChase(float DeltaTime);
	void TickAttack();
	void TickCooldown(float DeltaTime);
	void MoveTowards(const FVector& Target, float Speed, float DeltaTime);
	int32 FindNearestPatrolIndex() const;
	bool IsAliveAndValid(ATortugaCharacter* Char) const;

	UFUNCTION()
	void OnRep_CrabState();

	UFUNCTION()
	void OnDetectionBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);
};
