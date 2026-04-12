#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_JellyfishActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class USceneComponent;
class USoundBase;
class UNiagaraSystem;
class ATortugaCharacter;

/**
 * Medusa saltarina: esfera con patas que hace rebotar a los jugadores al pisarla.
 *
 * Mecánica de red:
 *  - Detección de overlap: solo en el servidor (BounceZone).
 *  - Impulso: LaunchCharacter con bZOverride=true → el Z siempre es MaxBounceVelocity,
 *    nunca acumula altura por rebotar varias veces seguidas.
 *  - Sonido + VFX: NetMulticast Reliable → todos los clientes lo ven/oyen.
 *  - Animación squish (aplanar/rebotar la esfera): local en cada máquina,
 *    disparada por el mismo Multicast.
 *
 * Uso: colocar BP_JellyfishActor en un nivel o chunk. Ajustar HeadMesh y asignar
 *      el BounceSound / BounceVFX en los Class Defaults del BP.
 *
 * Requisitos del Blueprint:
 *  - Asignar una esfera como HeadMesh.
 *  - Opcionalmente añadir patas como componentes hijo del HeadMesh en el BP.
 *  - BounceZone es un box trigger fino posicionado en la parte superior de la esfera.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_JellyfishActor : public AActor
{
	GENERATED_BODY()

public:
	ATN_JellyfishActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	// ── Componentes ─────────────────────────────────────────────────────────────

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Jellyfish")
	TObjectPtr<USceneComponent> Root;

	/** Mesh de la cabeza (esfera). Sin colisión propia — el BounceZone maneja el trigger. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Jellyfish")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	/**
	 * Trigger fino en la parte superior de la esfera.
	 * Solo se activa cuando el jugador llega desde arriba (velocidad Z negativa).
	 * Editar su tamaño en el BP para ajustar el área de rebote.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Jellyfish")
	TObjectPtr<UBoxComponent> BounceZone;

	// ── Configuración de rebote ──────────────────────────────────────────────────

	/**
	 * Velocidad Z máxima (cm/s) aplicada al jugador al rebotar.
	 * Con bZOverride=true en LaunchCharacter, esta velocidad SUSTITUYE la Z actual
	 * → sin acumulación de altura por rebotar repetidamente.
	 * Aproximación: 1200 cm/s ≈ 3 m de altura de rebote con gravedad estándar.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jellyfish|Bounce",
		meta = (ClampMin = "200.0", ClampMax = "3000.0"))
	float MaxBounceVelocity = 1200.f;

	/**
	 * Cooldown en segundos entre rebotes para el mismo jugador.
	 * Evita que un jugador parado encima de la medusa reciba impulsos continuos.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jellyfish|Bounce",
		meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float BounceCooldown = 0.5f;

	// ── Feedback audiovisual ─────────────────────────────────────────────────────

	/** Sonido reproducido en el punto de rebote del jugador (en todos los clientes). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jellyfish|Feedback")
	TObjectPtr<USoundBase> BounceSound;

	/** Sistema Niagara (VFX) spawneado en el punto de rebote del jugador. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jellyfish|Feedback")
	TObjectPtr<UNiagaraSystem> BounceVFX;

	// ── Animación squish ─────────────────────────────────────────────────────────

	/**
	 * Duración (s) de la fase de aplastamiento (HeadMesh se aplana).
	 * Cuanto menor, más snappy el feedback.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jellyfish|Squish",
		meta = (ClampMin = "0.02", ClampMax = "0.5"))
	float SquishInDuration = 0.08f;

	/**
	 * Duración (s) de la fase de recuperación (HeadMesh vuelve a su forma normal).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jellyfish|Squish",
		meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float SquishOutDuration = 0.3f;

	/**
	 * Escala Z de la esfera en el punto máximo de aplastamiento.
	 * 0.6 = comprime la esfera al 60% de su altura → da sensación de gelatina.
	 * X e Y se expanden proporcionalmente para mantener el volumen visual.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jellyfish|Squish",
		meta = (ClampMin = "0.1", ClampMax = "0.95"))
	float SquishZScale = 0.6f;

private:
	// ── Overlap ──────────────────────────────────────────────────────────────────

	UFUNCTION()
	void OnBounceZoneBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	/** Aplica el impulso al jugador (solo servidor). */
	void ApplyBounce(ATortugaCharacter* TurtleChar);

	// ── Replicación de posición inicial (mismo patrón que SeagullActor) ───────────

	UPROPERTY(ReplicatedUsing = OnRep_InitialLocation)
	FVector InitialLocation = FVector::ZeroVector;

	UFUNCTION()
	void OnRep_InitialLocation();

	// Posición inicial replicada vía DOREPLIFETIME + OnRep_InitialLocation.
	// No se necesita Multicast adicional — el OnRep cubre clientes conectados y JIP.

	bool bPositionSynced = false;

	void DeferredCaptureInitialLocation();

	// ── Multicast: efectos visuales + sonido + squish ────────────────────────────

	/**
	 * Llamado desde el servidor cuando un jugador rebota.
	 * EffectLocation = posición de los pies del jugador en el momento del rebote.
	 */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayBounceEffects(FVector EffectLocation);

	// ── Animación squish (local en cada máquina) ──────────────────────────────────

	/** Escala original del HeadMesh (guardada en BeginPlay para restaurar correctamente). */
	FVector HeadMeshDefaultScale = FVector::OneVector;

	/** 0 = forma normal, 1 = máximo aplastamiento. */
	float SquishAlpha = 0.f;

	/** true = yendo hacia el aplastamiento. */
	bool bSquishingIn = false;

	/** true = volviendo a la forma normal. */
	bool bSquishingOut = false;

	/** Aplica la escala del HeadMesh según SquishAlpha. */
	void ApplySquishScale() const;

	// ── Cooldown por jugador ──────────────────────────────────────────────────────

	/** Tiempo restante de cooldown indexado por jugador (solo servidor). */
	TMap<TWeakObjectPtr<ATortugaCharacter>, float> PlayerCooldowns;
};
