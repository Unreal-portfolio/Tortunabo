// Carrera del Rally (Docs/Rally_MVP.md): opciones de URL, emparejado en buggies, fases, puertas en orden, vueltas, puestos a
// 5 Hz, contramano, reaparición, meta y resultados. Todo lo decide el servidor; el estado sale por ATN_RallyGameState.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Rally/TN_RallyLogic.h"
#include "Rally/TN_RallyGameState.h"
#include "TN_RallyGameMode.generated.h"

class ATN_RallyAIController;
class ATN_RallyTrack;
class APlayerController;

UCLASS()
class TORTUNABO_API ATN_RallyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATN_RallyGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override { return false; }
	virtual void Logout(AController* Exiting) override;

	/** Munición de una caja según el puesto del buggy (TNRally::AmmoWeightsForPlace). */
	bool RollAmmoFor(AActor* Vehicle, ETNRallyAmmo& OutAmmo, int32& OutCharges) const;

	UFUNCTION(BlueprintPure, Category = "Rally")
	FName GetVariant() const { return Variant; }

	/** Buggy (ATN_Buggy, de Vehicles/): clase blanda para no depender de ella al compilar el Rally. */
	UPROPERTY(EditDefaultsOnly, Category = "Rally")
	TSoftClassPtr<APawn> VehicleClass;

	UPROPERTY(EditDefaultsOnly, Category = "Rally")
	TSubclassOf<ATN_RallyAIController> AIControllerClass;

	UPROPERTY(EditDefaultsOnly, Category = "Rally")
	FName DefaultVariant = TEXT("I03R_tortuga_magna");

	/** Vueltas por defecto en circuito (?Laps= y luego el laps del manifest mandan); en punto a punto siempre 1. */
	UPROPERTY(EditDefaultsOnly, Category = "Rally", meta = (ClampMin = "1", ClampMax = "9"))
	int32 DefaultLaps = 2;

	/** Calentamiento tras la llegada de la última tortuga (s) y tope desde la primera. */
	UPROPERTY(EditDefaultsOnly, Category = "Rally|Tiempos")
	float WarmupSeconds = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Tiempos")
	float WarmupMaxSeconds = 20.f;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Tiempos")
	float CountdownSeconds = 3.f;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Tiempos")
	float EarlyStartPenaltySeconds = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Tiempos")
	float FinishGraceSeconds = 20.f;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Tiempos")
	float ResultsSeconds = 15.f;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Tiempos")
	float RespawnLockSeconds = 3.f;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Tiempos")
	float RespawnGhostSeconds = 2.f;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Tiempos")
	float TurnAroundGhostSeconds = 1.f;

	/** Periodo de los puestos y de las comprobaciones (contramano, atasco, fuera de pista): 5 Hz. */
	UPROPERTY(EditDefaultsOnly, Category = "Rally|Tiempos")
	float EvaluateInterval = 0.2f;

private:
	/** Estado del servidor de cada buggy (lo replicado va en FTNRallyStanding). */
	struct FTeamRuntime
	{
		int32 TeamIndex = INDEX_NONE;
		TWeakObjectPtr<APawn> Vehicle;
		bool bBot = false;
		bool bRetired = false;
		int32 GridSlot = 0;
		/** Hueco de la parrilla (con la cota del suelo), para la salida anticipada y la reaparición antes de la salida. */
		FTransform GridTransform;
		int32 GatesPassed = 0;
		int32 LastGate = INDEX_NONE;
		/** Recorrido real desde la última puerta validada (regla del 60 %). */
		double OdometerCm = 0.0;
		double Arc = 0.0;
		double SegmentProgressCm = 0.0;
		FVector PrevLocation = FVector::ZeroVector;
		bool bFinished = false;
		double FinishSeconds = 0.0;
		int32 Place = 0;
		bool bWrongWay = false;
		double RespawnEndTime = 0.0;
		/** Sin comprobaciones de reaparición hasta esta hora (tras reaparecer o girar). */
		double ImmuneUntil = 0.0;
		bool bEarlyPenalized = false;
		/** Salida anticipada: el motor sigue cortado hasta esta hora (1 s después del verde). */
		double EngineUnlockAt = 0.0;
		TNRally::FWrongWayState WrongWay;
		TNRally::FStuckState Stuck;
		TNRally::FOffTrackState OffTrack;
		/** Estadística de la carrera (línea [RallyStats] al dar los resultados): reapariciones por motivo y vuelcos. */
		int32 Respawns[4] = { 0, 0, 0, 0 };
		int32 Flips = 0;
		int32 TurnArounds = 0;
		bool bWasFlipped = false;
	};

	enum class ERespawnReason : uint8 { Hazard, OffTrack, Stuck, Request };

	ATN_RallyGameState* GetRallyGameState() const;
	double Now() const;
	TNRally::FLapRules MakeLapRules() const;

	void AssignPlayer(APlayerController* Player);
	int32 CreateTeam(bool bBot);
	int32 FindFreeGridSlot() const;
	void SpawnBots();
	FTeamRuntime* FindTeamByController(const AController* Controller);
	FTeamRuntime* FindTeamByVehicle(const AActor* Vehicle);
	const FTeamRuntime* FindTeamByVehicle(const AActor* Vehicle) const;
	void Spectate(APlayerController* Player);
	int32 CountSeatedHumans() const;
	void OnHumanSeated();

	void UpdatePhase();
	void StartCountdown();
	void StartRacing();
	void StartFinishing();
	void StartResults();
	void SetAllEnginesLocked(bool bLocked);
	bool IsRaceRunning() const;
	bool IsBeforeStart() const;
	/** Quita (antes de la salida) o retira los equipos sin buggy o con el buggy vacío (TNRally::DecideTeamCleanup). */
	void CleanupTeams();
	/** Torretas activas solo en Racing y Finishing y en equipos sin retirar (TNRally::AreWeaponsLive). */
	void ApplyWeaponLocks();

	void CheckEarlyStarts();
	void TickProgress();
	void HandleGateCrossing(FTeamRuntime& Team, int32 GateIndex, bool bForward, double Alpha, float DeltaSeconds);
	void CheckAmmoBoxes(FTeamRuntime& Team, const FVector& From, const FVector& To);
	void EvaluateTeams(double DeltaSeconds);
	void EvaluateTeam(FTeamRuntime& Team, double DeltaSeconds);
	bool IsInHazard(const FVector& Location) const;
	void RespawnTeam(FTeamRuntime& Team, ERespawnReason Reason);
	/** Peticiones de reaparición de las ocupantes (ITN_RallyVehicle::ConsumeRespawnRequest): se atienden en carrera. */
	void ConsumeRespawnRequests(bool bRacing);
	void TurnAround(FTeamRuntime& Team);
	void RebuildStandings();
	void RefreshSeats();
	/** Una línea [RallyStats] con terminados, reapariciones por motivo, vuelcos y tiempo del ganador (pruebas del piloto IA). */
	void LogRaceStats(bool bTimedOut) const;

	UPROPERTY(Transient)
	TObjectPtr<ATN_RallyTrack> Track;

	TArray<FTeamRuntime> Teams;
	TArray<TWeakObjectPtr<APlayerController>> PendingPlayers;

	FName Variant;
	int32 Seats = 2;
	int32 Bots = 0;
	int32 Laps = 2;
	/** ?Laps= en la URL manda sobre el laps del manifest. */
	bool bLapsFromUrl = false;
	int32 NextTeamIndex = 0;
	double FirstSeatTime = -1.0;
	double EvaluateAccumulator = 0.0;
	bool bTrackReady = false;
	bool bLoggedMissingVehicle = false;
	bool bRestartRequested = false;

	// Opciones de prueba (servidor sin jugadoras, carreras de la IA en bucle):
	/** ?AutoStart: el calentamiento empieza solo aunque no se siente ninguna jugadora (carrera solo de bots). */
	bool bAutoStart = false;
	/** ?RaceTimeout=S: la carrera pasa a resultados a los S s del verde (0 = sin tope). */
	float RaceTimeoutSeconds = 0.f;
	/** ?Races=N: tras los resultados de la carrera N del proceso, el juego se cierra en vez de empezar otra (0 = nunca). */
	int32 RaceLimit = 0;
};
