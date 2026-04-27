#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_GoalZone.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UWidgetComponent;
class USoundBase;
class UNiagaraSystem;
class ATN_PhysicsObjectActor;
class ATN_GoalZone;

/**
 * Server-side delegate disparado cuando una GoalZone alcanza RequiredGoals.
 * Mismo patrón que CollectionZone: el GameMode puede iterar TActorIterator y
 * suscribirse en BeginPlay para reaccionar (bonus al RaceScore, abrir ruta…).
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGoalZoneAllReached, ATN_GoalZone*);

/**
 * Portería para puzzles físicos.
 *
 * Flujo:
 *   1. La bola física (ATN_PhysicsObjectActor / ATN_BouncePhysicsObject) entra
 *      en el GoalBox → ++GoalCount, se teletransporta al BallSpawnTarget con
 *      velocidad reseteada y dormancy flusheada.
 *   2. El UWidgetComponent encima muestra el contador (binding desde UMG al
 *      accessor BlueprintPure GetGoalCount / GetRequiredGoals).
 *   3. Al alcanzar RequiredGoals: bAllGoalsReached=true (replicado), se aplican
 *      offsets a TargetActorsOnGoalReached (puerta, plataforma, lo que sea),
 *      multicast VFX/audio, y delegate broadcast al GameMode.
 *
 * Multiplayer:
 *   - GoalCount + bAllGoalsReached replicados — todos los clientes ven el contador
 *     vivo y la condición de victoria.
 *   - Respawn server-auth puro (SetActorLocation + SetPhysicsLinearVelocity 0).
 *   - VFX/audio vía Multicast Reliable — cosmético-pero-crítico.
 *
 * Setup en editor:
 *   1. BP_GoalZone hijo de esta clase.
 *   2. Asignar BaseMesh (visual de portería).
 *   3. Asignar BallSpawnTarget (un Target Point en el mapa).
 *   4. Asignar AcceptedBallClass (BP_BouncePhysicsObject) — opcional, NULL acepta
 *      cualquier ATN_PhysicsObjectActor.
 *   5. Asignar WidgetClass al ScoreWidget3D (UMG con TextBlock bindeado a GetGoalCount).
 *   6. Para puzzle: poblar TargetActorsOnGoalReached + offsets.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_GoalZone : public AActor
{
	GENERATED_BODY()

public:
	ATN_GoalZone();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server-only delegate broadcast al alcanzar RequiredGoals. */
	FOnGoalZoneAllReached OnZoneGoalReached;

	UFUNCTION(BlueprintPure, Category = "GoalZone")
	int32 GetGoalCount() const { return GoalCount; }

	UFUNCTION(BlueprintPure, Category = "GoalZone")
	int32 GetRequiredGoals() const { return RequiredGoals; }

	UFUNCTION(BlueprintPure, Category = "GoalZone")
	bool IsAllGoalsReached() const { return bAllGoalsReached; }

	/** Bonus opcional sumado al RaceScore al completar la zona. El handler vive
	 *  en el GameMode (mismo patrón que CollectionZone — no se inyecta aquí). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoalZone")
	int32 GoalReachedBonusScore = 100;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoalZone")
	TObjectPtr<UStaticMeshComponent> BaseMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoalZone")
	TObjectPtr<UBoxComponent> GoalBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoalZone")
	TObjectPtr<UWidgetComponent> ScoreWidget3D;

	/** Clase de bola aceptada. NULL = cualquier ATN_PhysicsObjectActor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoalZone")
	TSubclassOf<ATN_PhysicsObjectActor> AcceptedBallClass;

	/** Actor cuyo Transform define el punto de respawn de la bola tras gol.
	 *  Recomendado: AActor TargetPoint colocado en el mapa. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoalZone")
	TObjectPtr<AActor> BallSpawnTarget;

	/** Goles necesarios para activar OnAllGoalsReached y aplicar TargetActors. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoalZone",
		meta = (ClampMin = "1"))
	int32 RequiredGoals = 3;

	/** Cooldown (s) tras un gol antes de aceptar el siguiente — evita doble
	 *  conteo si la bola toca el GoalBox 2 veces seguidas durante el respawn. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoalZone",
		meta = (ClampMin = "0.0"))
	float ReentryCooldown = 0.4f;

	/** Si true, sigue aceptando goles después de RequiredGoals (counter sin tope). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoalZone")
	bool bContinueScoringAfterGoal = false;

	/** Actores que se moverán al alcanzar RequiredGoals (puerta sube, plataforma
	 *  aparece, etc). Mismo flujo que CollectionZone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoalZone|Activation")
	TArray<TObjectPtr<AActor>> TargetActorsOnGoalReached;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoalZone|Activation")
	FVector ActivationLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoalZone|Activation")
	FRotator ActivationRotationOffset = FRotator::ZeroRotator;

	/** Sonido al marcar un gol — spawned en cada máquina vía Multicast. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoalZone|FX")
	TObjectPtr<USoundBase> GoalScoredSound;

	/** VFX al marcar un gol — spawned en cada máquina vía Multicast. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoalZone|FX")
	TObjectPtr<UNiagaraSystem> GoalScoredVFX;

	/** Sonido al alcanzar RequiredGoals (puzzle resuelto). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoalZone|FX")
	TObjectPtr<USoundBase> AllGoalsReachedSound;

	/** VFX al alcanzar RequiredGoals (puzzle resuelto). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoalZone|FX")
	TObjectPtr<UNiagaraSystem> AllGoalsReachedVFX;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_GoalCount, Category = "GoalZone")
	int32 GoalCount = 0;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AllGoalsReached, Category = "GoalZone")
	bool bAllGoalsReached = false;

private:
	UFUNCTION()
	void OnRep_GoalCount();

	UFUNCTION()
	void OnRep_AllGoalsReached();

	UFUNCTION()
	void OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	void RespawnBall(ATN_PhysicsObjectActor* Ball);
	void ApplyTargetTransforms();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastOnGoalScored();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastOnAllGoalsReached();

	float LastGoalServerTime = -1000.f;
};
