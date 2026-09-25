#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TN_CarryComponent.generated.h"

class ATortugaCharacter;
class USoundBase;

/**
 * @brief Coger y lanzar a otra tortuga (fase 2 del issue #6).
 *
 * Reglas:
 *  - Solo se puede coger a una tortuga metida en su caparazón o aturdida (en ese
 *    caso se mete en el caparazón al cogerla). Vale cualquiera, también rivales.
 *  - La llevada no controla su movimiento. Si intenta moverse de forma continuada
 *    SecondsToEscape segundos, se libera. Mientras forcejea, al portador le tiembla
 *    la cámara y su lanzamiento pierde mucha fuerza.
 *  - Lanzamiento en parábola hacia donde apunta la cámara. En el aire la lanzada
 *    no puede salir del caparazón: al tocar suelo rebota en vertical, sale del
 *    caparazón durante ese rebote (se estira en el aire) y aterriza de pie.
 *
 * Red: estado server-authoritative. CarriedTurtle (en el portador) y CarriedBy (en
 * el llevado) replican y cada máquina aplica localmente el enganche. El impulso
 * del lanzamiento se aplica en el servidor y por Client RPC en el dueño del
 * lanzado, para que su predicción no pelee con la corrección.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class TORTUNABO_API UTN_CarryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTN_CarryComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ── Input (cliente dueño) ───────────────────────────────────────────────

	/** Busca una tortuga cogible delante y pide cogerla. true si había candidata. */
	bool TryGrabNearest();

	/** Lanza a la tortuga que lleva, hacia donde apunta la cámara. */
	void RequestThrow();

	/** Deja a la tortuga en el suelo sin lanzarla. */
	void RequestDrop();

	/** El llevado intenta moverse (forcejeo). Solo envía cambios de estado. */
	void SetStruggleInput(bool bStruggling);

	// ── Estado ──────────────────────────────────────────────────────────────

	UFUNCTION(BlueprintPure, Category = "Carry")
	bool IsCarrying() const { return CarriedTurtle != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Carry")
	bool IsBeingCarried() const { return CarriedBy != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Carry")
	bool IsCarriedStruggling() const { return bCarriedStruggling; }

	ATortugaCharacter* GetCarriedTurtle() const { return CarriedTurtle; }
	ATortugaCharacter* GetCarrier() const { return CarriedBy; }

	/** Lo llama el personaje al aterrizar (servidor y cliente dueño). */
	void NotifyLanded();

	/** Lo llama el personaje al entrar en el agua durante el vuelo. */
	void NotifyEnteredWater();

	bool IsAwaitingBounce() const { return bAwaitingBounce; }

	/** Servidor: suelta a quien lleve sin lanzarlo (muerte, derribo, escape). */
	void ForceRelease(bool bEscapeHop);

protected:
	/** Alcance para coger (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Carry", meta = (ClampMin = "50.0"))
	float GrabRange = 240.f;

	/** Velocidad del lanzamiento (cm/s). */
	UPROPERTY(EditDefaultsOnly, Category = "Carry", meta = (ClampMin = "100.0"))
	float ThrowSpeed = 1500.f;

	/** Multiplicador de fuerza si el llevado está forcejeando. */
	UPROPERTY(EditDefaultsOnly, Category = "Carry", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StruggleThrowMultiplier = 0.45f;

	/** Ángulo de lanzamiento mínimo/máximo (grados sobre la horizontal). */
	UPROPERTY(EditDefaultsOnly, Category = "Carry", meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float MinThrowPitch = 28.f;

	UPROPERTY(EditDefaultsOnly, Category = "Carry", meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float MaxThrowPitch = 72.f;

	/** Segundos de forcejeo continuo para liberarse. */
	UPROPERTY(EditDefaultsOnly, Category = "Carry", meta = (ClampMin = "0.1"))
	float SecondsToEscape = 2.f;

	/** Velocidad máxima del portador mientras lleva a alguien. */
	UPROPERTY(EditDefaultsOnly, Category = "Carry", meta = (ClampMin = "0.0"))
	float CarrySpeedCap = 330.f;

	/** Altura sobre el portador a la que va la llevada (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Carry")
	float CarryHeight = 120.f;

	/** Velocidad vertical del rebote al caer tras un lanzamiento. */
	UPROPERTY(EditDefaultsOnly, Category = "Carry", meta = (ClampMin = "0.0"))
	float BounceVelocity = 560.f;

	/** Intensidad del temblor de cámara del portador (grados). */
	UPROPERTY(EditDefaultsOnly, Category = "Carry", meta = (ClampMin = "0.0"))
	float StruggleShakeDegrees = 2.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Carry|Audio")
	TObjectPtr<USoundBase> GrabSound;

	UPROPERTY(EditDefaultsOnly, Category = "Carry|Audio")
	TObjectPtr<USoundBase> ThrowSound;

private:
	UPROPERTY(ReplicatedUsing = OnRep_CarriedTurtle)
	TObjectPtr<ATortugaCharacter> CarriedTurtle;

	UPROPERTY(ReplicatedUsing = OnRep_CarriedBy)
	TObjectPtr<ATortugaCharacter> CarriedBy;

	/** En el portador: su carga forcejea (temblor de cámara local). */
	UPROPERTY(Replicated)
	bool bCarriedStruggling = false;

	UFUNCTION(Server, Reliable)
	void ServerGrab(ATortugaCharacter* Target);

	UFUNCTION(Server, Reliable)
	void ServerThrow(FRotator AimRotation);

	UFUNCTION(Server, Reliable)
	void ServerDrop();

	UFUNCTION(Server, Unreliable)
	void ServerSetStruggling(bool bStruggling);

	/** En el dueño del lanzado: mismo impulso que en el servidor. */
	UFUNCTION(Client, Reliable)
	void ClientApplyThrow(FVector StartLocation, FVector Velocity, bool bBounce);

	UFUNCTION()
	void OnRep_CarriedTurtle();

	UFUNCTION()
	void OnRep_CarriedBy();

	ATortugaCharacter* GetTurtle() const;
	bool CanBeGrabbed(const ATortugaCharacter* Target) const;
	void ApplyCarrierLocalState(bool bCarrying);
	void ApplyCarriedLocalState(ATortugaCharacter* Carrier);
	void Release(ATortugaCharacter* Carried, const FVector& Location, const FVector& Velocity, bool bThrown);
	void RestoreCollisionWith(ATortugaCharacter* Other);

	/** Servidor, en el llevado: forcejeo actual y tiempo acumulado. */
	bool bStruggling = false;
	float StruggleTime = 0.f;
	bool bLocalStruggleSent = false;

	/** En el lanzado: rebote pendiente al tocar suelo (servidor y dueño). */
	bool bAwaitingBounce = false;

	/** Portador que acaba de soltarle (para dejar de ignorar su colisión). */
	TWeakObjectPtr<ATortugaCharacter> LastCarrier;
	bool bCarrierStateApplied = false;
	bool bCarriedStateApplied = false;
	float ShakeTime = 0.f;
};
