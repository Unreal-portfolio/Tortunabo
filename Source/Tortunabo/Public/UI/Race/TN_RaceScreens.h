#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Game/TN_BeachRaceGameState.h"
#include "UI/Race/TN_RaceArrivalWidget.h"
#include "UI/Race/TN_RaceChampionWidget.h"
#include "UI/Race/TN_RaceFinishCountdownWidget.h"
#include "UI/Race/TN_RaceRoundClockWidget.h"
#include "UI/Race/TN_RaceRoundIntroWidget.h"
#include "UI/Race/TN_RaceSprintWidget.h"
#include "UI/Race/TN_RaceTallyWidget.h"
#include "TN_RaceScreens.generated.h"

class APlayerController;
class APlayerState;
class ATN_BeachRaceGenerator;
class ATN_CoopPlayerState;
class UTN_GhostHatchWidget;

/** Para qué está puesta la cáscara oscura del modo carrera (UTN_GhostHatchWidget::ShowCurtain). */
enum class ETNRaceCurtainUse : uint8
{
	None,
	/** Llegada al agua: «Has quedado X.º» encima (UTN_RaceArrivalWidget). */
	Arrival,
	/** Entre ronda y ronda: «RONDA N» y el 3, 2, 1 encima (UTN_RaceRoundIntroWidget). */
	RoundIntro
};

/**
 * @brief Pantallas del modo carrera en cada máquina con jugador: la cuenta atrás tras la primera en el agua
 * (UTN_RaceFinishCountdownWidget), el recuento de conchas tras cada ronda (UTN_RaceTallyWidget), el título del sprint
 * final (UTN_RaceSprintWidget) y la pantalla del campeón con el podio (UTN_RaceChampionWidget, ATN_RacePodiumStage).
 *
 * Sin RPC ni cambios en el PlayerController: cada fotograma mira lo que ya se replica en ATN_BeachRaceGameState
 * (RacePhase, RoundWinner, RoundHalfShells, FinishCountdown, Champion, Podium, SprintFinalists, bSprintFinal,
 * PhaseSecondsLeft, CurrentRound, RoundTarget) y en los ATN_CoopPlayerState (RaceShellHalves y los cosméticos), y pone o
 * quita las pantallas (cuenta atrás en el ZOrder 15; recuento, campeón y sprint en el 20, 21 y 22: por encima del HUD):
 *  - Racing con FinishCountdown: la cuenta atrás de 10 s y, al acabar, «¡TIEMPO!» (con el motivo, RoundEndReason: si se
 *    ha agotado el tiempo de la ronda sin nadie en el agua, lo dice su cinta).
 *  - Racing con el tiempo de la ronda contando (RoundEndServerTime) y 60 s o menos: el reloj del último minuto
 *    (UTN_RaceRoundClockWidget), con aviso a los 60 y a los 30 s.
 *  - RoundResults: el recuento de la ronda (conchas en medias). Las de cada jugador se apuntan al empezar la ronda
 *    (mientras se prepara y los primeros segundos de carrera), así que las de esta ronda vuelan aunque RaceShellHalves
 *    llegue antes que la fase.
 *  - SprintIntro: el título del sprint final con las caras de las finalistas.
 *  - Champion: si la ronda ya tuvo su recuento (o viene del sprint), directamente la pantalla del campeón; si no (p. ej.
 *    TN.Race.Champion), primero el recuento con la concha que corona al campeón y luego el podio. En el podio suena para
 *    todos la música de victoria (UTN_MatchMusicSubsystem), pasado el momento en que el director de la música decide.
 *  - Waiting o Racing: nada más (se van con un fundido).
 *
 * Y la cáscara oscura del modo carrera (UTN_GhostHatchWidget::ShowCurtain, ZOrder 50; lo de encima en el 51):
 *  - Llegada al agua (Racing): en cuanto la tortuga propia entra en el agua de meta (o llega el puesto replicado,
 *    ATN_BeachRaceGameState::RoundArrivals), la cáscara se cierra desde arriba y desde abajo en 0,28 s; cerrada y con el
 *    puesto del servidor, «Has quedado X.º» con su premio y su mensaje (UTN_RaceArrivalWidget, 2,7 s) y se rompe dejando
 *    ver al fantasma espectador, o el recuento o el podio si ya han salido (con todas en el agua se espera tapada a que
 *    salga el recuento, que el servidor retrasa hasta ahí). Si el puesto no llega en 1,6 s (no era una llegada), se funde.
 *  - Entre ronda y ronda (Waiting viniendo del recuento, del título del sprint o del podio): se cierra sobre la pantalla
 *    que se va, «RONDA N» o «SPRINT FINAL» con su frase y «Colocando la playa…» (UTN_RaceRoundIntroWidget); con las
 *    tortugas ya en sus huevos, un «pum» de la cáscara por cada número del 3, 2, 1 (PhaseSecondsLeft) y, al dar la salida
 *    (Racing), se rompe: la tortuga ya está saltando de su huevo.
 *
 * Vista previa sin jugar, en cualquier mapa: TN.Race.Tally [ganador] [jugadores] [final] [medias], TN.Race.Podium
 * [jugadores], TN.Race.SprintPreview [jugadores], TN.Race.CountdownPreview, TN.Race.ClockPreview [segundos],
 * TN.Race.ArrivalPreview [puesto] [sprint], TN.Race.RoundPreview [ronda] [sprint] y TN.Race.PreviewOff
 * (Docs/Modo_Carrera.md).
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
	 * (-1 = nadie llega al agua), NumPlayers las columnas (1-8) y, con bChampionRound, la concha corona al ganador y
	 * después sale el podio.
	 */
	void StartTallyPreview(int32 WinnerRow, int32 NumPlayers, bool bChampionRound, int32 NumHalves = 1);

	/** Vista previa de la pantalla del campeón con el podio (tu tortuga primera y otras de mentira detrás). */
	void StartPodiumPreview(int32 NumPlayers);

	/** Vista previa del título del sprint final con tu tortuga y otras de mentira (2-8 finalistas). */
	void StartSprintPreview(int32 NumPlayers);

	/** Vista previa de la cuenta atrás de 10 s tras la primera en el agua (y su «¡TIEMPO!»). */
	void StartCountdownPreview();

	/**
	 * Vista previa del reloj del último minuto de la ronda desde StartSeconds (con sus avisos a los 60 y a los 30 s) y, al
	 * llegar a 0, el «¡TIEMPO!» de la ronda sin nadie en el agua.
	 */
	void StartClockPreview(float StartSeconds);

	/** Vista previa de la llegada al agua: la cáscara se cierra, «Has quedado Place.º» con su premio y se rompe. */
	void StartArrivalPreview(int32 Place, bool bSprint);

	/** Vista previa del paso entre rondas: la cáscara, «RONDA Round» (o «SPRINT FINAL»), 3, 2, 1 y se rompe. */
	void StartRoundIntroPreview(int32 Round, bool bSprint);

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
	/** Reloj del último minuto de la ronda (sin nadie en el agua todavía): lo pone, lo alimenta y lo quita. */
	void TickRoundClock(APlayerController* PC, const ATN_BeachRaceGameState& State);
	/** Pone (o reutiliza) el reloj del último minuto y le pasa lo que enseña. */
	void ShowRoundClock(APlayerController* PC, const FTNRaceRoundClockView& View);
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

	// Cáscara oscura del modo carrera: llegada al agua y paso entre rondas
	/** Mira si la tortuga propia ha llegado al agua (puesto replicado o ella en el agua de meta) y cierra la cáscara. */
	void TickArrival(float DeltaTime, APlayerController* PC, const ATN_BeachRaceGameState& State);
	/**
	 * La tortuga propia (visible, de este jugador, aún corriendo) está en el agua de meta de una ronda sin cerrar: su centro,
	 * como lo mira el servidor.
	 */
	bool IsLocalTurtleInFinishWater(const APlayerController* PC, const ATN_BeachRaceGameState& State);
	/** Cierra la cáscara de la llegada (ConfirmedPlace > 0 si el puesto ya ha llegado). */
	void StartArrival(APlayerController* PC, int32 ConfirmedPlace);
	/** Sigue la llegada: confirmación, «Has quedado X.º» con la cáscara cerrada y apertura. State null en la vista previa. */
	void AdvanceArrival(float DeltaTime, APlayerController* PC, const ATN_BeachRaceGameState* State);
	/** Cierra la cáscara del paso entre rondas con su título. */
	void StartRoundIntro(APlayerController* PC, const FTNRaceRoundIntroSetup& Setup);
	/** Sigue el paso entre rondas: título, 3, 2, 1 y se rompe al dar la salida. State null en la vista previa. */
	void AdvanceRoundIntro(float DeltaTime, const ATN_BeachRaceGameState* State);
	FTNRaceRoundIntroSetup BuildRoundIntro(const ATN_BeachRaceGameState& State) const;
	/** Abre la cáscara (rota con fiesta o fundida) y despide lo que va encima. */
	void OpenCurtain(bool bBurst);
	/** Quita la cáscara y lo de encima sin animación (el mundo se va, la vista previa se cierra). */
	void HideCurtain();

	UPROPERTY(Transient)
	TObjectPtr<UTN_GhostHatchWidget> Curtain;

	UPROPERTY(Transient)
	TObjectPtr<UTN_RaceArrivalWidget> ArrivalScreen;

	UPROPERTY(Transient)
	TObjectPtr<UTN_RaceRoundIntroWidget> RoundIntro;

	ETNRaceCurtainUse CurtainUse = ETNRaceCurtainUse::None;
	/** Segundos desde que se puso la cáscara. */
	float CurtainClock = 0.f;
	/** La llegada al agua de esta ronda ya se ha tratado (no se repite); se olvida al preparar otra ronda. */
	bool bArrivalHandled = false;
	/** El puesto de la llegada ya ha llegado del servidor (ArrivalPlace). */
	bool bArrivalConfirmed = false;
	int32 ArrivalPlace = 0;
	/** Vista previa de la llegada: si es del sprint final. */
	bool bPreviewSprint = false;
	/** El generador de la playa (para el agua de meta), buscado cada 2 s mientras no hay. */
	TWeakObjectPtr<ATN_BeachRaceGenerator> BeachGenerator;
	float NextGeneratorLookup = 0.f;
	/** Frase de ánimo de la ronda que se presenta (se sortea al cerrar la cáscara). */
	int32 IntroQuip = 0;
	/** Cuenta de salida entre rondas: lo que queda según esta máquina, el último valor replicado y el último número golpeado. */
	float IntroCountLeft = -1.f;
	float IntroLastReplicated = -1.f;
	int32 IntroShownNumber = 0;
	/**
	 * Se ha visto la preparación (PhaseSecondsLeft a 0 en Waiting): solo entonces cuenta el 3, 2, 1 (un resto del reloj del
	 * recuento que llegue tarde por red no da golpes).
	 */
	bool bIntroSawPrep = false;
	/** Premios de la pantalla del puesto ya dibujados de antemano. */
	int32 WarmPrize = 0;

	UPROPERTY(Transient)
	TObjectPtr<UTN_RaceTallyWidget> Tally;

	UPROPERTY(Transient)
	TObjectPtr<UTN_RaceChampionWidget> ChampionScreen;

	UPROPERTY(Transient)
	TObjectPtr<UTN_RaceFinishCountdownWidget> CountdownScreen;

	UPROPERTY(Transient)
	TObjectPtr<UTN_RaceSprintWidget> SprintScreen;

	UPROPERTY(Transient)
	TObjectPtr<UTN_RaceRoundClockWidget> RoundClock;

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
	/**
	 * Qué vista previa: 0 recuento o podio, 1 título del sprint, 2 cuenta atrás, 3 reloj del último minuto, 4 llegada al
	 * agua, 5 paso entre rondas.
	 */
	uint8 PreviewKind = 0;
	/** Segundos con los que empieza la vista previa del reloj del último minuto. */
	float PreviewClockStart = 62.f;
	bool bPreviewChampionAfter = false;
	float PreviewClock = 0.f;
	TArray<FTNRaceTallyRow> PreviewRows;
	int32 PreviewWinner = INDEX_NONE;
};
