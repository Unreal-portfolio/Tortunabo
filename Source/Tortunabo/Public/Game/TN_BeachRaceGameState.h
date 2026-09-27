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
	Champion      UMETA(DisplayName = "Campeón y podio")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBeachRacePhaseChanged);

/**
 * GameState del modo carrera en la playa (LVL_BeachRace, ATN_BeachRaceGameMode). Hereda el de rondas del mapa
 * procedural: ProcMode = Race, CurrentRound, RoundTarget (conchas para ser campeón: 3) y bRoundInProgress. Las conchas
 * de cada jugador son sus victorias de ronda (ATN_CoopPlayerState::RoundWins). Añade la fase para el HUD: recuento tras
 * cada ronda (caras y tres conchas en zigzag por jugador) y, al llegar alguien a tres, el campeón con su podio.
 */
UCLASS()
class TORTUNABO_API ATN_BeachRaceGameState : public ATN_ProcMapGameState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RacePhase, Category = "Beach")
	ETNBeachRacePhase RacePhase = ETNBeachRacePhase::Waiting;

	/** Quien ganó la última ronda (se lleva una concha); null si nadie llegó. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RacePhase, Category = "Beach")
	TObjectPtr<APlayerState> RoundWinner = nullptr;

	/** Campeón de la partida (el primero en llegar a RoundTarget conchas); null mientras nadie llegue. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RacePhase, Category = "Beach")
	TObjectPtr<APlayerState> Champion = nullptr;

	/** Podio al acabar la partida: primero, segundo y tercero (por conchas y, a igualdad, por la última ronda). */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RacePhase, Category = "Beach")
	TArray<TObjectPtr<APlayerState>> Podium;

	/** Segundos que quedan de la fase actual (recuento o podio); 0 = sin cuenta. */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Beach")
	float PhaseSecondsLeft = 0.f;

	/** En todas las máquinas, al cambiar la fase, el ganador de la ronda, el campeón o el podio. */
	UPROPERTY(BlueprintAssignable, Category = "Beach")
	FOnBeachRacePhaseChanged OnRacePhaseChanged;

	/** Servidor: el OnRep no corre en el host, así que el GameMode avisa a mano tras cambiar los datos. */
	void NotifyRacePhaseChanged() { OnRacePhaseChanged.Broadcast(); }

protected:
	UFUNCTION()
	void OnRep_RacePhase() { OnRacePhaseChanged.Broadcast(); }
};
