#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Game/TN_BeachRaceGameState.h"
#include "UI/Race/TN_RaceChampionWidget.h"
#include "UI/Race/TN_RaceTallyWidget.h"
#include "TN_RaceScreens.generated.h"

class APlayerController;
class APlayerState;
class ATN_CoopPlayerState;

/**
 * @brief Pantallas del modo carrera en cada máquina con jugador: el recuento de conchas tras cada ronda
 * (UTN_RaceTallyWidget) y la pantalla del campeón con el podio (UTN_RaceChampionWidget, ATN_RacePodiumStage).
 *
 * Sin RPC ni cambios en el PlayerController: cada fotograma mira lo que ya se replica en ATN_BeachRaceGameState
 * (RacePhase, RoundWinner, Champion, Podium, PhaseSecondsLeft, CurrentRound, RoundTarget) y en los ATN_CoopPlayerState
 * (RoundWins y los cosméticos), y pone o quita las pantallas (por encima del HUD, ZOrder 20 y 21):
 *  - RoundResults: el recuento de la ronda. Las conchas de cada jugador se apuntan al empezar la ronda (mientras se
 *    prepara y los primeros segundos de carrera), así que la del ganador vuela aunque su RoundWins llegue antes que la
 *    fase.
 *  - Champion: si la ronda ya tuvo su recuento, directamente la pantalla del campeón; si no (p. ej. TN.Race.Champion),
 *    primero el recuento con la concha que corona al campeón y luego el podio. En el podio suena para todos la música
 *    de victoria (UTN_MatchMusicSubsystem), pasado el momento en que el director de la música decide el resultado.
 *  - Waiting o Racing: nada (se van con un fundido).
 *
 * Vista previa sin jugar, en cualquier mapa: TN.Race.Tally [ganador] [jugadores] [final], TN.Race.Podium [jugadores] y
 * TN.Race.PreviewOff (ver Docs/Modo_Carrera.md).
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
	void StartTallyPreview(int32 WinnerRow, int32 NumPlayers, bool bChampionRound);

	/** Vista previa de la pantalla del campeón con el podio (tu tortuga primera y otras de mentira detrás). */
	void StartPodiumPreview(int32 NumPlayers);

	void StopPreview();

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void TickMatch(float DeltaTime, APlayerController* PC, const ATN_BeachRaceGameState& State);
	void TickPreview(float DeltaTime, APlayerController* PC);
	/** Apunta las conchas de cada jugador al empezar la ronda (preparación y primeros segundos de carrera). */
	void TrackWins(float DeltaTime, const ATN_BeachRaceGameState& State);
	/**
	 * Mientras se juega, dibuja de antemano (una por fotograma) las texturas de las pantallas y las caras de cada jugador
	 * con su piel, para que el recuento no dé tirones al salir.
	 */
	void WarmArt(const ATN_BeachRaceGameState& State);
	FTNRaceTallySetup BuildTally(const ATN_BeachRaceGameState& State, const APlayerController* PC, const APlayerState* Winner);
	FTNRaceChampionSetup BuildChampion(const ATN_BeachRaceGameState& State, const APlayerController* PC) const;
	FTNRaceTallyRow RowOf(const ATN_CoopPlayerState& PS, const APlayerController* PC) const;
	void BuildPreviewRows(APlayerController* PC, int32 NumPlayers, TArray<FTNRaceTallyRow>& OutRows) const;
	void ShowTally(APlayerController* PC, const FTNRaceTallySetup& Setup);
	void ShowChampion(APlayerController* PC, const FTNRaceChampionSetup& Setup);
	void HideAll();
	void PlayVictory(bool bOn);

	UPROPERTY(Transient)
	TObjectPtr<UTN_RaceTallyWidget> Tally;

	UPROPERTY(Transient)
	TObjectPtr<UTN_RaceChampionWidget> ChampionScreen;

	bool bHasPhase = false;
	ETNBeachRacePhase ShownPhase = ETNBeachRacePhase::Waiting;
	int32 ShownRound = -1;
	float PhaseClock = 0.f;
	/** PlayerId de cada columna del recuento (para el ganador que llega tarde por red). */
	TArray<int32> TallyPlayerIds;
	/** Conchas de cada jugador (PlayerId) al empezar la ronda en curso: la de la ronda vuela desde ahí. */
	TMap<int32, int32> WinsAtRoundStart;
	bool bTrackingRound = false;
	float RacingClock = 0.f;
	/** Ronda cuyo recuento ya enseñó volar la concha (el campeón no la repite). */
	int32 LandedRound = -1;
	bool bVictoryPlaying = false;
	/** Por dónde va el dibujo de antemano: texturas fijas y (jugador, cara). */
	int32 WarmStatic = 0;
	int32 WarmCursor = 0;

	bool bPreview = false;
	bool bPreviewChampionAfter = false;
	float PreviewClock = 0.f;
	TArray<FTNRaceTallyRow> PreviewRows;
	int32 PreviewWinner = INDEX_NONE;
};
