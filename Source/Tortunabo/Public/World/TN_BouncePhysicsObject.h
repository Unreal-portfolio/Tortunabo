#pragma once

#include "CoreMinimal.h"
#include "World/TN_PhysicsObjectActor.h"
#include "TN_BouncePhysicsObject.generated.h"

class ATortugaCharacter;

/**
 * PhysicsObject hijo tipo balón: rota libremente y recibe un impulso al ser
 * golpeado por un ATortugaCharacter, como un balón de fútbol que se patea.
 * A diferencia del padre, permite las tres rotaciones.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_BouncePhysicsObject : public ATN_PhysicsObjectActor
{
	GENERATED_BODY()

public:
	ATN_BouncePhysicsObject();

protected:
	virtual void BeginPlay() override;

	/** Velocidad Z (cm/s) que adquiere la bola al ser golpeada por un jugador. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Kick",
		meta = (ClampMin = "0.0"))
	float KickVerticalBoost = 350.f;

	/** Empuje horizontal (cm/s) en la dirección jugador → bola. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Kick",
		meta = (ClampMin = "0.0"))
	float KickHorizontalBoost = 600.f;

	/**
	 * Cooldown mínimo (s) entre kicks del mismo jugador — evita que el delegate
	 * OnComponentHit dispare varios impulsos en un solo contacto continuo.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Kick",
		meta = (ClampMin = "0.0"))
	float PerPlayerCooldown = 0.25f;

private:
	UFUNCTION()
	void OnKickHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
	               UPrimitiveComponent* OtherComp, FVector NormalImpulse,
	               const FHitResult& Hit);

	TMap<TWeakObjectPtr<ATortugaCharacter>, float> LastKickTimeByPlayer;
};
