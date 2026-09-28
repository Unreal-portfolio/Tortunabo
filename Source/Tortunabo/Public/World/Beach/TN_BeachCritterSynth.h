#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "TN_BeachCritterSynth.generated.h"

namespace TNBeachCritterDSP
{
	// Cola de disparos y niveles continuos compartidos con el generador de audio (Private/World/Beach/TN_BeachCritterSynth.cpp).
	struct FCritterShared;
}

/**
 * Efectos sintetizados de los enemigos de la ronda 3 de la carrera (ermitaño, pulpo, pulgas y tanque de juguete). El
 * orden es el del motor DSP.
 */
enum class ETNBeachCritterSfx : uint8
{
	/** «¡Plop!» del ermitaño al meterse en su concha. */
	ShellPop,
	/** Bolos: golpe hueco de la concha y traqueteo de caparazones. */
	Strike,
	/** La concha cae tras un bote. */
	Thud,
	/** La concha se sacude (arena y chasquidos). */
	Rattle,
	/** Patitas del ermitaño andando. */
	Tap,
	/** Chapoteo en el agua. */
	Splash,
	/** Burbuja que sube y revienta. */
	Bubble,
	/** Tentáculo que agarra: palmada mojada y ventosa. */
	Slap,
	/** Chorro de tinta. */
	Ink,
	/** Picor de las pulgas: chisporroteo y chillidos diminutos. */
	Itch,
	/** «¡Pomp!» del cañón de juguete (tapón). */
	FoamPop,
	/** «¡Paf!» de una bolita de espuma al dar. */
	FoamHit,
	/** Tos del motor del tanque aturdido. */
	Sputter,
	/** Muelle de la antena. */
	Boing,
	/** Servo de la torreta. */
	Servo,
};

/**
 * Sonidos de los enemigos de la ronda 3 sintetizados en tiempo real, sin archivos de audio (el patrón de
 * UTN_BeachEnemySynthComponent): golpes cortos (ETNBeachCritterSfx) y tres sonidos continuos con su nivel: la concha que
 * rueda (SetRoll), el enjambre de pulgas (SetSwarm) y el motor eléctrico del tanque de juguete (SetMotor).
 *
 * El sonido lo genera un ISoundGenerator en el hilo de render de audio sin UObjects, asignaciones ni bloqueos; el hilo
 * de juego deja disparos en una cola circular sin bloqueos y escribe los niveles continuos en atómicos. Mono y
 * espacializado con la atenuación hecha en código. Solo suena cuando hace falta: arranca si el oyente está a su alcance y
 * se para tras unos segundos de silencio.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class TORTUNABO_API UTN_BeachCritterSynthComponent : public USynthComponent
{
	GENERATED_BODY()

public:
	UTN_BeachCritterSynthComponent(const FObjectInitializer& ObjectInitializer);

	virtual void OnRegister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Crea el componente en InOwner enganchado a InParent (o a su raíz) y lo registra. Null en servidor dedicado, sin
	 * audio o fuera de un mundo de juego.
	 */
	static UTN_BeachCritterSynthComponent* AttachTo(AActor* InOwner, USceneComponent* InParent, float InInnerRadius, float InFalloff);

	/** Dispara un efecto. Pitch multiplica las frecuencias (1 = de serie) y Volume la ganancia (0..2). */
	void Play(ETNBeachCritterSfx Sound, float Pitch = 1.f, float Volume = 1.f);

	/** Concha que rueda: Level 0..1 (0 = callada) y velocidad 0..1. Se suaviza en el hilo de audio. */
	void SetRoll(float Level, float Speed);

	/** Enjambre de pulgas: 0..1 (0 = callado). */
	void SetSwarm(float Level);

	/** Motor eléctrico del tanque: Level 0..1 (0 = callado) y revoluciones 0..1. */
	void SetMotor(float Level, float Rpm);

	/** Radio con volumen pleno (cm). Solo tiene efecto antes de registrar el componente. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beach|Audio", meta = (ClampMin = "0.0"))
	float InnerRadius = 1200.f;

	/** Distancia (cm) desde el radio interior en la que se apaga. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beach|Audio", meta = (ClampMin = "100.0"))
	float FalloffDistance = 8000.f;

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
	void KeepAliveFor(float Seconds);

	TSharedPtr<TNBeachCritterDSP::FCritterShared, ESPMode::ThreadSafe> Shared;

	/** Segundos de silencio que quedan antes de parar el sintetizador. */
	float SilenceLeft = 0.f;
	float RollLevel = 0.f;
	float SwarmLevel = 0.f;
	float MotorLevel = 0.f;
};
