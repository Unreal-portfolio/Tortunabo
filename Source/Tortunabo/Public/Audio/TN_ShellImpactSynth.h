#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "TN_ShellImpactSynth.generated.h"

namespace TNShellImpact
{
	// Cola de disparos compartida con el generador de audio (Private/Audio/TN_ShellImpactDSP.h).
	struct FQueue;
}

/**
 * Timbres de los golpes del caparazón (el orden es el del motor DSP de TN_ShellImpactDSP.h; si se añade uno, al final y
 * antes de Count).
 */
UENUM(BlueprintType)
enum class ETNShellImpactSound : uint8
{
	/** Arena o tierra blanda: «¡fum!» apagado, sin resonancia. */
	Sand    UMETA(DisplayName = "Arena"),
	/** Roca: «¡tac!» seco con tres resonancias de piedra. */
	Rock    UMETA(DisplayName = "Roca"),
	/** Madera: «¡toc!» hueco de tablón. */
	Wood    UMETA(DisplayName = "Madera"),
	/** Agua: «¡plof!» con burbuja y gotas. */
	Water   UMETA(DisplayName = "Agua"),
	/** Otra tortuga: «¡clonc!» de caparazón contra caparazón. */
	Turtle  UMETA(DisplayName = "Otra tortuga"),
	/** Un enemigo: «¡boing!» de dibujos animados. */
	Enemy   UMETA(DisplayName = "Enemigo"),
	/** Trastos de los bañistas: «¡pok!» de plástico y lata. */
	Junk    UMETA(DisplayName = "Trastos"),
	Count   UMETA(Hidden)
};

/**
 * Sintetizador de los golpes del caparazón (bola física de la tortuga metida en su caparazón), sin archivos de audio. Mismo
 * patrón que UTN_RaceItemSynthComponent: el sonido lo genera un ISoundGenerator en el hilo de audio (C++ puro, sin UObjects,
 * asignaciones ni bloqueos); el hilo de juego solo deja disparos en una cola circular sin bloqueos. Mono y espacializado,
 * con la atenuación hecha en código. Solo suena cuando hace falta: Play arranca el sintetizador si el oyente está a su
 * alcance y lo para tras unos segundos de silencio. Lo crea y lo maneja UTN_ShellImpactFXComponent.
 *
 * Categoría de volumen del menú de pausa: Efectos (la de todo lo sintetizado que no es música ni ambiente).
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class TORTUNABO_API UTN_ShellImpactSynthComponent : public USynthComponent
{
	GENERATED_BODY()

public:
	UTN_ShellImpactSynthComponent(const FObjectInitializer& ObjectInitializer);

	virtual void OnRegister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Dispara un golpe. Strength (0..1) es la fuerza del impacto (cambia el brillo, la duración y la energía), Pitch multiplica
	 * las frecuencias (1 = de serie) y Volume la ganancia (0..2). Devuelve false si no se ha disparado (sin oyente a su alcance).
	 */
	bool Play(ETNShellImpactSound Sound, float Strength, float Pitch = 1.f, float Volume = 1.f);

	/** Crea, registra y engancha el componente a la raíz de InOwner. Devuelve null en servidor dedicado, sin audio o fuera de un mundo de juego. */
	static UTN_ShellImpactSynthComponent* AttachTo(AActor* InOwner);

	/** Radio con volumen pleno (cm). Solo tiene efecto antes de registrar el componente. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShellImpact|Audio", meta = (ClampMin = "0.0"))
	float InnerRadius = 400.f;

	/** Distancia (cm) desde el radio interior en la que se apaga. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShellImpact|Audio", meta = (ClampMin = "100.0"))
	float FalloffDistance = 3200.f;

	/** Volumen general de este componente. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShellImpact|Audio", meta = (ClampMin = "0.0", ClampMax = "2.0"))
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
	TSharedPtr<TNShellImpact::FQueue, ESPMode::ThreadSafe> Queue;

	/** Segundos de silencio que quedan antes de parar el sintetizador. */
	float SilenceLeft = 0.f;

	/** Semilla del azar de los golpes (cada disparo pasa a la siguiente). */
	uint32 NextSeed = 1u;
};
