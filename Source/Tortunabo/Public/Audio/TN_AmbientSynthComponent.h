#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "World/ProcMap/TN_ProcMapEnums.h"
#include "TN_AmbientSynthComponent.generated.h"

namespace TNAmbientDSP
{
	// Parámetros atómicos compartidos con el generador de audio (Private/Audio/TN_AmbientSynthDSP.h).
	struct FSharedParams;
}

/** Capas del paisaje sonoro sintetizado (el orden es el de TNAmbientDSP::Layer). */
UENUM(BlueprintType)
enum class ETNAmbientLayer : uint8
{
	Wind      UMETA(DisplayName = "Viento"),
	Surf      UMETA(DisplayName = "Oleaje"),
	Stream    UMETA(DisplayName = "Agua corriente"),
	Birds     UMETA(DisplayName = "Pájaros"),
	Cicadas   UMETA(DisplayName = "Cigarras"),
	Crickets  UMETA(DisplayName = "Grillos"),
	Frogs     UMETA(DisplayName = "Ranas"),
	Lava      UMETA(DisplayName = "Lava"),
	Bells     UMETA(DisplayName = "Campanas"),
	Count     UMETA(Hidden)
};

/** Especies de aves (el orden es el de TNAmbientDSP::Bird). */
UENUM(BlueprintType)
enum class ETNAmbientBird : uint8
{
	Whistle     UMETA(DisplayName = "Silbidos tropicales"),
	Trill       UMETA(DisplayName = "Trino"),
	Macaw       UMETA(DisplayName = "Guacamayo"),
	Oropendola  UMETA(DisplayName = "Oropéndola"),
	Gull        UMETA(DisplayName = "Gaviota"),
	Heron       UMETA(DisplayName = "Garza"),
	Piper       UMETA(DisplayName = "Andarríos"),
	Crow        UMETA(DisplayName = "Graja"),
	Eagle       UMETA(DisplayName = "Águila"),
	Dove        UMETA(DisplayName = "Paloma"),
	Sparrow     UMETA(DisplayName = "Gorrión"),
	Hen         UMETA(DisplayName = "Gallina"),
	Count       UMETA(Hidden)
};

/** Qué genera un UTN_AmbientSynthComponent. */
UENUM(BlueprintType)
enum class ETNAmbientSourceKind : uint8
{
	/** Paisaje sonoro por capas: estéreo y sin espacializar (lo crea UTN_AmbientSoundscapeComponent). */
	Soundscape  UMETA(DisplayName = "Paisaje sonoro 2D"),
	/** Cascada: fragor constante con burbujeo en la poza (mono, 3D). */
	Waterfall   UMETA(DisplayName = "Cascada (3D)"),
	/** Géiser: rugido que sigue a SetIntensity y borboteo con el chorro bajo (mono, 3D). */
	Geyser      UMETA(DisplayName = "Géiser (3D)"),
	/** Poza o río de lava: rumor grave, crepitar y burbujas (mono, 3D). */
	LavaPool    UMETA(DisplayName = "Lava (3D)")
};

/**
 * Mezcla completa del paisaje sonoro: volumen objetivo de cada capa, peso de cada especie de ave y forma del viento
 * y del oleaje. UTN_AmbientSoundscapeComponent la calcula mezclando los preajustes de bioma (GetBiomeMix) con el
 * contexto de la cámara (agua cerca, altura, tormenta, cueva) y la aplica con ApplyMix.
 */
struct FTNAmbientMix
{
	static constexpr int32 NumLayers = static_cast<int32>(ETNAmbientLayer::Count);
	static constexpr int32 NumBirds = static_cast<int32>(ETNAmbientBird::Count);

	/** Volumen de cada capa: 0 = apagada, 1 = presencia plena (hasta ~1,5 con viento de tormenta). */
	float Layers[NumLayers] = {};
	/** Peso de cada especie al elegir quién canta (no hace falta que sumen 1). */
	float Birds[NumBirds] = {};
	/** Llamadas de aves por segundo. */
	float BirdRate = 0.f;
	/** 0 = aves cerca y claras; 1 = lejanas y apagadas. */
	float BirdDistance = 0.4f;
	/** Viento: profundidad de las ráfagas, silbido de banda estrecha (desierto, cumbres) y brillo (0..1). */
	float WindGust = 0.4f;
	float WindWhistle = 0.f;
	float WindBright = 0.4f;
	/** Oleaje: 0 = chapoteo de laguna, 1 = rompientes de mar abierto cada 5-9 s. */
	float SurfSize = 0.5f;
	/** Tormenta (0..1): ráfagas más fuertes y truenos lejanos. */
	float Storm = 0.f;
	/** Cierre (0..1): cueva, bajo techo o bajo el agua (paso bajo general). */
	float Enclosure = 0.f;

	/** Todo a cero (punto de partida para sumar preajustes con peso). */
	static FTNAmbientMix Zero();
	/** this += Other * Weight, campo a campo. */
	void AddWeighted(const FTNAmbientMix& Other, float Weight);
	/** Multiplica todos los campos (normaliza una suma con pesos). */
	void Scale(float Factor);
};

/**
 * Ambiente sintetizado en tiempo real (sin assets ni licencias): viento, oleaje, agua corriente, aves, cigarras,
 * grillos, ranas, lava y campanas, o una fuente puntual 3D (cascada, géiser, lava). Calidad de boceto: audio puede
 * sustituir capas por sonidos propios con UTN_AmbienceDataAsset.
 *
 * El sonido lo genera un ISoundGenerator (TNAmbientDSP::FEngine, TN_AmbientSynthDSP.h) en el hilo de render de audio:
 * sin UObjects, sin asignaciones ni bloqueos allí. Este componente solo escribe objetivos en unos parámetros atómicos
 * compartidos con él (hilo de juego); el motor los alcanza suavizados, así que se pueden fijar cada frame.
 *
 * Paisaje 2D: estéreo con ruido distinto en cada canal y un limitador suave al final. Fuentes 3D: mono, espacializadas
 * con una atenuación creada en código; se paran solas lejos del oyente (no gastan CPU) y vuelven al acercarse.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class TORTUNABO_API UTN_AmbientSynthComponent : public USynthComponent
{
	GENERATED_BODY()

public:
	UTN_AmbientSynthComponent(const FObjectInitializer& ObjectInitializer);

	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Crea en InOwner una fuente 3D de agua (cascada o, con bGeyser, géiser), enganchada a su raíz: mono, volumen pleno
	 * hasta ~5 m y caída hasta ~40 m. Devuelve null en servidor dedicado, sin audio o fuera de un mundo de juego (p. ej.
	 * al generar el mapa en el editor). El géiser debe llamar a SetIntensity con su pulso cada frame.
	 */
	static UTN_AmbientSynthComponent* AttachWaterSound(AActor* InOwner, float InLoudness, bool bGeyser);

	/** Como AttachWaterSound, pero en un punto del mundo (p. ej. donde cae el agua de una cascada). */
	static UTN_AmbientSynthComponent* AttachWaterSoundAt(AActor* InOwner, const FVector& InWorldLocation, float InLoudness, bool bGeyser);

	/** Cualquier fuente puntual (lava incluida) en el origen de InOwner, con radio interior y caída en cm. */
	static UTN_AmbientSynthComponent* AttachPointSound(AActor* InOwner, ETNAmbientSourceKind InKind, float InLoudness,
		float InInnerRadius = 500.f, float InFalloffDistance = 3500.f);

	/** Cualquier fuente puntual en un punto del mundo, enganchada a la raíz de InOwner. */
	static UTN_AmbientSynthComponent* AttachPointSoundAt(AActor* InOwner, const FVector& InWorldLocation, ETNAmbientSourceKind InKind,
		float InLoudness, float InInnerRadius = 500.f, float InFalloffDistance = 3500.f);

	/** Preajuste de un bioma (lo que suena con él al 100 %). */
	static void GetBiomeMix(ETNProcBiome Biome, FTNAmbientMix& OutMix);

	/** Preajuste genérico: brisa, pocos pájaros y algún grillo (sin mapa procedural). */
	static void GetGenericMix(FTNAmbientMix& OutMix);

	/** Aplica una mezcla completa (volúmenes, especies y forma). */
	void ApplyMix(const FTNAmbientMix& Mix);

	/** Mezcla aplicada por última vez (depuración). */
	const FTNAmbientMix& GetCurrentMix() const { return CurrentMix; }

	/** Aplica el preajuste de un bioma (paisaje 2D colocado a mano, pruebas). */
	UFUNCTION(BlueprintCallable, Category = "Ambience")
	void ApplyBiomePreset(ETNProcBiome Biome);

	/** Volumen objetivo de una capa (0 = apagada, 1 = plena). */
	UFUNCTION(BlueprintCallable, Category = "Ambience")
	void SetLayerLevel(ETNAmbientLayer InLayer, float Level);

	UFUNCTION(BlueprintPure, Category = "Ambience")
	float GetLayerLevel(ETNAmbientLayer InLayer) const;

	/** Peso de una especie al elegir qué ave canta. */
	UFUNCTION(BlueprintCallable, Category = "Ambience")
	void SetBirdWeight(ETNAmbientBird Species, float Weight);

	/** Fuentes 3D: fuerza del chorro del géiser o del caudal de la cascada (0..1); se sigue en ~50 ms. */
	UFUNCTION(BlueprintCallable, Category = "Ambience")
	void SetIntensity(float InIntensity);

	/** Tormenta (0..1): más ráfagas y truenos. */
	UFUNCTION(BlueprintCallable, Category = "Ambience")
	void SetStormAmount(float Amount);

	/** Cierre (0..1): cueva o bajo el agua; apaga los agudos. */
	UFUNCTION(BlueprintCallable, Category = "Ambience")
	void SetEnclosure(float Amount);

	/** Ganancia general (se multiplica por Loudness). */
	UFUNCTION(BlueprintCallable, Category = "Ambience")
	void SetMasterGain(float Gain);

	/** Segundos que tarda cada capa en alcanzar su volumen nuevo. */
	UFUNCTION(BlueprintCallable, Category = "Ambience")
	void SetSmoothingSeconds(float Seconds);

	/** Qué genera. Se fija antes de registrar el componente: decide mono o estéreo y la espacialización. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience")
	ETNAmbientSourceKind SourceKind = ETNAmbientSourceKind::Soundscape;

	/** Paisaje 2D colocado a mano: aplica el preajuste de PresetBiome al empezar. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience")
	bool bApplyPresetOnBeginPlay = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience", meta = (EditCondition = "bApplyPresetOnBeginPlay"))
	ETNProcBiome PresetBiome = ETNProcBiome::Jungle;

	/** Volumen de esta fuente. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float Loudness = 1.f;

	/** Fuentes 3D: radio con volumen pleno (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience|3D", meta = (ClampMin = "0.0"))
	float InnerRadius = 500.f;

	/** Fuentes 3D: distancia (cm) desde el radio interior en la que se apaga. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience|3D", meta = (ClampMin = "100.0"))
	float FalloffDistance = 3500.f;

	/** Fuentes 3D: se paran lejos del oyente (no calculan nada) y vuelven a sonar al acercarse. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience|3D")
	bool bCullByDistance = true;

	/** Semilla del azar (0 = según la posición: cada fuente suena distinta, pero siempre igual en el mismo sitio). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience", AdvancedDisplay)
	int32 Seed = 0;

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual ISoundGeneratorPtr CreateSoundGenerator(const FSoundGeneratorInitParams& InParams) override;

private:
	/** Crea, coloca (en InWorldLocation si no es null) y registra una fuente 3D; null donde no hay audio de juego. */
	static UTN_AmbientSynthComponent* CreatePointSound(AActor* InOwner, const FVector* InWorldLocation, ETNAmbientSourceKind InKind,
		float InLoudness, float InInnerRadius, float InFalloffDistance);

	/** Canales, espacialización, atenuación y parámetros fijos según SourceKind (antes de arrancar). */
	void ConfigureForKind();

	/** Fuentes 3D: arranca o para según la distancia al oyente más cercano. */
	void UpdateDistanceCulling();

	/** Parámetros compartidos con el generador del hilo de audio (atómicos; los dos los mantienen vivos). */
	TSharedPtr<TNAmbientDSP::FSharedParams, ESPMode::ThreadSafe> SharedParams;

	FTNAmbientMix CurrentMix;
	float MasterGain = 1.f;
};
