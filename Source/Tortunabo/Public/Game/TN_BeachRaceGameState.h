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

	/** Medias conchas de un jugador (0 si no es un ATN_CoopPlayerState). */
	static int32 GetShellHalves(const APlayerState* PlayerState);

protected:
	UFUNCTION()
	void OnRep_RacePhase() { OnRacePhaseChanged.Broadcast(); }
};
