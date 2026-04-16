#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_EnemySeagull.generated.h"

class UStaticMeshComponent;
class UDecalComponent;
class ATortugaCharacter;
class APlayerController;
class ATN_RunGameMode;

/**
 * Gaviota dinámica enemiga (#11).
 *
 * Comportamiento:
 *   1. Aparece de repente encima del jugador objetivo (sin telegrafía prolongada).
 *   2. Sigue al jugador a FollowSpeed (< velocidad de paseo) — el jugador puede escapar corriendo.
 *   3. Un círculo en el suelo (Decal) muestra la zona de peligro y SE ENCOGE con el tiempo.
 *   4. Cuando el cronómetro llega a 0: si el jugador está dentro de MinKillRadius → muerte.
 *   5. Si el jugador lleva Big Head activo (#2): el impacto quita el efecto en lugar de matar.
 *
 * Autoridad: servidor controla movimiento, cronómetro y detección de muerte.
 *   CountdownRemaining se replica → los clientes pueden mostrar HUD/efectos.
 *
 * Uso:
 *   1. Crear BP_EnemySeagull hijo de este actor.
 *   2. Asignar SeagullMesh y DecalMaterial en el BP.
 *   3. Llamar SpawnOnRandomPlayer() desde el RunGameMode o TimedEvent en el nivel.
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
	 * Inicializa la gaviota con un objetivo. Llamar desde el servidor inmediatamente tras spawnear.
	 * Equivalente al "activar" del sistema.
	 */
	UFUNCTION(BlueprintCallable, Category = "EnemySeagull")
	void InitializeWithTarget(ATortugaCharacter* Target);

	/**
	 * Helper de spawn: selecciona un jugador vivo aleatorio y spawnea esta clase encima de él.
	 * Devuelve null si no hay jugadores vivos o si WorldContext no es válido.
	 *
	 * Ejemplo de uso en BP Level Blueprint:
	 *   ATN_EnemySeagull::SpawnOnRandomPlayer(this, BP_EnemySeagull::StaticClass());
	 */
	UFUNCTION(BlueprintCallable, Category = "EnemySeagull",
		meta = (WorldContext = "WorldContextObject"))
	static ATN_EnemySeagull* SpawnOnRandomPlayer(const UObject* WorldContextObject,
		TSubclassOf<ATN_EnemySeagull> SeagullClass);

	/** Cronómetro restante replicado (segundos). Usar en UI/VFX para feedback al jugador. */
	UFUNCTION(BlueprintPure, Category = "EnemySeagull")
	float GetCountdownRemaining() const { return CountdownRemaining; }

	/** Radio de peligro actual (normalizado con el tiempo). Usar para escalar efectos BP. */
	UFUNCTION(BlueprintPure, Category = "EnemySeagull")
	float GetCurrentDangerRadius() const;

protected:
	// ── Componentes ──────────────────────────────────────────────────────────

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "EnemySeagull")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Mesh visual de la gaviota. Asignar en el BP hijo. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "EnemySeagull")
	TObjectPtr<UStaticMeshComponent> SeagullMesh;

	/**
	 * Decal proyectado hacia abajo que representa el círculo de peligro.
	 * Asignar un material de decal (anillo/círculo rojo) en el BP hijo.
	 * Su tamaño se actualiza cada tick según GetCurrentDangerRadius().
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "EnemySeagull")
	TObjectPtr<UDecalComponent> DangerDecal;

	// ── Configuración ─────────────────────────────────────────────────────────

	/**
	 * Velocidad de seguimiento (cm/s). Debe ser menor que la velocidad de paseo
	 * del jugador (WalkSpeed = 450) para que pueda escapar corriendo.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EnemySeagull|Follow",
		meta = (ClampMin = "50.0"))
	float FollowSpeed = 280.f;

	/** Altura de vuelo sobre el objetivo (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EnemySeagull|Follow",
		meta = (ClampMin = "100.0"))
	float FollowHeight = 400.f;

	/** Duración total del cronómetro de ataque (segundos). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EnemySeagull|Attack",
		meta = (ClampMin = "1.0"))
	float AttackTimerSeconds = 8.f;

	/** Radio inicial del círculo de peligro (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EnemySeagull|Attack",
		meta = (ClampMin = "50.0"))
	float MaxDangerRadius = 500.f;

	/**
	 * Radio mínimo de muerte cuando el cronómetro llega a 0 (cm).
	 * El jugador debe estar más lejos que este valor para sobrevivir.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EnemySeagull|Attack",
		meta = (ClampMin = "10.0"))
	float MinKillRadius = 150.f;

	/**
	 * Profundidad del Decal (cm). Ajustar si el decal no se proyecta bien en superficies irregulares.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EnemySeagull|Visual",
		meta = (ClampMin = "50.0"))
	float DecalDepth = 800.f;

	// ── Eventos Blueprint ─────────────────────────────────────────────────────

	/** Llamado en TODAS las máquinas cuando la gaviota mata al objetivo. */
	UFUNCTION(BlueprintImplementableEvent, Category = "EnemySeagull")
	void OnSeagullKill(ATortugaCharacter* Victim);

	/** Llamado en TODAS las máquinas cuando el objetivo escapa (cronómetro = 0, fuera del radio). */
	UFUNCTION(BlueprintImplementableEvent, Category = "EnemySeagull")
	void OnSeagullMissed(ATortugaCharacter* Escapee);

	/** Llamado en TODAS las máquinas cuando el Big Head absorbe el golpe en lugar de la muerte. */
	UFUNCTION(BlueprintImplementableEvent, Category = "EnemySeagull")
	void OnBigHeadAbsorbed(ATortugaCharacter* Target);

private:
	// ── Estado replicado ──────────────────────────────────────────────────────

	UPROPERTY(ReplicatedUsing = OnRep_CountdownRemaining)
	float CountdownRemaining = 0.f;

	UFUNCTION()
	void OnRep_CountdownRemaining();

	/** Índice en PlayerArray del objetivo — se replica para que los clientes puedan
	 *  referenciar el mismo personaje sin puntero directo. */
	UPROPERTY(ReplicatedUsing = OnRep_TargetPlayerIndex)
	int32 TargetPlayerIndex = -1;

	UFUNCTION()
	void OnRep_TargetPlayerIndex();

	// ── Estado solo servidor ──────────────────────────────────────────────────

	TWeakObjectPtr<ATortugaCharacter>  TargetCharacter;
	TWeakObjectPtr<APlayerController>  TargetController;

	bool bAttackResolved = false;

	// ── Lógica interna ────────────────────────────────────────────────────────

	void TickFollowTarget(float DeltaTime);
	void TickCountdown(float DeltaTime);
	void ResolveAttack();
	void UpdateDecalSize();

	/** Envía el resultado (kill/miss/bighead) a todos los clientes para feedback visual. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastResolveResult(bool bKilled, bool bBigHeadAbsorbed, ATortugaCharacter* VictimOrEscapee);
};
