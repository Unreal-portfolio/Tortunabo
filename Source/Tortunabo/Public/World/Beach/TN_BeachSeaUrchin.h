#pragma once

#include "CoreMinimal.h"
#include "World/Beach/TN_BeachEnemy.h"
#include "World/ProcMap/TN_ProcMapAmbientFX.h"
#include "TN_BeachSeaUrchin.generated.h"

class UStaticMeshComponent;

/**
 * Erizo de mar de la playa (ETNBeachElement::SeaUrchin): una bola de púas de unos 3 m (un erizo de 11 cm a escala),
 * grande y lento, que rueda girando sobre sí mismo hacia la tortuga más cercana que entre en su radio, sin salirse de
 * su zona. Tocarlo pincha: la tortuga cae derribada con ragdoll y mareo (TNBeach::KnockDownTurtle), despedida hacia
 * fuera dando una vuelta (en carrera no se muere), y el erizo retrocede un poco rodando. Si no nota a nadie, pasea casi
 * sin parar por su zona (respiros cortos), rodeando lo grande del reparto y apartándose de los demás enemigos. Si le da
 * algo lanzado se marea (ApplyHitStun): se tambalea en el sitio con pajaritos, sin rodar ni pinchar.
 */
UCLASS()
class TORTUNABO_API ATN_BeachSeaUrchin : public ATN_BeachEnemy
{
	GENERATED_BODY()

public:
	ATN_BeachSeaUrchin();

protected:
	virtual void ApplySpec() override;
	virtual void ServerTick(float DeltaSeconds) override;
	virtual void VisualTick(float DeltaSeconds) override;
	virtual float GetBodyRadius() const override;
	virtual float GetActiveRange() const override { return LeashRadius + DetectRadius + 1500.f; }

	/** Todas las máquinas: ha pinchado a Victim en Where. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPrick(ATortugaCharacter* Victim, FVector_NetQuantize Where);

private:
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Ball;

	float SizeK = 1.f;
	/** Altura del centro al rodar (cm) y distancia de contacto con una tortuga. */
	float RollRadius = 125.f;
	float HitDistance = 170.f;
	float DetectRadius = 2000.f;
	float LeashRadius = 1600.f;

	// Servidor.
	TWeakObjectPtr<ATortugaCharacter> Target;
	FVector WanderGoal = FVector::ZeroVector;
	float StateLeft = 0.f;
	float GroundTimer = 0.f;
	float GroundZ = 0.f;

	// Visual.
	FQuat Spin = FQuat::Identity;
	FVector LastShown = FVector::ZeroVector;
	float Clock = 0.f;
	float RollSoundTimer = 0.f;
	float Speed = 0.f;
	TNAmbientFX::FEmitter Dust;

	/** Servidor: rueda hacia Goal sin salirse de la correa. */
	void RollToward(const FVector& Goal, float MoveSpeed, float DeltaSeconds);
	/** Servidor: pincha a quien toque (en cualquier estado). true si ha pinchado a alguien. */
	bool CheckPricks();
};
