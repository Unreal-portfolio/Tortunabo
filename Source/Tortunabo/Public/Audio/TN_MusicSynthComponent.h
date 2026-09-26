#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "TN_MusicSynthComponent.generated.h"

namespace TNMusic
{
	// Parámetros atómicos compartidos con el generador de audio (Private/Audio/TN_MusicSynthDSP.h).
	struct FMusicSharedParams;
}

/** Pistas de música sintetizada disponibles (el orden coincide con TNMusic::ETrack). */
UENUM(BlueprintType)
enum class ETNMusicTrack : uint8
{
	None  UMETA(DisplayName = "Silencio"),
	Shop  UMETA(DisplayName = "Tienda (La Concha Dorada)"),
	Booth UMETA(DisplayName = "Probador"),
};

/**
 * Música original sintetizada en tiempo real (sin archivos de audio) para la tienda y el probador del lobby: un
 * calipso tropical alegre («La Concha Dorada», Fa mayor, 112 BPM, forma AABA de 32 compases) y un lounge/bossa
 * juguetón para el probador (Sol mayor, 96 BPM, forma ABA de 24 compases), cada uno compuesto como secuencias de
 * notas en código (ver TN_MusicSynthDSP.h) con instrumentos sintetizados: steel pan, marimba/kalimba, bajo
 * pizzicato, congas, shaker y un «ding» de caja registradora en la tienda; piano eléctrico FM tipo Rhodes, bajo
 * andante, escobillas, silbido/flauta y arpegios de brillo mágico en el probador.
 *
 * El sonido lo genera un ISoundGenerator (TNMusic::FMusicEngine, TN_MusicSynthDSP.h) en el hilo de render de audio:
 * sin UObjects, sin asignaciones ni bloqueos allí. Este componente solo escribe objetivos en unos parámetros
 * atómicos compartidos con él (hilo de juego); el motor los alcanza con un fundido cruzado de potencia constante
 * entre dos capas (la pista saliente y la entrante), así que PlayTrack se puede llamar cuantas veces haga falta sin
 * miedo a cortes. Un limitador suave cierra la mezcla (picos por debajo de -1 dBFS, niveles medidos entre -18 y
 * -20 dBFS RMS; validado fuera del motor en Scripts/scratchpad).
 *
 * 2D (menús, tienda y probador vistos en la interfaz): estéreo, sin espacializar. 3D (p. ej. el puesto del
 * tendero en el mundo): mono, con atenuación por distancia y radio interior/caída como las fuentes puntuales de
 * UTN_AmbientSynthComponent; se para sola lejos del oyente para no gastar CPU y retoma al acercarse.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class TORTUNABO_API UTN_MusicSynthComponent : public USynthComponent
{
	GENERATED_BODY()

public:
	UTN_MusicSynthComponent(const FObjectInitializer& ObjectInitializer);

	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Cambia a InTrack con un fundido cruzado de InFadeSeconds (potencia constante, sin clics). Si InTrack ya es la
	 * pista sonando o a la que se está llegando, no hace nada (no la reinicia). ETNMusicTrack::None equivale a
	 * StopMusic con ese mismo fundido.
	 */
	UFUNCTION(BlueprintCallable, Category = "Music")
	void PlayTrack(ETNMusicTrack InTrack, float InFadeSeconds = 0.8f);

	/** Silencia con un fundido de InFadeSeconds; al terminar, el motor deja de calcular nada (CPU a cero). */
	UFUNCTION(BlueprintCallable, Category = "Music")
	void StopMusic(float InFadeSeconds = 0.8f);

	/** Volumen general de este componente (0..1; admite hasta 1,5 para reforzar), suavizado en ~0,25 s. */
	UFUNCTION(BlueprintCallable, Category = "Music")
	void SetMusicVolume(float InVolume);

	/** Pista a la que se ha pedido ir (puede seguir en fundido de entrada). */
	UFUNCTION(BlueprintPure, Category = "Music")
	ETNMusicTrack GetRequestedTrack() const;

	/**
	 * Crea, registra y arranca un componente de música 2D (estéreo, sin espacializar) en InOwner: para la música de
	 * menús, tienda y probador. Devuelve null en servidor dedicado, sin audio de juego o fuera de un mundo de juego.
	 * Empieza en silencio: llama a PlayTrack para arrancar un tema.
	 */
	static UTN_MusicSynthComponent* AttachMusic2D(AActor* InOwner);

	/**
	 * Crea, coloca en InWorldLocation, registra y arranca un componente de música 3D (mono, espacializado) en
	 * InOwner con InTrack sonando ya a InVolume: por ejemplo, el puesto de la tienda. Radio pleno InInnerRadius y
	 * caída de InFalloff más allá; se para sola lejos del oyente. Null donde no hay audio de juego.
	 */
	static UTN_MusicSynthComponent* AttachMusic3D(AActor* InOwner, const FVector& InWorldLocation, ETNMusicTrack InTrack,
		float InVolume, float InInnerRadius = 400.f, float InFalloff = 2500.f);

	/** Fuentes 3D: radio con volumen pleno (cm). Solo tiene efecto antes de registrar el componente. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music|3D")
	bool bSpatial = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music|3D", meta = (EditCondition = "bSpatial"))
	float InnerRadius = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music|3D", meta = (EditCondition = "bSpatial"))
	float FalloffDistance = 2500.f;

	/** Fuentes 3D: se paran lejos del oyente (no calculan nada) y vuelven a sonar al acercarse. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music|3D", meta = (EditCondition = "bSpatial"))
	bool bCullByDistance = true;

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual ISoundGeneratorPtr CreateSoundGenerator(const FSoundGeneratorInitParams& InParams) override;

private:
	/** Canales, espacialización y atenuación según bSpatial (antes de arrancar). */
	void ConfigureChannels();

	/** Fuentes 3D: arranca o para según la distancia al oyente más cercano. */
	void UpdateDistanceCulling();

	/** Parámetros compartidos con el generador del hilo de audio (atómicos; los dos los mantienen vivos). */
	TSharedPtr<TNMusic::FMusicSharedParams, ESPMode::ThreadSafe> SharedParams;

	/** Último volumen pedido por SetMusicVolume (para poder devolverlo con GetRequestedTrack/depuración). */
	float RequestedVolume = 1.f;
};
