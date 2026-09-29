#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "TN_ScoreShellSynthComponent.generated.h"

namespace TNScoreShellDSP
{
	// Cola de disparos compartida con el generador de audio (Private/Audio/TN_ScoreShellSynthComponent.cpp).
	struct FShellShared;
}

/** Sonidos sintetizados de las conchas de puntos (el orden es el del motor DSP). */
UENUM()
enum class ETNScoreShellSound : uint8
{
	/**
	 * «¡Plin!» de campanita al coger una concha, según su tamaño: una nota corta en las pequeñas, las dos de la moneda de
	 * siempre en las normales, arpegio de do mayor con el acorde que queda y chispitas en las grandes y, en las reinas,
	 * un arpegio más largo con un acorde de novena que se abre y más chispitas.
	 */
	Plin,
	/** «Pom» redondo y corto (como una burbuja de madera) del contador del HUD al llegarle cada icono. */
	Pom,
};

/**
 * Efectos de sonido de las conchas de puntos, sintetizados en tiempo real (sin archivos de audio). Mismo patrón que
 * UTN_SearchSynthComponent: un ISoundGenerator en el hilo de render de audio sin UObjects, asignaciones ni bloqueos,
 * una cola de disparos sin bloqueos desde el hilo de juego y la atenuación en código.
 *  - 3D (Attach3D): el «¡plin!» en el mundo, en el estallido de cada concha recogida (ATN_ScoreShellBurst); lo oye
 *    todo el que esté cerca.
 *  - 2D (Attach2D): el «pom» del contador en el HUD del jugador que la ha cogido (va en su PlayerController).
 * Solo suena cuando hace falta y se para tras unos segundos de silencio.
 */
UCLASS(ClassGroup = (Audio))
class TORTUNABO_API UTN_ScoreShellSynthComponent : public USynthComponent
{
	GENERATED_BODY()

public:
	UTN_ScoreShellSynthComponent(const FObjectInitializer& ObjectInitializer);

	virtual void OnRegister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Dispara un sonido. Tier: tamaño de la concha (TNScoreShells::ETier, 0-3); Semitones: transporte (el «pom» sube con
	 * cada icono de la tanda); Volume: 0..2. En 3D no suena si el oyente está fuera de alcance.
	 */
	void TriggerSound(ETNScoreShellSound Sound, uint8 Tier, float Semitones = 0.f, float Volume = 1.f);

	/** Crea y registra un componente 3D en InOwner, en InWorldLocation. Null en servidor dedicado, sin audio o fuera de juego. */
	static UTN_ScoreShellSynthComponent* Attach3D(AActor* InOwner, const FVector& InWorldLocation, float InInnerRadius = 400.f,
		float InFalloff = 3000.f);

	/** Crea y registra un componente 2D (sin espacializar) en InOwner, o devuelve el que ya tenga. Null si no hay audio. */
	static UTN_ScoreShellSynthComponent* Attach2D(AActor* InOwner);

	/** Radio con volumen pleno (cm), en 3D. Solo antes de registrar. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ScoreShell|Audio", meta = (ClampMin = "0.0"))
	float InnerRadius = 400.f;

	/** Distancia (cm) desde el radio interior en la que se apaga, en 3D. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ScoreShell|Audio", meta = (ClampMin = "100.0"))
	float FalloffDistance = 3000.f;

	/** Volumen general. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ScoreShell|Audio", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float Loudness = 1.f;

	/** En el mundo (3D) o en el HUD (2D). Solo antes de registrar. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ScoreShell|Audio")
	bool bSpatial = true;

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual ISoundGeneratorPtr CreateSoundGenerator(const FSoundGeneratorInitParams& InParams) override;

private:
	void ConfigureSpatial();
	bool IsListenerNear() const;

	TSharedPtr<TNScoreShellDSP::FShellShared, ESPMode::ThreadSafe> ShellQueue;

	/** Segundos de silencio que quedan antes de parar el sintetizador. */
	float SilenceLeft = 0.f;
};
