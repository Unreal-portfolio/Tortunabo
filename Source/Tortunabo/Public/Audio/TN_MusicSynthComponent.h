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
	None       UMETA(DisplayName = "Silencio"),
	Shop       UMETA(DisplayName = "Tienda (La Concha Dorada)"),
	Booth      UMETA(DisplayName = "Probador"),
	Victory    UMETA(DisplayName = "Victoria (fanfarria y bucle festivo)"),
	Defeat     UMETA(DisplayName = "Derrota (trombón y bucle tristón)"),
	Eliminated UMETA(DisplayName = "Eliminado (jingle corto, sin bucle)"),
};

/**
 * Música original sintetizada en tiempo real (sin archivos de audio), compuesta como secuencias de notas en código
 * (ver TN_MusicSynthDSP.h):
 *  - Tienda: calipso tropical alegre («La Concha Dorada», Fa mayor, 112 BPM, AABA de 32 compases) con steel pan,
 *    marimba/kalimba, bajo pizzicato, congas, shaker y un «ding» de caja registradora.
 *  - Probador: lounge/bossa juguetón (Sol mayor, 96 BPM, ABA de 24 compases) con piano eléctrico FM tipo Rhodes, bajo
 *    andante, escobillas, silbido y arpegios de brillo mágico.
 *  - Victoria: fanfarria «¡ta-ta-ta-tááán!» de 5 s (metales FM, redoble de caja y timbal, platillo) y luego un bucle
 *    festivo suave de calipso (Si bemol mayor, 120 BPM, AABA de 32 compases) con steel pan en trémolo, marimba,
 *    bajo, shaker y congas.
 *  - Derrota: «wah-wah-wah-waaah» de trombón con sordina que baja cromáticamente (3,5 s) y luego un bucle tristón y
 *    gracioso (Re menor, 72 BPM, AB de 16 compases) con bajo pizzicato de puntillas, silbido desafinado, tic-tac de
 *    caja china y un suspiro del trombón.
 *  - Eliminado: jingle de 2 s sin bucle (silbato de émbolo que se desploma, «¡toc!», «uh-oh» de kalimba y un «ploc»).
 *
 * El sonido lo genera un ISoundGenerator (TNMusic::FMusicEngine, TN_MusicSynthDSP.h) en el hilo de render de audio:
 * sin UObjects, sin asignaciones ni bloqueos allí. Este componente solo escribe objetivos en unos parámetros
 * atómicos compartidos con él (hilo de juego); el motor los alcanza con un fundido cruzado de potencia constante
 * entre capas (la pista saliente, la entrante y una de reserva), así que PlayTrack se puede llamar cuantas veces haga
 * falta sin miedo a cortes. Un limitador suave cierra la mezcla (picos por debajo de -1 dBFS, temas en bucle entre
 * -18 y -22 dBFS RMS; validado fuera del motor en Scripts/scratchpad).
 *
 * 2D (menús, tienda, probador y música de fin de partida): estéreo, sin espacializar. 3D (p. ej. el puesto del
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
	 * Cambia a InTrack con un fundido cruzado de InFadeSeconds (potencia constante, sin clics; los jingles entran
	 * enseguida y solo la pista saliente usa el fundido entero). Si InTrack ya es la pista sonando o a la que se está
	 * llegando, no hace nada (no la reinicia, tampoco un jingle que acaba en bucle); un jingle sin bucle que ya ha
	 * terminado sí vuelve a sonar. ETNMusicTrack::None equivale a StopMusic con ese mismo fundido. En 2D vuelve a
	 * arrancar el componente si está parado (tras un viaje sin cortes llega registrado pero parado).
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
	 * menús, tienda, probador y fin de partida. Devuelve null en servidor dedicado, sin audio de juego o fuera de un
	 * mundo de juego. Empieza en silencio: llama a PlayTrack para arrancar un tema.
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
