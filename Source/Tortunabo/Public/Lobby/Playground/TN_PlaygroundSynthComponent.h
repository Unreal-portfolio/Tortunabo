#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "TN_PlaygroundSynthComponent.generated.h"

namespace TNPlaygroundSynthDSP
{
	// Cola de disparos compartida con el generador de audio (Private/Lobby/Playground/TN_PlaygroundSynthComponent.cpp).
	struct FSfxShared;
}

/** Efectos sintetizados del parque de pruebas del lobby (el orden es el del motor DSP). */
UENUM(BlueprintType)
enum class ETNPlaygroundSound : uint8
{
	Boing   UMETA(DisplayName = "Boing de medusa"),
	Creak   UMETA(DisplayName = "Crujido de madera y cuerda"),
	Bonk    UMETA(DisplayName = "Golpe de pala de plástico"),
	Whoosh  UMETA(DisplayName = "Barrido de la pala"),
};

/**
 * Efectos de sonido sintetizados en tiempo real (sin archivos de audio) para las piezas del parque de pruebas: el
 * «boing» de la medusa (tono de muelle con vibrato que se apaga, filtro de «oi» que se cierra, golpe grave y chapoteo),
 * el crujido de los tablones y las cuerdas del puente (impulsos de fricción irregulares por dos resonadores de madera),
 * el golpe de plástico de la pala y el barrido de aire cuando pasa cerca.
 *
 * Mismo patrón que UTN_MusicSynthComponent y UTN_AmbientSynthComponent: el sonido lo genera un ISoundGenerator en el hilo
 * de render de audio, sin UObjects, asignaciones ni bloqueos; el hilo de juego solo deja disparos en una cola circular
 * sin bloqueos (un productor y un consumidor). Mono y espacializado, con la atenuación hecha en código.
 *
 * Solo suena cuando hace falta: TriggerSound arranca el sintetizador si el oyente está a su alcance y lo para tras unos
 * segundos de silencio, para no tener voces del mezclador ocupadas por piezas calladas.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class TORTUNABO_API UTN_PlaygroundSynthComponent : public USynthComponent
{
	GENERATED_BODY()

public:
	UTN_PlaygroundSynthComponent(const FObjectInitializer& ObjectInitializer);

	virtual void OnRegister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Dispara un efecto. Pitch multiplica las frecuencias (1 = tono de serie) y Volume la ganancia (0..2). No hace nada si
	 * el oyente más cercano está fuera de alcance (no se acumulan disparos viejos para cuando vuelva).
	 */
	UFUNCTION(BlueprintCallable, Category = "Playground|Audio")
	void TriggerSound(ETNPlaygroundSound Sound, float Pitch = 1.f, float Volume = 1.f);

	/**
	 * Crea, coloca en InWorldLocation y registra el componente en InOwner (enganchado a su raíz). Devuelve null en servidor
	 * dedicado, sin audio o fuera de un mundo de juego (en el editor no suena nada).
	 */
	static UTN_PlaygroundSynthComponent* AttachTo(AActor* InOwner, const FVector& InWorldLocation, float InInnerRadius = 400.f,
		float InFalloff = 2500.f);

	/** Radio con volumen pleno (cm). Solo tiene efecto antes de registrar el componente. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Playground|Audio", meta = (ClampMin = "0.0"))
	float InnerRadius = 400.f;

	/** Distancia (cm) desde el radio interior en la que se apaga. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Playground|Audio", meta = (ClampMin = "100.0"))
	float FalloffDistance = 2500.f;

	/** Volumen general de este componente. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Playground|Audio", meta = (ClampMin = "0.0", ClampMax = "2.0"))
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
	TSharedPtr<TNPlaygroundSynthDSP::FSfxShared, ESPMode::ThreadSafe> SfxQueue;

	/** Segundos de silencio que quedan antes de parar el sintetizador. */
	float SilenceLeft = 0.f;
};
