#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "TN_BeachSandWormSynth.generated.h"

namespace TNSandWormDSP
{
	// Cola de disparos y retumbar continuo compartidos con el generador de audio (Private/World/Beach/TN_BeachSandWormSynth.cpp).
	struct FShared;
}

/** Efectos sintetizados del gusano de arena (el orden es el del motor DSP). */
UENUM(BlueprintType)
enum class ETNSandWormSfx : uint8
{
	Roar  UMETA(DisplayName = "Rugido"),
	Sand  UMETA(DisplayName = "Arena que revienta"),
	Bite  UMETA(DisplayName = "Bocado"),
	Gulp  UMETA(DisplayName = "Trago"),
	Burp  UMETA(DisplayName = "Eructo"),
};

/**
 * Sonidos del gusano de arena gigante del modo carrera (ATN_BeachSandWorm), sintetizados en tiempo real, sin archivos de
 * audio (mismo patrón que UTN_BeachEnemySynthComponent): el retumbar de debajo de la arena mientras avisa y mientras se
 * hunde (SetRumble: ruido muy grave, un sub que oscila, un gruñido que rechina y crujidos de piedrecitas) y cinco
 * disparos: el rugido grave al salir, la arena que revienta, el bocado («¡ÑAM!»: dos mordiscos húmedos con chasquido de
 * dientes), el trago («glup» que baja de tono) y el eructo (vibración ronca con vocales que cambian).
 *
 * El sonido lo genera un ISoundGenerator en el hilo de render de audio sin UObjects, asignaciones ni bloqueos; el hilo de
 * juego deja disparos en una cola circular sin bloqueos y escribe el nivel del retumbar en un atómico. Mono y
 * espacializado con la atenuación hecha en código. Solo suena cuando hace falta: arranca si el oyente está a su alcance y
 * se para tras unos segundos de silencio.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class TORTUNABO_API UTN_BeachSandWormSynthComponent : public USynthComponent
{
	GENERATED_BODY()

public:
	UTN_BeachSandWormSynthComponent(const FObjectInitializer& ObjectInitializer);

	virtual void OnRegister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Crea el componente en InOwner enganchado a InParent (o a su raíz) y lo registra. Null en servidor dedicado, sin
	 * audio o fuera de un mundo de juego.
	 */
	static UTN_BeachSandWormSynthComponent* AttachTo(AActor* InOwner, USceneComponent* InParent, float InInnerRadius, float InFalloff);

	/** Dispara un efecto. Pitch multiplica las frecuencias (1 = de serie) y Volume la ganancia (0..2). */
	void Play(ETNSandWormSfx Sound, float Pitch = 1.f, float Volume = 1.f);

	/** Retumbar de debajo de la arena: 0..1 (0 = callado). Se suaviza en el hilo de audio. */
	void SetRumble(float Level);

	/** Radio con volumen pleno (cm). Solo tiene efecto antes de registrar el componente. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beach|Audio", meta = (ClampMin = "0.0"))
	float InnerRadius = 3000.f;

	/** Distancia (cm) desde el radio interior en la que se apaga. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beach|Audio", meta = (ClampMin = "100.0"))
	float FalloffDistance = 30000.f;

	/** Volumen general de este componente. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beach|Audio", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float Loudness = 1.f;

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual ISoundGeneratorPtr CreateSoundGenerator(const FSoundGeneratorInitParams& InParams) override;

private:
	void ConfigureSpatial();
	bool IsListenerNear() const;
	void EnsurePlaying();

	TSharedPtr<TNSandWormDSP::FShared, ESPMode::ThreadSafe> Shared;

	/** Segundos de silencio que quedan antes de parar el sintetizador. */
	float SilenceLeft = 0.f;
	float RumbleLevel = 0.f;
};
