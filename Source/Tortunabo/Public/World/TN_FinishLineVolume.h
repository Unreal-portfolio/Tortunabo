#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_FinishLineVolume.generated.h"

class UBoxComponent;
class ATN_RunGameMode;

/**
 * Línea de meta.
 *
 * Refactorizado de ATriggerBox a AActor + UBoxComponent para poder
 * ser colocado dentro de Blueprint Actors (chunks spawneados en runtime).
 * ATriggerBox hereda de ABrush y NO puede vivir dentro de un BP hijo.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_FinishLineVolume : public AActor
{
	GENERATED_BODY()

public:
	ATN_FinishLineVolume();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Caja de colisión — editar su tamaño en el Viewport del Blueprint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FinishLine")
	TObjectPtr<UBoxComponent> TriggerBox;

private:
	UFUNCTION()
	void OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	ATN_RunGameMode* ResolveRunGameMode() const;
};
