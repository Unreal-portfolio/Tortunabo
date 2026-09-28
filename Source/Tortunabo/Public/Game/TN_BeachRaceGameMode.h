#pragma once

#include "CoreMinimal.h"
#include "Game/TN_RunGameMode.h"
#include "Game/TN_BeachRaceGameState.h"
#include "TN_BeachRaceGameMode.generated.h"

class ACharacter;
class APlayerStart;
class ATN_BeachRaceGenerator;
class ATN_CoopPlayerState;
class ATortugaCharacter;

/** Qué elige el anfitrión en la pantalla del campeón. */
UENUM(BlueprintType)
enum class ETNBeachChampionChoice : uint8
{
	/** Otra partida de carrera en la misma playa, sin pasar por el lobby. */
	PlayAgain   UMETA(DisplayName = "Volver a jugar"),
	/** Vuelve al lobby con el otro modo ya elegido (Carrera → Cooperativo). */
	ChangeMode  UMETA(DisplayName = "Cambiar de modo"),
	/** Al menú principal (el anfitrión cierra la partida para todos; un cliente sale solo). */
	Quit        UMETA(DisplayName = "Salir")
};

/** Sitio seguro apuntado de una tortuga en la playa (en el suelo, fuera de zonas de muerte), para volver tras caer. */
struct FTNBeachSafeSpot
{
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	float Time = 0.f;
};

/** Una llegada al agua de meta en la ronda en curso. */
struct FTNBeachArrival
{
	TWeakObjectPtr<APlayerController> Controller;
	TWeakObjectPtr<APlayerState> State;
	FString Name;
	/** Medias conchas que se lleva: 2 la primera, 1 las de la cuenta atrás. */
	int32 Halves = 0;
	/** Hasta cuándo (hora del mundo) se la ve en el agua con su chapuzón antes de pasar a espectadora. */
	float HoldEnd = 0.f;
	/** Ya ha pasado por la meta de la base (puesto, puntos, oculta y espectadora). */
	bool bSettled = false;
};

/**
 * @brief GameMode del modo carrera en la playa (LVL_BeachRace; Docs/Modo_Carrera.md, sección «Flujo de la carrera»).
 *
 * Hereda de ATN_RunGameMode (no del GameMode del mapa procedural, cuyo bucle de rondas es privado y va atado a
 * ATN_ProcMapGenerator): de la base reutiliza la espera a los jugadores tras el viaje, la llegada a la meta
 * (MarkPlayerFinished: puesto, puntos, espectador), los resultados, la vuelta al lobby (LobbyReturnMapPath) y el flujo
 * replicado (MatchFlowState, CountdownValue) del que tiran la pantalla de carga del huevo («¡ADELANTE!») y la música de
 * fin de partida. El bucle de rondas es el de la carrera del mapa procedural, adaptado a la playa:
 *
 *  1. Preparación (RacePhase = Waiting, MatchFlowState = WaitingForPlayers): el generador reparte los elementos de la
 *     ronda (el terreno es fijo), las tortugas aparecen en la salida (escalonada, ATN_BeachRaceGenerator::
 *     GetStartTransform; los sitios rotan cada ronda) y se quedan quietas. En la primera ronda tras el viaje la cuenta
 *     atrás es el huevo de la pantalla de carga; en las demás, PreRaceCountdownSeconds (CountdownValue y
 *     PhaseSecondsLeft: 3, 2, 1).
 *  2. Carrera (Racing, InProgress): la primera tortuga que toca el agua de meta tras saltar el acantilado gana la ronda y
 *     una concha entera (el GameMode mira ATN_BeachRaceGenerator::IsFinishWater diez veces por segundo; también vale
 *     que algo llame a MarkPlayerFinished). Desde ahí, cuenta atrás de FinishCountdownSeconds (10 s) para todas: quien
 *     llegue dentro se lleva media concha. Al acabar, a cada una que no ha llegado se la come un gusano de arena
 *     (ATN_BeachSandWorm) y, al acabar el bocado (o enseguida si ya han llegado todas), el recuento. Las conchas
 *     van en medias (ATN_CoopPlayerState::RaceShellHalves). Quien llega se queda a la vista en el agua con su chapuzón
 *     FinishSplashHoldSeconds y luego pasa a espectadora por la meta de la base. Arranca la tormenta de bañistas
 *     (ATN_BeachStorm, por nombre) y la para al acabar. Aquí no se muere: MarkPlayerDead aturde (TNBeach::StunTurtle) y,
 *     fuera del mapa, en una zona de muerte o en el vacío, devuelve a la tortuga a un sitio seguro cercano y la aturde.
 *     El salto del acantilado no hace bola ni aturde (se cae de cabeza al agua).
 *  3. Recuento (RoundResults, Countdown): RoundResultsSeconds con la ganadora (RoundWinner) y las medias
 *     (RoundHalfShells). Después, otra ronda o, si alguien ha llegado a WinsToWinMatch conchas, el campeón; si en lo más
 *     alto hay empate con WinsToWinMatch o más, el sprint final.
 *  4. Sprint final (SprintIntro y luego Waiting/Racing con bSprintFinal): título «¡SPRINT FINAL!»; después solo las
 *     empatadas corren, desde el nido de huevos llevado a la línea del sprint a mitad del recorrido (SetStartEggsAtSprint;
 *     reaparecen dentro con RestartPlayerAtTransform en GetSprintStartTransform y OpenStartEggs las lanza al dar la
 *     salida); la primera en el agua es campeona (sin cuenta de 10 s ni gusanos; en el límite, la más cerca del mar).
 *     Las demás, espectadoras.
 *  5. Campeón (Champion, Results): Champion y Podium rellenos; se queda así hasta que el anfitrión elige con
 *     RequestChampionChoice (Volver a jugar, Cambiar de modo o Salir).
 *
 * Pruebas: opciones de URL ?BeachSeed=N ?BeachWins=N y la consola TN.Race.* (WinRound, Champion, Sprint, Stun, Kill,
 * Void, PlayAgain, ChangeMode, Menu) y TN.Mode (ver Docs/Modo_Carrera.md).
 */
UCLASS()
class TORTUNABO_API ATN_BeachRaceGameMode : public ATN_RunGameMode
{
	GENERATED_BODY()

public:
	ATN_BeachRaceGameMode();

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void MarkPlayerFinished(APlayerController* PlayerController) override;

	/** En la playa no se muere: aturde y, si hace falta, devuelve a la tortuga a un sitio seguro. */
	virtual void MarkPlayerDead(APlayerController* PlayerController) override;

	// ── Pantalla del campeón (las llama la interfaz; solo el anfitrión) ──────────────────────────────────────────

	/**
	 * Lo que pide la interfaz del jugador local en la pantalla del campeón. En el anfitrión (servidor escucha) aplica
	 * la elección para todos y devuelve true; en un cliente solo vale Quit (sale él solo al menú) y el resto devuelve
	 * false (lo decide el anfitrión). Sin RPC: en servidor escucha la interfaz del anfitrión corre en el servidor.
	 */
	UFUNCTION(BlueprintCallable, Category = "Beach|Champion", meta = (WorldContext = "WorldContextObject"))
	static bool RequestChampionChoice(const UObject* WorldContextObject, ETNBeachChampionChoice Choice);

	/** true si el jugador local puede elegir en la pantalla del campeón (es el anfitrión y la partida ha acabado). */
	UFUNCTION(BlueprintPure, Category = "Beach|Champion", meta = (WorldContext = "WorldContextObject"))
	static bool CanLocalPlayerChoose(const UObject* WorldContextObject);

	/** Servidor: otra partida de carrera en la misma playa (conchas a cero, ronda 1, sin viajar). */
	UFUNCTION(BlueprintCallable, Category = "Beach|Champion")
	void PlayAgain();

	/** Servidor: vuelve al lobby con el otro modo elegido en la GameInstance (Carrera ↔ Cooperativo). */
	UFUNCTION(BlueprintCallable, Category = "Beach|Champion")
	void ChangeModeAndReturnToLobby();

	/** Servidor: el anfitrión vuelve al menú principal y la partida se cierra para todos. */
	UFUNCTION(BlueprintCallable, Category = "Beach|Champion")
	void QuitToMainMenu();

	UFUNCTION(BlueprintPure, Category = "Beach")
	ETNBeachRacePhase GetRacePhase() const;

	UFUNCTION(BlueprintPure, Category = "Beach")
	int32 GetCurrentRound() const { return CurrentRound; }

	UFUNCTION(BlueprintPure, Category = "Beach")
	ATN_BeachRaceGenerator* GetGenerator() const;

	// ── Pruebas (consola TN.Race.*) ──────────────────────────────────────────────────────────────────────────────

	/**
	 * El jugador PlayerIndex (orden de PlayerArray) toca el agua como si llegara: la primera gana la ronda y arranca la
	 * cuenta atrás; las siguientes, media concha.
	 */
	void DebugWinRound(int32 PlayerIndex);

	/** El jugador PlayerIndex pasa a tener las conchas del campeón y se salta directamente a su pantalla. */
	void DebugChampion(int32 PlayerIndex);

	/**
	 * Empate forzado: los jugadores PlayerIndices pasan a WinsToWinMatch conchas (las demás, por debajo) y empieza el
	 * sprint final entre ellas (con una sola también, para probarlo).
	 */
	void DebugSprint(const TArray<int32>& PlayerIndices);

	/** Pasa por MarkPlayerDead (lo que en el cooperativo mataría). */
	void DebugKill(int32 PlayerIndex);

	/** Lleva a la tortuga muy por debajo del vacío para probar la vuelta a un sitio seguro. */
	void DebugVoid(int32 PlayerIndex);

	/** Aturde a la tortuga Seconds segundos. */
	void DebugStun(int32 PlayerIndex, float Seconds);

protected:
	virtual void OnWaitingTimeout() override;
	virtual void UpdateRoundProgressAndMaybeFinish() override;

	/** Clase del generador que se crea si el nivel no tiene uno colocado. */
	UPROPERTY(EditDefaultsOnly, Category = "Beach")
	TSubclassOf<ATN_BeachRaceGenerator> GeneratorClass;

	/** Conchas enteras para ser campeón (en medias, el doble). */
	UPROPERTY(EditDefaultsOnly, Category = "Beach|Rounds", meta = (ClampMin = "1"))
	int32 WinsToWinMatch = 3;

	/** Semilla fija del reparto para repetir rondas (0 = aleatoria). Cada ronda suma 1. */
	UPROPERTY(EditDefaultsOnly, Category = "Beach|Rounds")
	int32 FixedSeed = 0;

	/** Espera mínima de la preparación (elementos replicados y tortugas colocadas en la salida). */
	UPROPERTY(EditDefaultsOnly, Category = "Beach|Rounds", meta = (ClampMin = "0.0"))
	float MinPreRoundSeconds = 2.f;

	/** Si el generador no dice que la ronda está lista en este tiempo, se corre igual. */
	UPROPERTY(EditDefaultsOnly, Category = "Beach|Rounds", meta = (ClampMin = "1.0"))
	float RoundReadyTimeoutSeconds = 20.f;

	/** Cuenta atrás en la salida antes de cada ronda que no abre el huevo de la pantalla de carga. 0 = sin cuenta. */
	UPROPERTY(EditDefaultsOnly, Category = "Beach|Rounds", meta = (ClampMin = "0.0"))
	float PreRaceCountdownSeconds = 3.f;

	/** Segundos del recuento de conchas tras cada ronda. */
	UPROPERTY(EditDefaultsOnly, Category = "Beach|Rounds", meta = (ClampMin = "1.0"))
	float RoundResultsSeconds = 7.f;

	/** Límite de cada ronda (unos 5 min de media): al agotarse gana quien esté más cerca del mar. 0 = sin límite. */
	UPROPERTY(EditDefaultsOnly, Category = "Beach|Rounds", meta = (ClampMin = "0.0"))
	float RoundTimeLimitSeconds = 540.f;

	/**
	 * Segundos que cada tortuga que llega se queda a la vista dentro del agua de meta, con su chapuzón, antes de que la
	 * base la oculte y la pase a espectadora. El puesto se decide en el instante del contacto.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Beach|Rounds", meta = (ClampMin = "0.0", ClampMax = "1.5"))
	float FinishSplashHoldSeconds = 0.8f;

	/** Cuenta atrás para todas tras la primera en el agua: quien llegue dentro se lleva media concha. */
	UPROPERTY(EditDefaultsOnly, Category = "Beach|Rounds", meta = (ClampMin = "1.0"))
	float FinishCountdownSeconds = 10.f;

	/** Segundos con «¡TIEMPO!» en pantalla (las tortugas quietas) antes del recuento. */
	UPROPERTY(EditDefaultsOnly, Category = "Beach|Rounds", meta = (ClampMin = "0.2"))
	float TimeUpHoldSeconds = 1.6f;

	/**
	 * Margen tras el bocado de los gusanos de arena (ATN_BeachSandWorm::EatSeconds) antes del recuento. Solo si al acabar
	 * la cuenta quedaba alguna sin llegar; si no, TimeUpHoldSeconds.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Beach|Rounds", meta = (ClampMin = "0.0"))
	float SandWormMarginSeconds = 0.6f;

	/** Segundos del título «¡SPRINT FINAL!» antes de preparar el sprint. */
	UPROPERTY(EditDefaultsOnly, Category = "Beach|Sprint", meta = (ClampMin = "1.0"))
	float SprintIntroSeconds = 5.f;

	/** Límite del sprint (media playa): al agotarse gana la más cerca del mar. 0 = sin límite. */
	UPROPERTY(EditDefaultsOnly, Category = "Beach|Sprint", meta = (ClampMin = "0.0"))
	float SprintTimeLimitSeconds = 300.f;

	/** Margen (cm) alrededor del nido del sprint en el que se quitan los elementos de la ronda (donde caen al salir). */
	UPROPERTY(EditDefaultsOnly, Category = "Beach|Sprint", meta = (ClampMin = "0.0"))
	float SprintClearMargin = 1500.f;

	/** Segundos entre la elección del campeón y el viaje (el huevo se cierra y la música se funde). */
	UPROPERTY(EditDefaultsOnly, Category = "Beach|Champion", meta = (ClampMin = "0.1"))
	float ChampionLeaveDelaySeconds = 1.2f;

	/** Aturdimiento de lo que en el cooperativo mataría (zonas de muerte, caídas, enemigos, tormenta). */
	UPROPERTY(EditDefaultsOnly, Category = "Beach|Stun", meta = (ClampMin = "0.1"))
	float DeathStunSeconds = 3.f;

	/** Aturdimiento al volver a un sitio seguro tras caer al vacío o a una zona de muerte. */
	UPROPERTY(EditDefaultsOnly, Category = "Beach|Stun", meta = (ClampMin = "0.1"))
	float RescueStunSeconds = 2.5f;

	/**
	 * Vacío: por debajo de (el suelo más bajo pisado en la ronda, o la salida) menos esto (cm), la tortuga vuelve a un
	 * sitio seguro. Holgado para que el salto del acantilado al agua de meta nunca cuente como vacío.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Beach|Stun", meta = (ClampMin = "500.0"))
	float VoidDepth = 15000.f;

	/** Cada cuánto (s) se apunta el último sitio seguro de cada tortuga (en el suelo, fuera de zonas de muerte). */
	UPROPERTY(EditDefaultsOnly, Category = "Beach|Stun", meta = (ClampMin = "0.1"))
	float SafeSpotSampleSeconds = 0.5f;

	/** Antigüedad mínima (s) del sitio seguro al que se vuelve (para no reaparecer justo en el borde). */
	UPROPERTY(EditDefaultsOnly, Category = "Beach|Stun", meta = (ClampMin = "0.0"))
	float SafeSpotMinAgeSeconds = 1.f;

	/** Nombre de la clase de la tormenta de bañistas (en /Script/Tortunabo); si no existe, no hay tormenta. */
	UPROPERTY(EditDefaultsOnly, Category = "Beach|Storm")
	FString StormClassName = TEXT("TN_BeachStorm");

	/** Distancia (cm) detrás de la salida a la que aparece la tormenta. */
	UPROPERTY(EditDefaultsOnly, Category = "Beach|Storm", meta = (ClampMin = "0.0"))
	float StormSpawnBehind = 3000.f;

private:
	UPROPERTY(Transient)
	TObjectPtr<ATN_BeachRaceGenerator> Generator;

	UPROPERTY(Transient)
	TObjectPtr<AActor> Storm;

	/** PlayerStart de cada sitio de la salida (creados en ejecución al primer uso). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<APlayerStart>> StartPoints;

	UClass* StormClass = nullptr;
	bool bStormClassResolved = false;

	int32 CurrentRound = 0;
	int32 RoundSeed = 0;
	int32 UrlSeed = 0;
	bool bPlayersArrived = false;
	bool bPreparingRound = false;
	/** Se puede llegar a la meta (carrera y cuenta atrás tras la primera). */
	bool bRoundActive = false;
	bool bMatchOver = false;
	bool bSuppressRoundCheck = false;
	bool bLeaving = false;
	/** La próxima preparación enseña la cuenta atrás (no la primera tras el viaje, que la tapa el huevo). */
	bool bShowPreRaceCountdown = false;
	/** Se acabó la cuenta atrás (o llegaron todas): «¡TIEMPO!» en pantalla y todas quietas hasta el recuento. */
	bool bTimeUp = false;
	/** La ronda en curso (o la que se prepara) es el sprint final de desempate. */
	bool bSprint = false;
	float PrepStartTime = 0.f;
	float PhaseEndTime = 0.f;
	float NextSafeSampleTime = 0.f;
	/** Suelo más bajo pisado en la ronda (o la salida): referencia del vacío. */
	double LowestGroundZ = 0.0;
	/** Salida de la ronda (en el sprint, el centro de su nido) y hacia dónde está el mar: tormenta y progreso. */
	FVector CourseOrigin = FVector::ZeroVector;
	FVector CourseForward = FVector::ForwardVector;

	/** Llegadas al agua de meta en la ronda en curso, en orden. */
	TArray<FTNBeachArrival> Arrivals;

	/** Finalistas del sprint (su orden es el de sus huevos). */
	TArray<TWeakObjectPtr<APlayerController>> SprintFinalists;

	/** Última ronda que ganó cada jugador (PlayerId → ronda), para desempatar el podio. */
	TMap<int32, int32> LastRoundWonByPlayer;

	/** Últimos sitios seguros de cada tortuga (el más nuevo al final). */
	TMap<TWeakObjectPtr<APlayerController>, TArray<FTNBeachSafeSpot>> SafeSpots;

	/** Jugadores con el movimiento bloqueado, con el peón que tenían (si cambia, se vuelve a avisar al cliente). */
	TMap<TWeakObjectPtr<APlayerController>, TWeakObjectPtr<APawn>> FrozenControllers;

	FTimerHandle PrepPollHandle;
	FTimerHandle PhaseClockHandle;
	FTimerHandle PhaseEndHandle;
	FTimerHandle WatchHandle;
	FTimerHandle RoundTimeLimitHandle;
	FTimerHandle FinishCountdownHandle;
	FTimerHandle TimeUpHandle;
	FTimerHandle SprintWinHandle;
	FTimerHandle LeaveHandle;

	ATN_BeachRaceGameState* GetBeachGameState() const;
	void ResolveUrlOptions();
	void EnsureGenerator();

	// Ronda
	/** Reparte la ronda CurrentRound y espera a que esté lista; con bCleanup, antes quita tortugas y tormenta. */
	void PrepareRound(bool bCleanup);
	void PollRoundReady();
	void BeginRace();
	void WatchRacers();
	void OnRoundTimeLimit();

	// Llegadas y cuenta atrás tras la primera
	bool HasArrived(const AController* Controller) const;
	/** true si ya no queda nadie corriendo (todas han llegado, se han ido o miran); Ignore, la que se está yendo. */
	bool AreAllRacersIn(const AController* Ignore = nullptr) const;
	/** Pasa por la meta de la base a quien ya ha tenido su margen en el agua (o a todas, con bAll). */
	void SettleArrivals(bool bAll);
	void OnFinishCountdownEnd();
	/**
	 * Se acaba la cuenta (o han llegado todas): «¡TIEMPO!», todas quietas y, en TimeUpHoldSeconds, el recuento. Con
	 * bSandWorms (la cuenta ha llegado a 0), a las que no han llegado se las comen los gusanos de arena y el recuento espera
	 * a que acabe el bocado.
	 */
	void FinishTimeUp(bool bAllIn, bool bSandWorms = false);
	/** Un gusano de arena para cada tortuga que aún corría (fuera antes del caparazón, el mareo y la carga); cuántas. */
	int32 FeedSandWorms();
	void CloseRoundAfterTimeUp();
	/** Recuento: reparte las conchas de las llegadas (entera la primera, medias las demás) y pasa a RoundResults. */
	void EndRound(const FString& ResultText);
	void AfterRoundResults();
	void StartNextRound();
	void EnterChampion(ATN_CoopPlayerState* ChampionState);
	void ResetMatchScores();
	void ResetRoundPlayerStates();
	void ResetRoundGameState() const;
	void CancelRoundTimers();
	void CleanupRoundActors();
	void LeaveAfterDelay(TFunction<void()> Action);

	// Sprint final
	void EnterSprintIntro(const TArray<ATN_CoopPlayerState*>& Finalists);
	void StartSprint();
	/** Despeja la línea del sprint y pone a cada finalista dentro de su huevo del nido; false si ya no queda ninguna. */
	bool PlaceSprintFinalists();
	/** Tortuga nueva para la finalista Index, dentro de su huevo (RestartPlayerAtTransform en GetSprintStartTransform). */
	void PlaceSprintFinalist(APlayerController* PlayerController, int32 Index);
	void CompleteSprintWin();
	/** Si se van finalistas: con una sola, gana; sin ninguna, la de más conchas. */
	void CheckSprintForfeit();
	bool IsSprintFinalist(const AController* Controller) const;
	/** Fuera del sprint: sin tortuga y a espectadora por la vía normal (MovePlayerToSpectator). */
	void SendOutOfSprint(APlayerController* PlayerController);

	// Fases y estado replicado
	void SetRacePhase(ETNBeachRacePhase NewPhase) const;
	void BeginPhaseClock(float Seconds);
	void StopPhaseClock();
	void TickPhaseClock();
	void SyncGameState() const;

	// Tortugas
	int32 GetStartSlot(const AController* Controller) const;
	FTransform GetStartTransformFor(int32 Slot) const;
	/** Transformada del generador con la cápsula apoyada en el suelo que haya debajo (o tal cual si no hay suelo). */
	FTransform PutOnFloor(const FTransform& Transform, float HalfHeight) const;
	float GetDefaultHalfHeight() const;
	void PlacePlayersAtStart();
	void RespawnControllerFresh(APlayerController* PlayerController);
	void FreezePlayers();
	void UnfreezePlayers();
	void ReleaseCarry(ATortugaCharacter* Turtle) const;
	/** Saca a la tortuga del caparazón, del derribo y de lo que lleve y la pone en Transform. */
	void TeleportTurtle(ATortugaCharacter* Turtle, const FTransform& Transform) const;
	/** Vuelve a un sitio seguro cercano (o a la salida) y queda aturdida. */
	void RescueTurtle(APlayerController* PlayerController, const TCHAR* Reason);
	FTransform FindSafeTransform(APlayerController* PlayerController);
	void SampleSafeSpot(APlayerController* PlayerController, const ACharacter* Character, float Now);
	bool IsInsideHazard(const APawn* Pawn) const;
	/**
	 * Salto del acantilado de meta: si la tortuga cae dentro de la zona del salto (ATN_BeachRaceGenerator::
	 * IsCliffJumpZone), esa caída no la mete sola en el caparazón ni la aturde al aterrizar
	 * (ATortugaCharacter::SetFallImmuneUntilLanded): entra de cabeza al agua. El resto de caídas de la playa, igual.
	 */
	void GuardCliffJump(APawn* Pawn) const;
	double GetVoidZ() const;
	float GetCourseProgress(const APawn* Pawn) const;

	// Tormenta
	void StartStorm();
	void StopStorm(bool bDestroy);

	APlayerController* GetControllerByIndex(int32 PlayerIndex) const;
	static bool CallNoParamFunction(UObject* Target, FName FunctionName);
	static int32 HalvesOf(const APlayerState* PlayerState);
};
