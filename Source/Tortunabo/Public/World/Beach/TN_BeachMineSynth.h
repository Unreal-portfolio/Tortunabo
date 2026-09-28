#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "TN_BeachMineSynth.generated.h"

namespace TNBeachMineDSP
{
	// Cola de disparos compartida con el generador de audio (Private/World/Beach/TN_BeachMineSynth.cpp).
	struct FMineSfxQueue;
}

/** Sonidos sintetizados de la mina de la playa (el orden es el del motor DSP). */
UENUM(BlueprintType)
enum class ETNBeachMineSound : uint8
{
	Click   UMETA(DisplayName = "Clic de la espoleta"),
	Beep    UMETA(DisplayName = "Pitido de aviso"),
	Boom    UMETA(DisplayName = "Explosión"),
	Debris  UMETA(DisplayName = "Lluvia de arena y piedrecitas"),
	Rearm   UMETA(DisplayName = "Rearme"),
};

/**
 * Sonidos de la mina de juguete (ATN_BeachMine) sintetizados en tiempo real, sin archivos de audio: el doble clic de
 * plástico al pisarla, el pitido electrónico de aviso, la explosión (chasquido que rasga, golpe grave que cae, retumbo
 * y crepitar), la arena y las piedrecitas que caen después y el clic-clac del rearme.
 *
 * Mismo patrón que UTN_BeachTrapSynthComponent: el sonido lo genera un ISoundGenerator en el hilo de render de audio sin
 * UObjects, asignaciones ni bloqueos; el hilo de juego solo deja disparos en una cola circular sin bloqueos. Mono y
 * espacializado, con la atenuación hecha en código. Solo suena cuando hace falta: Play arranca el sintetizador si el
 * oyente está a su alcance y lo para tras unos segundos de silencio.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class TORTUNABO_API UTN_BeachMineSynthComponent : public USynthComponent
{
	GENERATED_BODY()

public:
	UTN_BeachMineSynthComponent(const FObjectInitializer& ObjectInitializer);

	virtual void OnRegister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Dispara un efecto. Pitch multiplica las frecuencias (1 = de serie) y Volume la ganancia (0..2). */
	void Play(ETNBeachMineSound Sound, float Pitch = 1.f, float Volume = 1.f);

	/**
	 * Crea, coloca en InWorldLocation y registra el componente en InOwner (enganchado a su raíz). Devuelve null en servidor
	 * dedicado, sin audio o fuera de un mundo de juego.
	 */
	static UTN_BeachMineSynthComponent* AttachTo(AActor* InOwner, const FVector& InWorldLocation, float InInnerRadius = 1500.f, float InFalloff = 10000.f);

	/** Radio con volumen pleno (cm). Solo tiene efecto antes de registrar el componente. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beach|Audio", meta = (ClampMin = "0.0"))
	float InnerRadius = 1500.f;

	/** Distancia (cm) desde el radio interior en la que se apaga. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beach|Audio", meta = (ClampMin = "100.0"))
	float FalloffDistance = 10000.f;

	/** Volumen general de este componente. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beach|Audio", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float Loudness = 1.f;

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual ISoundGeneratorPtr CreateSoundGenerator(const FSoundGeneratorInitParams& InParams) override;

private:
	/** Mono, espacializado y con la atenuación en código (antes de arrancar). */
	void ConfigureSpatial();

	/** true si el oyente más cercano puede oír algo de este componente. */
	bool IsListenerNear() const;

	/** Cola de disparos compartida con el generador del hilo de audio (los dos la mantienen viva). */
	TSharedPtr<TNBeachMineDSP::FMineSfxQueue, ESPMode::ThreadSafe> SfxQueue;

	/** Segundos de silencio que quedan antes de parar el sintetizador. */
	float SilenceLeft = 0.f;
};
