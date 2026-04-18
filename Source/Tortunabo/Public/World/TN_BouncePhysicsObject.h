#pragma once

#include "CoreMinimal.h"
#include "World/TN_PhysicsObjectActor.h"
#include "TN_BouncePhysicsObject.generated.h"

class ATortugaCharacter;

/**
 * PhysicsObject hijo: rota libremente (bola) y "botea" al jugador al golpearlo.
 * Al impactar con una ATortugaCharacter aplica LaunchCharacter con componente Z
 * para lanzarla hacia arriba y un empuje horizontal escalable.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_BouncePhysicsObject : public ATN_PhysicsObjectActor
{
	GENERATED_BODY()

public:
	ATN_BouncePhysicsObject();

protected:
	virtual void BeginPlay() override;

	/** Velocidad Z (cm/s) aplicada al jugador al impactar. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Bounce",
		meta = (ClampMin = "0.0"))
	float PlayerBounceImpulseZ = 700.f;

	/** Empuje horizontal adicional (cm/s) en la dirección bola→jugador. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Bounce",
		meta = (ClampMin = "0.0"))
	float PlayerBounceHorizontal = 250.f;

	/**
	 * Cooldown mínimo (s) entre botes al mismo jugador — evita que el
	 * delegate OnComponentHit dispare varios impactos en un solo contacto.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Bounce",
		meta = (ClampMin = "0.0"))
	float PerPlayerCooldown = 0.25f;

private:
	UFUNCTION()
	void OnBounceHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
	                 UPrimitiveComponent* OtherComp, FVector NormalImpulse,
	                 const FHitResult& Hit);

	TMap<TWeakObjectPtr<ATortugaCharacter>, float> LastBounceTimeByPlayer;
};
