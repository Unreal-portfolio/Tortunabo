#pragma once

#include "CoreMinimal.h"
#include "Game/TN_ProcMapGameState.h"
#include "TN_BeachRaceGameState.generated.h"

class APlayerState;

/** Fase de la partida de carrera en la playa (la lee el HUD de resultados y del campeón). */
UENUM(BlueprintType)
enum class ETNBeachRacePhase : uint8
{
	Waiting       UMETA(DisplayName = "Esperando"),
	Racing        UMETA(DisplayName = "Carrera"),
	RoundResults  UMETA(DisplayName = "Recuento de conchas"),
	Champion      UMETA(DisplayName = "Campeón y podio"),
	/** Empate en lo más alto: título «¡SPRINT FINAL!» a pantalla completa antes de la ronda corta de desempate. */
	SprintIntro   UMETA(DisplayName = "Sprint final")
};

/** Cuenta atrás tras la primera tortuga en el agua (ATN_BeachRaceGameMode). */
UENUM(BlueprintType)
enum class ETNBeachFinishCountdown : uint8
{
	/** Nadie ha llegado todavía (o no hay ronda). */
	None      UMETA(DisplayName = "Sin cuenta"),
	/** La primera ya está en el agua: quien llegue antes de FinishCountdownEndTime se lleva media concha. */
	Counting  UMETA(DisplayName = "Cuenta atrás"),
	/** Se acabó la cuenta: «¡TIEMPO!» y, enseguida, el recuento. */
	TimeUp    UMETA(DisplayName = "¡Tiempo!"),
	/** Han llegado todas antes de acabar la cuenta. */
	AllIn     UMETA(DisplayName = "Todas en el agua")
};

/**
 * Por qué se ha cerrado la ronda en curso (ATN_BeachRaceGameMode): lo enseñan el «¡TIEMPO!», su cinta y el recuento, para
 * que nunca parezca que alguien ha llegado al agua si no es así.
 */
UENUM(BlueprintType)
enum class ETNBeachRoundEnd : uint8
{
	/** La ronda sigue (o se prepara). */
	None            UMETA(DisplayName = "Sin cerrar"),
	/** La cuenta de 10 s tras la primera en el agua ha llegado a 0. */
	Countdown       UMETA(DisplayName = "Fin de la cuenta tras la primera"),
	/** Han llegado todas (o ya no queda nadie corriendo) antes del final de la cuenta. */
	AllIn           UMETA(DisplayName = "Todas en el agua"),
	/** Se ha acabado el tiempo de la ronda sin nadie en el agua: la concha es para la más cerca del mar. */
	TimeLimit       UMETA(DisplayName = "Tiempo de la ronda agotado"),
	/** Se ha acabado el tiempo del sprint final sin nadie en el agua: gana la finalista más cerca del mar. */
	SprintTimeLimit UMETA(DisplayName = "Tiempo del sprint agotado"),
	/** El sprint final se ha quedado sin rival (se han ido las demás finalistas). */
	Forfeit         UMETA(DisplayName = "Sprint sin rival")
};

/**
 * Una llegada al agua de meta en la ronda en curso, con el puesto que ha decidido el servidor en el instante del contacto
 * (ATN_BeachRaceGameMode::MarkPlayerFinished). La lee la pantalla del puesto de cada jugador («Has quedado X.º»,
 * UTN_RaceArrivalWidget).
 */
USTRUCT(BlueprintType)
struct FTNBeachRoundArrival
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Beach")
	TObjectPtr<APlayerState> Player = nullptr;

	/** Puesto en la ronda (1 = la primera en el agua). */
	UPROPERTY(BlueprintReadOnly, Category = "Beach")
	int32 Place = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBeachRacePhaseChanged);

/**
 * GameState del modo carrera en la playa (LVL_BeachRace, ATN_BeachRaceGameMode). Hereda el de rondas del mapa
 * procedural: ProcMode = Race, CurrentRound, RoundTarget (conchas para ser campeón: 3) y bRoundInProgress. Las conchas
 * de cada jugador van en medias (ATN_CoopPlayerState::RaceShellHalves; RoundWins cuenta las rondas ganadas enteras).
 * Añade la fase para el HUD: cuenta atrás tras la primera en el agua, recuento tras cada ronda (caras y tres conchas en
 * zigzag por jugador, con medias conchas), sprint final si hay empate en lo más alto y, al final, el campeón con su
 * podio.
 */
UCLASS()
class TORTUNABO_API ATN_BeachRaceGameState : public ATN_ProcMapGameState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RacePhase, Category = "Beach")
	ETNBeachRacePhase RacePhase = ETNBeachRacePhase::Waiting;

	/**
	 * La primera en tocar el agua en la ronda (se lleva una concha entera). Se pone al tocarla, así que ya vale durante
	 * la cuenta atrás; null si nadie llegó (o en la preparación).
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RacePhase, Category = "Beach")
	TObjectPtr<APlayerState> RoundWinner = nullptr;

	/** Las que llegaron durante la cuenta atrás, en orden de llegada (media concha cada una). */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RacePhase, Category = "Beach")
	TArray<TObjectPtr<APlayerState>> RoundHalfShells;

	/**
	 * Todas las llegadas al agua de meta de la ronda en curso, en orden y con su puesto (también la del sprint final). Solo
	 * de verdad: la concha del tiempo de ronda agotado (la más cerca del mar) no está aquí. Se vacía al preparar cada ronda.
	 * Sin aviso de cambio de fase: la pantalla del puesto lo mira cada fotograma.
	 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Beach")
	TArray<FTNBeachRoundArrival> RoundArrivals;

	/** Campeón de la partida (el primero en llegar a RoundTarget conchas, o quien gana el sprint final). */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RacePhase, Category = "Beach")
	TObjectPtr<APlayerState> Champion = nullptr;

	/** Podio al acabar la partida: primero, segundo y tercero (por conchas y, a igualdad, por la última ronda). */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RacePhase, Category = "Beach")
	TArray<TObjectPtr<APlayerState>> Podium;

	/** Segundos que quedan de la fase actual (recuento, título del sprint, cuenta de salida o podio); 0 = sin cuenta. */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Beach")
	float PhaseSecondsLeft = 0.f;

	/** Cuenta atrás tras la primera en el agua (en Racing). */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RacePhase, Category = "Beach")
	ETNBeachFinishCountdown FinishCountdown = ETNBeachFinishCountdown::None;

	/** Hora del servidor (GetServerWorldTimeSeconds) a la que se acaba la cuenta atrás. */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Beach")
	float FinishCountdownEndTime = 0.f;

	/** Duración de la cuenta atrás (s), para la interfaz. */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Beach")
	float FinishCountdownSeconds = 10.f;

	/**
	 * Hora del servidor (GetServerWorldTimeSeconds) a la que se acaba el tiempo de la ronda (o del sprint) si nadie llega al
	 * agua; 0 = sin límite, o ya no cuenta (alguien ha llegado y manda la cuenta de 10 s, o la ronda está cerrada).
	 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Beach")
	float RoundEndServerTime = 0.f;

	/** Duración del tiempo de la ronda en curso (s), para la interfaz. */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Beach")
	float RoundTimeLimitSeconds = 0.f;

	/** Por qué se ha cerrado la ronda en curso (None mientras se corre o se prepara). */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RacePhase, Category = "Beach")
	ETNBeachRoundEnd RoundEndReason = ETNBeachRoundEnd::None;

	/** La ronda en curso (o la que se prepara) es el sprint final de desempate. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RacePhase, Category = "Beach")
	bool bSprintFinal = false;

	/** Las finalistas del sprint (empatadas en lo más alto con RoundTarget conchas o más). */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RacePhase, Category = "Beach")
	TArray<TObjectPtr<APlayerState>> SprintFinalists;

	/** En todas las máquinas, al cambiar la fase, el ganador de la ronda, el campeón o el podio. */
	UPROPERTY(BlueprintAssignable, Category = "Beach")
	FOnBeachRacePhaseChanged OnRacePhaseChanged;

	/** Servidor: el OnRep no corre en el host, así que el GameMode avisa a mano tras cambiar los datos. */
	void NotifyRacePhaseChanged() { OnRacePhaseChanged.Broadcast(); }

	/** Segundos que quedan de la cuenta atrás tras la primera en el agua (0 si no está contando). En cualquier máquina. */
	float GetFinishCountdownLeft() const;

	/**
	 * Segundos que quedan del tiempo de la ronda (o del sprint) para que alguien llegue al agua; -1 si no hay límite o ya no
	 * cuenta (alguien ha llegado, o la ronda no está en marcha). En cualquier máquina.
	 */
	float GetRoundTimeLeft() const;

	/** Medias conchas de un jugador (0 si no es un ATN_CoopPlayerState). */
	static int32 GetShellHalves(const APlayerState* PlayerState);

	/** Puesto de PlayerState en la ronda en curso (RoundArrivals); 0 si no ha llegado al agua. En cualquier máquina. */
	int32 GetArrivalPlace(const APlayerState* PlayerState) const;

	/** Servidor: apunta que PlayerState ha llegado al agua en el puesto Place (una vez por ronda). */
	void AddRoundArrival(APlayerState* PlayerState, int32 Place);

protected:
	UFUNCTION()
	void OnRep_RacePhase() { OnRacePhaseChanged.Broadcast(); }
};
