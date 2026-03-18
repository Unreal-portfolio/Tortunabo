#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TN_LegAnimComponent.generated.h"

class UTN_StaminaComponent;

/** Generic child-component pendulum leg animation. Not used by TortugaCharacter directly
 *  (that one uses inline Tick logic), but available as an optional BP component. */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TORTUNABO_API UTN_LegAnimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTN_LegAnimComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Leg Animation|Walk", meta = (ClampMin = "0.0"))
	float WalkAmplitudeDegrees = 60.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Leg Animation|Walk", meta = (ClampMin = "0.1"))
	float WalkCyclesPerSecond = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Leg Animation|Sprint", meta = (ClampMin = "0.0"))
	float SprintAmplitudeDegrees = 90.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Leg Animation|Sprint", meta = (ClampMin = "0.1"))
	float SprintCyclesPerSecond = 3.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Leg Animation|Limbs")
	TArray<FName> GroupA;   // e.g. Pata1 — phase 0

	UPROPERTY(EditDefaultsOnly, Category = "Leg Animation|Limbs")
	TArray<FName> GroupB;   // e.g. Pata2 — phase 180

	UPROPERTY(EditDefaultsOnly, Category = "Leg Animation|Limbs")
	FVector SwingAxisLocal = FVector(0.f, 1.f, 0.f);

	UPROPERTY(EditDefaultsOnly, Category = "Leg Animation", meta = (ClampMin = "0.0"))
	float MinSpeedToAnimate = 20.f;

	UPROPERTY(EditDefaultsOnly, Category = "Leg Animation", meta = (ClampMin = "0.1"))
	float AmplitudeFadeSpeed = 8.f;

private:
	float PhaseAccumulator    = 0.f;
	float AmplitudeMultiplier = 0.f;

	TWeakObjectPtr<UTN_StaminaComponent> CachedStamina;
	TMap<FName, FRotator> RestRotations;

	void CacheRestRotations();
	void ApplyAngleToGroup(const TArray<FName>& Names, float AngleDeg);
	void ApplyAngleToName(const FName& Name, float AngleDeg);
	USceneComponent* FindChildComponent(const FName& Name) const;
};

