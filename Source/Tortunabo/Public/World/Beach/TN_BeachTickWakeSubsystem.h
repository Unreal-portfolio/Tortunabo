#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TN_BeachTickWakeSubsystem.generated.h"

class ATN_BeachElement;

/**
 * Cuándo tiene el Tick encendido un elemento de la playa que se duerme de lejos (lógica pura, tests
 * Tortunabo.Perf.BeachTickWake). Despierto si hay una tortuga, un caparazón o una cámara local a menos de su
 * GetTickWakeDistance, o si tiene algo en marcha (IsTickBusy); para dormirse hay que alejarse un poco más (SleepFactor).
 */
namespace TNBeachTickWake
{
	/** Cada cuánto se miran las distancias de todos (s). */
	inline constexpr float CheckInterval = 0.25f;

	/** Se duerme a WakeDistance × SleepFactor: así no parpadea en el borde. */
	inline constexpr float SleepFactor = 1.1f;

	/**
	 * Margen (cm) sobre lo que ocupa el elemento para despertarlo antes de que una tortuga lo alcance: lanzada a 3000 cm/s
	 * recorre 750 cm entre dos miradas, así que llega con el Tick encendido desde hace más de un segundo.
	 */
	inline constexpr float ReachMargin = 3000.f;

	/** Distancia al cuadrado de At al vigilante más cercano; infinita si no hay ninguno. */
	inline double NearestDistSquared(const FVector& At, TConstArrayView<FVector> Watchers)
	{
		double Best = TNumericLimits<double>::Max();
		for (const FVector& Watcher : Watchers)
		{
			Best = FMath::Min(Best, FVector::DistSquared(At, Watcher));
		}
		return Best;
	}

	/** WakeDistance <= 0: no se duerme nunca. */
	inline bool ShouldBeAwake(double NearestDistSq, float WakeDistance, bool bWasAwake, bool bBusy)
	{
		if (bBusy || WakeDistance <= 0.f)
		{
			return true;
		}
		const double Limit = static_cast<double>(WakeDistance) * (bWasAwake ? SleepFactor : 1.f);
		return NearestDistSq < Limit * Limit;
	}
}

/**
 * @brief Enciende y apaga el Tick de los elementos de la playa que lo piden (ATN_BeachElement::GetTickWakeDistance > 0)
 * según lo cerca que estén las tortugas, los caparazones y las cámaras locales, todos de una vez.
 *
 * En reposo había más de 300 actores con Tick en la playa (Docs/Estres-Monkey-2026-09-29.md, coste 7; #59): minas, algas
 * y puertas de conchas tenían el suyo encendido aunque no hubiera nadie en cientos de metros, solo para mirar si alguien
 * las pisaba o para animar lo que nadie ve. Cada máquina decide con lo que tiene: el servidor, con todas las tortugas
 * (sus sensores siguen a tiempo); los clientes, además, con su cámara. TN.Perf.BeachTickWake 0 lo desactiva (todos
 * despiertos, como antes) para comparar.
 */
UCLASS()
class TORTUNABO_API UTN_BeachTickWakeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UTN_BeachTickWakeSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return Elements.Num() > 0 && !IsTemplate(); }

	/** El elemento se apunta en su BeginPlay: se decide ya si está despierto. */
	void Register(ATN_BeachElement* Element);

	/** Al irse (EndPlay). */
	void Unregister(ATN_BeachElement* Element);

private:
	TArray<TWeakObjectPtr<ATN_BeachElement>> Elements;
	float Clock = 0.f;

	/** Tortugas, caparazones y cámaras locales de esta máquina. */
	void GatherWatchers(TArray<FVector>& OutWatchers) const;

	/** Decide y aplica el estado de un elemento. */
	static void Evaluate(ATN_BeachElement& Element, TConstArrayView<FVector> Watchers, bool bEnabled);
};
