#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TN_RaceMusicDirector.h"
#include "TN_RaceMusicSubsystem.generated.h"

class APlayerController;
class UTN_RaceMusicComponent;

/**
 * Música de fondo de la carrera en la playa, en cada máquina con jugador y solo para su jugador local: un
 * UTN_RaceMusicComponent 2D colgado del PlayerController local (como la música de fin de partida). Sin RPC y sin tocar
 * el GameMode: vigila diez veces por segundo el estado que ya replica ATN_BeachRaceGameState (RacePhase, bSprintFinal,
 * PhaseSecondsLeft, FinishCountdown y el tiempo de la ronda) y el PlayerState local, y aplica lo que decide
 * TNRaceMusic::FDirector (TN_RaceMusicDirector.h, lógica pura probada fuera del motor).
 *
 * Solo existe donde alguien escucha (ni servidor dedicado ni ejecuciones sin audio). El componente se crea la primera vez
 * que hay que sonar, se destruye tras 20 s en silencio y al empezar a desmontarse el mundo (vuelta al lobby, salida al
 * menú u otro mapa): el PlayerController viaja al mundo nuevo, pero sin esta música. Cede el sitio a la música de fin de
 * partida (UTN_MatchMusicSubsystem): mientras el componente de esta suene victoria, derrota o eliminado, se calla.
 *
 * Pruebas por consola (cualquier mapa, también fuera de la carrera): TN.Race.Music.Play [tensión] [duck], .Stop, .Auto,
 * .Restart, .Status y las variables TN.Race.Music.Volume|Force|Tension|Duck|Layers|Debug.
 */
UCLASS()
class TORTUNABO_API UTN_RaceMusicSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** TN.Race.Music.Restart: vuelve a empezar la pieza desde la introducción. */
	void DebugRestart();

	/** TN.Race.Music.Status: escribe en el registro qué decide el director y cómo va el motor. */
	void LogStatus() const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void Poll(UWorld& InWorld, double InNowSeconds);

	/** Aplica la decisión al componente (lo crea si hace falta sonar y no existe). */
	void Apply(APlayerController* InLocalController, const TNRaceMusic::FDirectorDecision& InDecision, double InNowSeconds);

	UTN_RaceMusicComponent* EnsureMusic(APlayerController* InLocalController);
	void ReleaseMusic();

	/** true si el jugador local tiene sonando victoria, derrota o eliminado (UTN_MatchMusicSubsystem). */
	bool IsMatchMusicPlaying(const APlayerController* InLocalController) const;

	void HandleWorldBeginTearDown(UWorld* InWorld);

	TWeakObjectPtr<UTN_RaceMusicComponent> Music;
	TNRaceMusic::FDirector Director;
	TNRaceMusic::FDirectorDecision LastDecision;
	FDelegateHandle TearDownHandle;
	double NextPollSeconds = 0.0;
	double SilentSinceSeconds = 0.0;
	double NextDebugLogSeconds = 0.0;
	bool bAppliedPlay = false;
};
