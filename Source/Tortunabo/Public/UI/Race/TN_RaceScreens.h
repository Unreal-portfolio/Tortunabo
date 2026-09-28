#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Game/TN_BeachRaceGameState.h"
#include "UI/Race/TN_RaceChampionWidget.h"
#include "UI/Race/TN_RaceFinishCountdownWidget.h"
#include "UI/Race/TN_RaceSprintWidget.h"
#include "UI/Race/TN_RaceTallyWidget.h"
#include "TN_RaceScreens.generated.h"

class APlayerController;
class APlayerState;
class ATN_CoopPlayerState;

/**
 * @brief Pantallas del modo carrera en cada máquina con jugador: la cuenta atrás tras la primera en el agua
 * (UTN_RaceFinishCountdownWidget), el recuento de conchas tras cada ronda (UTN_RaceTallyWidget), el título del sprint
 * final (UTN_RaceSprintWidget) y la pantalla del campeón con el podio (UTN_RaceChampionWidget, ATN_RacePodiumStage).
 *
 * Sin RPC ni cambios en el PlayerController: cada fotograma mira lo que ya se replica en ATN_BeachRaceGameState
 * (RacePhase, RoundWinner, RoundHalfShells, FinishCountdown, Champion, Podium, SprintFinalists, bSprintFinal,
 * PhaseSecondsLeft, CurrentRound, RoundTarget) y en los ATN_CoopPlayerState (RaceShellHalves y los cosméticos), y pone o
 * quita las pantallas (cuenta atrás en el ZOrder 15; recuento, campeón y sprint en el 20, 21 y 22: por encima del HUD):
 *  - Racing con FinishCountdown: la cuenta atrás de 10 s y, al acabar, «¡TIEMPO!».
 *  - RoundResults: el recuento de la ronda (conchas en medias). Las de cada jugador se apuntan al empezar la ronda
 *    (mientras se prepara y los primeros segundos de carrera), así que las de esta ronda vuelan aunque RaceShellHalves
 *    llegue antes que la fase.
 *  - SprintIntro: el título del sprint final con las caras de las finalistas.
 *  - Champion: si la ronda ya tuvo su recuento (o viene del sprint), directamente la pantalla del campeón; si no (p. ej.
 *    TN.Race.Champion), primero el recuento con la concha que corona al campeón y luego el podio. En el podio suena para
 *    todos la música de victoria (UTN_MatchMusicSubsystem), pasado el momento en que el director de la música decide.
 *  - Waiting o Racing: nada más (se van con un fundido).
 *
 * Vista previa sin jugar, en cualquier mapa: TN.Race.Tally [ganador] [jugadores] [final] [medias], TN.Race.Podium
 * [jugadores], TN.Race.SprintPreview [jugadores], TN.Race.CountdownPreview y TN.Race.PreviewOff (Docs/Modo_Carrera.md).
 */
UCLASS()
class TORTUNABO_API UTN_RaceScreensSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/**
	 * Vista previa del recuento con tu tortuga y otras de mentira: WinnerRow es la columna a la que vuela la concha
	 * (-1 = nadie llega al agua), NumPlayers las columnas (1-6) y, con bChampionRound, la concha corona al ganador y
	 * después sale el podio.
	 */
	void StartTallyPreview(int32 WinnerRow, int32 NumPlayers, bool bChampionRound, int32 NumHalves = 1);

	/** Vista previa de la pantalla del campeón con el podio (tu tortuga primera y otras de mentira detrás). */
	void StartPodiumPreview(int32 NumPlayers);

	/** Vista previa del título del sprint final con tu tortuga y otras de mentira (2-6 finalistas). */
	void StartSprintPreview(int32 NumPlayers);

	/** Vista previa de la cuenta atrás de 10 s tras la primera en el agua (y su «¡TIEMPO!»). */
	void StartCountdownPreview();

	void StopPreview();

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void TickMatch(float DeltaTime, APlayerController* PC, const ATN_BeachRaceGameState& State);
	void TickPreview(float DeltaTime, APlayerController* PC);
	/** Apunta las conchas de cada jugador al empezar la ronda (preparación y primeros segundos de carrera). */
	void TrackWins(float DeltaTime, const ATN_BeachRaceGameState& State);
	/** Cuenta atrás tras la primera en el agua: la pone, la alimenta y la quita. */
	void TickCountdown(APlayerController* PC, const ATN_BeachRaceGameState& State);
	/**
	 * Mientras se juega, dibuja de antemano (una por fotograma) las texturas de las pantallas y las caras de cada jugador
	 * con su piel, para que el recuento no dé tirones al salir.
	 */
	void WarmArt(const ATN_BeachRaceGameState& State);
	/**
	 * Recuento de la ronda (la entera de RoundWinner y las medias de RoundHalfShells). Con ChampionOnly, el de la concha
	 * que corona a ese campeón (cuando se saltó el recuento, p. ej. con TN.Race.Champion).
	 */
	FTNRaceTallySetup BuildTally(const ATN_BeachRaceGameState& State, const APlayerController* PC, const APlayerState* ChampionOnly = nullptr);
	FTNRaceSprintSetup BuildSprint(const ATN_BeachRaceGameState& State, const APlayerController* PC) const;
	/** Campeona o empate en lo más alto a partir de las medias finales de cada columna. */
	static void DecideVerdict(FTNRaceTallySetup& Setup, int32 TargetHalves);
	FTNRaceChampionSetup BuildChampion(const ATN_BeachRaceGameState& State, const APlayerController* PC) const;
	FTNRaceTallyRow RowOf(const ATN_CoopPlayerState& PS, const APlayerController* PC) const;
	void BuildPreviewRows(APlayerController* PC, int32 NumPlayers, TArray<FTNRaceTallyRow>& OutRows) const;
	void ShowTally(APlayerController* PC, const FTNRaceTallySetup& Setup);
	void ShowChampion(APlayerController* PC, const FTNRaceChampionSetup& Setup);
	void ShowSprint(APlayerController* PC, const FTNRaceSprintSetup& Setup);
	void HideAll();
	void PlayVictory(bool bOn);

	UPROPERTY(Transient)
	TObjectPtr<UTN_RaceTallyWidget> Tally;

	UPROPERTY(Transient)
	TObjectPtr<UTN_RaceChampionWidget> ChampionScreen;

	UPROPERTY(Transient)
	TObjectPtr<UTN_RaceFinishCountdownWidget> CountdownScreen;

	UPROPERTY(Transient)
	TObjectPtr<UTN_RaceSprintWidget> SprintScreen;

	bool bHasPhase = false;
	ETNBeachRacePhase ShownPhase = ETNBeachRacePhase::Waiting;
	int32 ShownRound = -1;
	float PhaseClock = 0.f;
	/** PlayerId de cada columna del recuento (para el ganador que llega tarde por red). */
	TArray<int32> TallyPlayerIds;
	/** Medias conchas de cada jugador (PlayerId) al empezar la ronda en curso: las de la ronda llegan desde ahí. */
	TMap<int32, int32> HalvesAtRoundStart;
	bool bTrackingRound = false;
	float RacingClock = 0.f;
	/** Ronda cuyo recuento ya enseñó volar la concha (el campeón no la repite). */
	int32 LandedRound = -1;
	bool bVictoryPlaying = false;
	/** Por dónde va el dibujo de antemano: texturas fijas y (jugador, cara). */
	int32 WarmStatic = 0;
	int32 WarmCursor = 0;

	bool bPreview = false;
	/** Qué vista previa: 0 recuento o podio, 1 título del sprint, 2 cuenta atrás. */
	uint8 PreviewKind = 0;
	bool bPreviewChampionAfter = false;
	float PreviewClock = 0.f;
	TArray<FTNRaceTallyRow> PreviewRows;
	int32 PreviewWinner = INDEX_NONE;
};
