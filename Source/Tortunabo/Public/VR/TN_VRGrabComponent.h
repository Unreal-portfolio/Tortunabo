#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "TN_VRGrabComponent.generated.h"

class UPhysicsHandleComponent;
class UPrimitiveComponent;

/**
 * Coger objetos con física con las aletas en VR (Docs/Modo_VR.md): el agarre de cada mando coge el objeto con física más
 * cercano a la punta de la aleta, lo lleva pegado a la mano (UPhysicsHandleComponent: sigue chocando con lo demás) y, al
 * soltar, sale con la velocidad de la mano.
 *
 * Red: si el actor se replica, lo mueve el servidor (el dueño manda la mano unas 30 veces por segundo) y los demás lo
 * ven con su réplica de siempre; si no se replica (decorado con física local), se coge solo en la propia máquina.
 * Nunca coge tortugas, enemigos ni caparazones (esos tienen sus propias reglas) ni nada de más de MaxMass kg.
 *
 * Lo usa ATN_VRRig; vive en la tortuga para que sus RPC vayan por la conexión de su dueño.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class TORTUNABO_API UTN_VRGrabComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTN_VRGrabComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Dueño: coge lo más cercano a la mano Hand (0 izquierda, 1 derecha). true si ha cogido algo. */
	bool TryGrab(int32 Hand, const FTransform& HandWorld);

	/** Dueño, cada fotograma mientras se mantiene el agarre. */
	void UpdateGrab(int32 Hand, const FTransform& HandWorld);

	/** Dueño: suelta con la velocidad de la mano. */
	void Release(int32 Hand, const FVector& HandVelocity);

	bool IsGrabbing(int32 Hand) const;

	/** ¿Se puede coger con la mano? Con física, móvil, sin dueño pawn ni caparazón y de hasta MaxMass kg. */
	static bool IsGrabbable(const UPrimitiveComponent* Component, const AActor* ByActor, float MaxMassKg);

	/** Radio (cm) alrededor de la punta de la aleta en el que se busca qué coger. */
	UPROPERTY(EditAnywhere, Category = "VR|Grab", meta = (ClampMin = "5.0"))
	float GrabRadius = 22.f;

	/** Masa máxima (kg) de lo que se puede coger. */
	UPROPERTY(EditAnywhere, Category = "VR|Grab", meta = (ClampMin = "1.0"))
	float MaxMass = 250.f;

	/** Distancia máxima (cm) entre la tortuga y lo que coge, para aceptarlo en el servidor. */
	UPROPERTY(EditAnywhere, Category = "VR|Grab", meta = (ClampMin = "50.0"))
	float MaxServerReach = 350.f;

private:
	UFUNCTION(Server, Reliable)
	void ServerGrab(uint8 Hand, UPrimitiveComponent* Target, FVector_NetQuantize10 HandLocation, FRotator HandRotation);

	UFUNCTION(Server, Unreliable)
	void ServerMoveGrab(uint8 Hand, FVector_NetQuantize10 HandLocation, FRotator HandRotation);

	UFUNCTION(Server, Reliable)
	void ServerRelease(uint8 Hand, FVector_NetQuantize10 Velocity);

	UPrimitiveComponent* FindGrabbable(const FVector& At) const;
	/** Coge en esta máquina (servidor, o local si el actor no se replica). */
	bool GrabHere(int32 Hand, UPrimitiveComponent* Target, const FTransform& HandWorld);
	void MoveHere(int32 Hand, const FTransform& HandWorld);
	void ReleaseHere(int32 Hand, const FVector& Velocity);
	UPhysicsHandleComponent* GetHandle(int32 Hand);

	UPROPERTY(Transient)
	TObjectPtr<UPhysicsHandleComponent> LeftHandle;

	UPROPERTY(Transient)
	TObjectPtr<UPhysicsHandleComponent> RightHandle;

	/** Lo cogido por cada mano, dónde respecto a la mano y si lo mueve el servidor. */
	TWeakObjectPtr<UPrimitiveComponent> Held[2];
	FTransform HeldFromHand[2];
	bool bHeldByServer[2] = { false, false };
	double LastMoveSent[2] = { -1.0, -1.0 };
};
