#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "TN_BeachSplashSynthComponent.generated.h"

namespace TNBeachSplashDSP
{
	// Cola de disparos compartida con el generador de audio (Private/World/Beach/TN_BeachSplashSynthComponent.cpp).
	struct FSplashQueue;
}

/**
 * El «¡chof!» de una tortuga que cae al agua desde lo alto, sintetizado en tiempo real (sin archivos de audio): el
 * chasquido del golpe contra el agua, la lámina de agua que se abre, el «plom» grave de la cavidad que se cierra, un
 * rosario de burbujas que suben de tono y la lluvia de gotas del chorro al volver a caer. Size escala todo (1 = el salto
 * del acantilado de meta): más grande, más grave, más largo y con más burbujas.
 *
 * Mismo patrón que UTN_BeachTrapSynthComponent: el sonido lo genera un ISoundGenerator en el hilo de render de audio, sin
 * UObjects, asignaciones ni bloqueos; el hilo de juego solo deja disparos en una cola circular sin bloqueos. Mono y
 * espacializado, con la atenuación hecha en código. Solo suena cuando hace falta: PlaySplash arranca el sintetizador si el
 * oyente está a su alcance y lo para tras unos segundos de silencio. Lo usa el chapuzón de la meta
 * (UTN_BeachFinishSplashSubsystem), en cada máquina.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class TORTUNABO_API UTN_BeachSplashSynthComponent : public USynthComponent
{
	GENERATED_BODY()

public:
	UTN_BeachSplashSynthComponent(const FObjectInitializer& ObjectInitializer);

	virtual void OnRegister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Lleva la fuente a WorldAt y dispara un chapuzón de tamaño Size (0,3..1,5; 1 = el salto del acantilado). Pitch
	 * multiplica las frecuencias y Volume la ganancia (0..2). No hace nada si el oyente más cercano está fuera de alcance.
	 */
	UFUNCTION(BlueprintCallable, Category = "Beach|Audio")
	void PlaySplashAt(const FVector& WorldAt, float Size = 1.f, float Pitch = 1.f, float Volume = 1.f);

	/**
	 * Crea el componente en InOwner (enganchado a su raíz) y lo registra. Null en servidor dedicado, sin audio o fuera de
	 * un mundo de juego.
	 */
	static UTN_BeachSplashSynthComponent* AttachTo(AActor* InOwner, float InInnerRadius = 2500.f, float InFalloff = 30000.f);

	/** Radio con volumen pleno (cm). Solo tiene efecto antes de registrar el componente. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beach|Audio", meta = (ClampMin = "0.0"))
	float InnerRadius = 2500.f;

	/** Distancia (cm) desde el radio interior en la que se apaga. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beach|Audio", meta = (ClampMin = "100.0"))
	float FalloffDistance = 30000.f;

	/** Volumen general de este componente. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beach|Audio", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float Loudness = 0.9f;

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual ISoundGeneratorPtr CreateSoundGenerator(const FSoundGeneratorInitParams& InParams) override;

private:
	/** Mono, espacializado y con la atenuación en código (antes de arrancar). */
	void ConfigureSpatial();

	/** true si el oyente más cercano puede oír algo desde WorldAt. */
	bool IsListenerNear(const FVector& WorldAt) const;

	/** Cola de disparos compartida con el generador del hilo de audio (los dos la mantienen viva). */
	TSharedPtr<TNBeachSplashDSP::FSplashQueue, ESPMode::ThreadSafe> SplashQueue;

	/** Segundos de silencio que quedan antes de parar el sintetizador. */
	float SilenceLeft = 0.f;
};
