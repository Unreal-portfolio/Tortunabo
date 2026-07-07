#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_EnemySeagull.generated.h"

class UStaticMeshComponent;
class UDecalComponent;
class USoundBase;
class UNiagaraSystem;
class ATortugaCharacter;
class APlayerController;
class APlayerState;

/**
 * Gaviota dinámica enemiga (#11) — versión unificada.
 *
 * Ciclo de vida:
 *   1. ATN_SeagullSpawnZone spawnea esta gaviota encima del jugador objetivo
 *      que está dentro de la zona, y llama InitializeWithTarget.
 *   2. La gaviota sigue al jugador a FollowSpeed, proyectando un decal (círculo)
 *      que se encoge con el tiempo.
 *   3. ESCAPE: si el jugador permanece fuera del círculo EscapeSeconds seguidos
 *      → la gaviota sube y desaparece (AbortAndRetreat).
 *   4. CUBIERTA: cada RoofCheckInterval el servidor lanza un LineTrace desde la
 *      gaviota hacia el jugador. Si hay geometría entre ambos → AbortAndRetreat.
 *   5. ATAQUE: cuando el cronómetro llega a 0 y el jugador está dentro de
 *      MinKillRadius → picotazo físico (dive hacia abajo, aplica kill, sube y se destruye).
 *      Sombrilla (#29) y BigHead (#2) abortan el kill según sus reglas habituales.
 *
 * Red:
 *   Servidor controla todo (movimiento, escape, kill).
 *   bReplicateMovement=true → el dive es visible en todos los clientes.
 *   CountdownRemaining y TargetPlayerState replicados para feedback visual de clientes.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_EnemySeagull : public AActor
{
	GENERATED_BODY()

public:
	ATN_EnemySeagull();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Inicializa la gaviota con un objetivo. Llamar desde ATN_SeagullSpawnZone tras spawnear.
	 * Solo servidor.
	 */
	UFUNCTION(BlueprintCallable, Category = "EnemySeagull")
	void InitializeWithTarget(ATortugaCharacter* Target);

	/** Cronómetro restante (s). Replicado para HUD/VFX. */
	UFUNCTION(BlueprintPure, Category = "EnemySeagull")
	float GetCountdownRemaining() const { return CountdownRemaining; }

	/** Radio de peligro actual interpolado con el tiempo. */
	UFUNCTION(BlueprintPure, Category = "EnemySeagull")
	float GetCurrentDangerRadius() const;

protected:
	// ── Componentes ────────────────────────────────────────────────────────────

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "EnemySeagull")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "EnemySeagull")
	TObjectPtr<UStaticMeshComponent> SeagullMesh;

	/** Decal proyectado hacia abajo. Radio se actualiza cada tick. Asignar material de círculo en el BP. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "EnemySeagull")
	TObjectPtr<UDecalComponent> DangerDecal;

	// ── Seguimiento ────────────────────────────────────────────────────────────

	/** Velocidad de seguimiento (cm/s). Debe ser menor que WalkSpeed (450) para que el jugador pueda escapar. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EnemySeagull|Follow", meta = (ClampMin = "50.0"))
	float FollowSpeed = 350.f;

	/** Altura de vuelo sobre el objetivo (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EnemySeagull|Follow", meta = (ClampMin = "100.0"))
	float FollowHeight = 400.f;

	/**
	 * Amplitud (cm) del bobbing natural mientras la gaviota sigue al objetivo.
	 * 0 = movimiento estático rígido. Valores altos = más caricaturesco.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EnemySeagull|Follow", meta = (ClampMin = "0.0"))
	float IdleBobAmplitude = 25.f;

	/** Frecuencia (Hz) del bob principal en Z. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EnemySeagull|Follow", meta = (ClampMin = "0.1"))
	float IdleBobFrequency = 1.4f;

	// ── Ataque ─────────────────────────────────────────────────────────────────

	/** Duración total del cronómetro antes del ataque (s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EnemySeagull|Attack", meta = (ClampMin = "1.0"))
	float AttackTimerSeconds = 8.f;

	/** Radio inicial del círculo de peligro (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EnemySeagull|Attack", meta = (ClampMin = "50.0"))
	float MaxDangerRadius = 500.f;

	/** Radio mínimo al expirar el cronómetro. El jugador debe estar fuera de este radio para sobrevivir. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EnemySeagull|Attack", meta = (ClampMin = "10.0"))
	float MinKillRadius = 150.f;

	// ── Escape ─────────────────────────────────────────────────────────────────

	/** Segundos fuera del círculo de peligro para que la gaviota se retire. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EnemySeagull|Escape", meta = (ClampMin = "0.5"))
	float EscapeSeconds = 3.f;

	// ── Cubierta ───────────────────────────────────────────────────────────────

	/** Intervalo entre comprobaciones de techo (s). Menor = más reactivo pero más caro. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EnemySeagull|Cover", meta = (ClampMin = "0.05"))
	float RoofCheckInterval = 0.25f;

	// ── Picotazo físico ────────────────────────────────────────────────────────

	/** Duración del dive hacia abajo (s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EnemySeagull|Strike", meta = (ClampMin = "0.05"))
	float StrikeDuration = 0.15f;

	/** Duración de la subida tras el picotazo (s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EnemySeagull|Strike", meta = (ClampMin = "0.1"))
	float RiseDuration = 0.5f;

	/** Altura adicional sobre el spawn point a la que sube al retirarse (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EnemySeagull|Strike", meta = (ClampMin = "100.0"))
	float RetreatHeight = 800.f;

	// ── Visual ─────────────────────────────────────────────────────────────────

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EnemySeagull|Visual", meta = (ClampMin = "50.0"))
	float DecalDepth = 800.f;

	// ── Audio / VFX ────────────────────────────────────────────────────────────

	/** Sonido al hacer el picotazo. Reproducido en el punto del objetivo en todas las máquinas. */
	UPROPERTY(EditDefaultsOnly, Category = "EnemySeagull|Audio")
	TObjectPtr<USoundBase> StrikeSound;

	/** Sonido al retirarse (escape o cubierta detectada). */
	UPROPERTY(EditDefaultsOnly, Category = "EnemySeagull|Audio")
	TObjectPtr<USoundBase> RetreatSound;

	/** Sonido al matar al jugador. */
	UPROPERTY(EditDefaultsOnly, Category = "EnemySeagull|Audio")
	TObjectPtr<USoundBase> KillSound;

	/** Sonido al absorber el BigHead en lugar de matar. */
	UPROPERTY(EditDefaultsOnly, Category = "EnemySeagull|Audio")
	TObjectPtr<USoundBase> BigHeadAbsorbSound;

	/** VFX spawneado en el punto del objetivo al hacer el picotazo. */
	UPROPERTY(EditDefaultsOnly, Category = "EnemySeagull|VFX")
	TObjectPtr<UNiagaraSystem> StrikeVFX;

private:
	// ── Estado replicado ───────────────────────────────────────────────────────

	UPROPERTY(ReplicatedUsing = OnRep_CountdownRemaining)
	float CountdownRemaining = 0.f;

	UFUNCTION()
	void OnRep_CountdownRemaining();

	/**
	 * PlayerState del objetivo — replicado para que clientes resuelvan la referencia.
	 * Puntero (NetGUID) en lugar de índice de PlayerArray: el array se reordena con
	 * disconnects/JIP y un índice replicado apuntaría al jugador equivocado.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_TargetPlayerState)
	TObjectPtr<APlayerState> TargetPlayerState;

	UFUNCTION()
	void OnRep_TargetPlayerState();

	// ── Estado solo servidor ──────────────────────────────────────────────────

	TWeakObjectPtr<ATortugaCharacter> TargetCharacter;
	TWeakObjectPtr<APlayerController> TargetController;

	float TimeOutsideShadow = 0.f;
	float NextRoofCheckTime = 0.f;
	bool  bAttackResolved   = false;

	// ── Strike state machine ───────────────────────────────────────────────────

	bool  bIsStriking       = false;
	bool  bIsRetreating     = false;
	bool  bStrikeGoingDown  = true;
	float StrikeAlpha       = 0.f;
	float StrikeStartZ      = 0.f;
	float StrikeTargetZ     = 0.f;
	float RetreatStartZ     = 0.f;
	float RetreatEndZ       = 0.f;

	// ── Lógica ────────────────────────────────────────────────────────────────

	void TickFollowTarget(float DeltaTime);
	void TickCountdown(float DeltaTime);
	void TickEscapeCheck(float DeltaTime);
	void TickRoofCheck(float DeltaTime);
	void TickStrike(float DeltaTime);
	void TickRetreat(float DeltaTime);

	void ResolveAttack();
	void AbortAndRetreat();
	bool HasRoofBetweenSeagullAndTarget() const;

	void UpdateDecalSize();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayStrikeEffects(FVector TargetLocation);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayRetreatEffect();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayKillEffect(ATortugaCharacter* Victim);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayBigHeadAbsorbEffect(ATortugaCharacter* Target);
};
