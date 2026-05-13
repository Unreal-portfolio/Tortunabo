#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TN_LegAnimComponent.generated.h"

class UTN_StaminaComponent;

/**
 * @brief Componente genérico de animación pendular de patas para personajes ensamblados con primitivas.
 *
 * No lo usa ATortugaCharacter directamente (ese tiene la lógica de pendulum inline en Tick),
 * pero queda disponible como componente Blueprint opcional para otros actores.
 * Aplica una oscilación senoidal a dos grupos de huesos (GroupA, GroupB) desfasados 180°
 * con amplitud y frecuencia distintas según el jugador esté caminando o sprintando.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TORTUNABO_API UTN_LegAnimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTN_LegAnimComponent();

	/** @brief Cachea las rotaciones de descanso de cada hueso para poder restaurarlas. */
	virtual void BeginPlay() override;

	/** @brief Tick que actualiza el ángulo del péndulo y lo aplica a los grupos. */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Leg Animation|Walk", meta = (ClampMin = "0.0"))
	float WalkAmplitudeDegrees = 60.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Leg Animation|Walk", meta = (ClampMin = "0.1"))
	float WalkCyclesPerSecond = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Leg Animation|Sprint", meta = (ClampMin = "0.0"))
	float SprintAmplitudeDegrees = 90.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Leg Animation|Sprint", meta = (ClampMin = "0.1"))
	float SprintCyclesPerSecond = 3.5f;

	/** Grupo de huesos en fase 0 (ej. Pata1). */
	UPROPERTY(EditDefaultsOnly, Category = "Leg Animation|Limbs")
	TArray<FName> GroupA;

	/** Grupo de huesos en fase 180° (ej. Pata2). */
	UPROPERTY(EditDefaultsOnly, Category = "Leg Animation|Limbs")
	TArray<FName> GroupB;

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

	/** @brief Guarda en RestRotations la rotación de descanso de cada hueso listado en los grupos. */
	void CacheRestRotations();

	/** @brief Aplica un ángulo a todos los huesos de un grupo (offset respecto a su RestRotation). */
	void ApplyAngleToGroup(const TArray<FName>& Names, float AngleDeg);

	/** @brief Aplica un ángulo a un hueso individual por nombre. */
	void ApplyAngleToName(const FName& Name, float AngleDeg);

	/** @brief Busca un USceneComponent hijo por nombre en el owner. */
	USceneComponent* FindChildComponent(const FName& Name) const;
};
