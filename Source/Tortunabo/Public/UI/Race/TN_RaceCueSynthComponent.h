#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "TN_RaceCueSynthComponent.generated.h"

namespace TNRaceCueDSP
{
	// Cola de disparos compartida con el generador de audio (Private/UI/Race/TN_RaceCueSynthComponent.cpp).
	struct FCueQueue;
}

/** Avisos sonoros de las pantallas de la carrera (el orden es el del motor DSP). */
UENUM()
enum class ETNRaceCue : uint8
{
	/** «¡Toc!» de caja china de la cuenta atrás (Pitch lo sube en los últimos segundos). */
	Tick,
	/** Silbato de árbitro, «¡pi-piii!», al acabarse la cuenta atrás. */
	TimeUp,
	/** Fanfarria de metales «¡ta-ta-ta-taaan!» con redoble, timbal y platillo: el título del sprint final. */
	Fanfare,
	/** Golpe grave («¡pum!») de las caras que entran en el sprint y del «VS». */
	Slam,
};

/**
 * Sonidos de las pantallas del modo carrera sintetizados en tiempo real (sin archivos de audio): la cuenta atrás tras la
 * primera en el agua (UTN_RaceFinishCountdownWidget) y el título del sprint final (UTN_RaceSprintWidget). En 2D, sin
 * espacializar, en el PlayerController de quien mira. Mismo patrón que UTN_ScoreShellSynthComponent: un ISoundGenerator
 * en el hilo de render de audio sin UObjects, asignaciones ni bloqueos, y una cola de disparos sin bloqueos desde el hilo
 * de juego. Solo suena cuando hace falta y se para tras unos segundos de silencio.
 */
UCLASS(ClassGroup = (Audio))
class TORTUNABO_API UTN_RaceCueSynthComponent : public USynthComponent
{
	GENERATED_BODY()

public:
	UTN_RaceCueSynthComponent(const FObjectInitializer& ObjectInitializer);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Dispara un aviso. Pitch multiplica las frecuencias (1 = de serie) y Volume la ganancia (0..2). */
	void Play(ETNRaceCue Cue, float Pitch = 1.f, float Volume = 1.f);

	/** El componente 2D de InOwner (lo crea y lo registra la primera vez). Null sin audio o fuera de un mundo de juego. */
	static UTN_RaceCueSynthComponent* Attach2D(AActor* InOwner);

	/** Volumen general. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Race|Audio", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float Loudness = 0.8f;

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual ISoundGeneratorPtr CreateSoundGenerator(const FSoundGeneratorInitParams& InParams) override;

private:
	TSharedPtr<TNRaceCueDSP::FCueQueue, ESPMode::ThreadSafe> CueQueue;

	/** Segundos de silencio que quedan antes de parar el sintetizador. */
	float SilenceLeft = 0.f;
};
