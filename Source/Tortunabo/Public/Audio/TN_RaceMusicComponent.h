#pragma once

#include "CoreMinimal.h"
#include "Audio/TN_MusicSynthComponent.h"
#include "TN_RaceMusicComponent.generated.h"

namespace TNRaceMusic
{
	// Parámetros atómicos compartidos con el generador de audio (Private/Audio/TN_RaceMusicDSP.h).
	struct FRaceMusicParams;
}

/** Estado del motor de la música de la carrera para depuración (lo escribe el hilo de audio; TN.Race.Music.Status). */
struct FTNRaceMusicDebugInfo
{
	bool bRunning = false;
	/** Compás dentro de la vuelta de 32 (-1 en la introducción). */
	int32 Bar = -1;
	/** Vuelta 0 o 1 (la pieza dura dos vueltas antes de repetirse igual). */
	int32 Pass = 0;
	float Tension = 0.f;
	float Duck = 0.f;
};

/**
 * Música de fondo de la carrera en la playa («Marcha de la Playa»), sintetizada en tiempo real en el hilo de audio con el
 * motor de TN_RaceMusicDSP.h: una banda militar de vacaciones (oom-pah de marcha, caja de marcha, steel pan, flauta de
 * banda, marimbas, congas y campana de barco) con una capa de tensión y un «ducking» controlables. Detalle de la
 * composición y del control en Docs/Sonido_Tortuga.md («Música de la carrera»).
 *
 * Hereda de UTN_MusicSynthComponent solo para caer en la categoría Música del menú de pausa y sus ajustes de volumen
 * (UTN_GameSettingsSubsystem::ClassFor mira IsA<UTN_MusicSynthComponent>); el motor de temas de aquel (PlayTrack,
 * StopMusic, SetMusicVolume...) NO se usa aquí y no hace nada útil: la pieza se controla con las funciones de esta clase.
 * Es 2D (estéreo, sin espacializar) y se crea en el PlayerController local (UTN_RaceMusicSubsystem) en cada máquina con
 * audio: la música no se replica.
 */
UCLASS(ClassGroup = (Audio))
class TORTUNABO_API UTN_RaceMusicComponent : public UTN_MusicSynthComponent
{
	GENERATED_BODY()

public:
	UTN_RaceMusicComponent(const FObjectInitializer& ObjectInitializer);

	/**
	 * Crea, registra y arranca un componente de música de carrera en InOwner. Devuelve null en servidor dedicado, sin
	 * audio de juego o fuera de un mundo de juego. Empieza callado: llama a SetRacePlaying(true) para que suene.
	 */
	static UTN_RaceMusicComponent* AttachRaceMusic(AActor* InOwner);

	/**
	 * true = suena (empieza por la introducción y el compás 1 si estaba parada); false = se funde a silencio en
	 * InFadeSeconds y el motor deja de calcular (CPU a cero). Cambiar solo la duración del fundido no reinicia nada.
	 */
	void SetRacePlaying(bool bInPlay, float InFadeSeconds);

	/** Capa de tensión 0..1 (el motor la suaviza: sube en ~2 s y baja en ~3 s). */
	void SetRaceTension(float InTension);

	/** «Ducking» 0..1: 0 no se aparta, 1 baja 10 dB y cierra un paso bajo (entra en ~0,3 s y sale en ~0,9 s). */
	void SetRaceDuck(float InDuck);

	/** Volumen general (0..1,5), suavizado en ~0,25 s. */
	void SetRaceVolume(float InVolume);

	/** Máscara de capas (depuración): 1 ritmo, 2 armonía, 4 melodía, 8 corneta, 16 tensión; 31 = todo. */
	void SetRaceLayerMask(int32 InMask);

	/** Vuelve al compás 1 (con la introducción) si está sonando. */
	void RestartRacePiece();

	FTNRaceMusicDebugInfo GetRaceDebugInfo() const;

protected:
	virtual ISoundGeneratorPtr CreateSoundGenerator(const FSoundGeneratorInitParams& InParams) override;

private:
	/** Parámetros compartidos con el generador del hilo de audio (atómicos; los dos los mantienen vivos). */
	TSharedPtr<TNRaceMusic::FRaceMusicParams, ESPMode::ThreadSafe> RaceParams;
};
