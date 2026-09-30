#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TN_ScorePickupWakeSubsystem.generated.h"

class ATN_ScorePickup;

/**
 * Cuándo está despierta una concha de puntos (lógica pura, tests Tortunabo.Perf.ScorePickupWake): cerca de la cámara
 * local, a menos de su WakeDistance; para dormirse hay que alejarse un poco más (SleepFactor), así no parpadea en el borde.
 */
namespace TNScorePickupWake
{
	/** Cada cuánto se miran las distancias de todas (s). */
	inline constexpr float CheckInterval = 0.25f;

	/** Se duerme a WakeDistance × SleepFactor. */
	inline constexpr float SleepFactor = 1.1f;

	inline bool ShouldBeAwake(double DistSquared, float WakeDistance, bool bWasAwake)
	{
		const double Limit = static_cast<double>(WakeDistance) * (bWasAwake ? SleepFactor : 1.f);
		return DistSquared < Limit * Limit;
	}
}

/**
 * @brief Despierta y duerme las conchas de puntos según la distancia a la cámara local, todas de una vez.
 *
 * Hay cientos en el mapa (253 en la playa, Docs/Estres-Monkey-2026-09-29.md coste 7): cada una tenía su Tick (y el de su
 * WidgetComponent) encendido aunque estuviera lejos, a medio ritmo, solo para mirar la distancia. Ahora este subsistema
 * mira todas las distancias cada CheckInterval y las lejanas tienen el Tick apagado del todo
 * (ATN_ScorePickup::SetAwake). Solo en las máquinas con pantalla: en el servidor dedicado las conchas no se apuntan.
 */
UCLASS()
class TORTUNABO_API UTN_ScorePickupWakeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UTN_ScorePickupWakeSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return Entries.Num() > 0 && !IsTemplate(); }

	/** La concha se apunta al empezar: se decide ya si está despierta. */
	void Register(ATN_ScorePickup* Pickup);

	/** Al irse (EndPlay). */
	void Unregister(ATN_ScorePickup* Pickup);

private:
	struct FEntry
	{
		TWeakObjectPtr<ATN_ScorePickup> Pickup;
		bool bAwake = false;
	};

	TArray<FEntry> Entries;
	float Clock = 0.f;

	/** Dónde está la cámara local; false si aún no hay (entonces todas despiertas, como antes). */
	bool GetViewLocation(FVector& OutLocation) const;

	/** Decide y aplica el estado de una concha. */
	void Evaluate(FEntry& Entry, bool bHasView, const FVector& ViewLocation) const;
};
