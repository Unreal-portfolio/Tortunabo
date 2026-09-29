#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Testing/TN_MonkeyPlan.h"
#include "TN_MonkeyComponent.generated.h"

class ATortugaCharacter;
class APlayerController;

/** Algo notable que ha visto el mono en una tortuga (atasco, caída bajo el terreno, tortuga perdida...). */
struct FTNMonkeyEvent
{
	FString Kind;
	float Time = 0.f;
	FVector Location = FVector::ZeroVector;
	FString Detail;
};

/** Lo que ha hecho y visto el mono con un jugador. */
struct FTNMonkeyPlayerStats
{
	int32 PlayerIndex = 0;
	int32 ActionCounts[static_cast<int32>(TNMonkey::EAction::Count)] = {};
	int32 PauseSkipped = 0;
	double DistanceCm = 0.0;
	double MinZ = TNumericLimits<double>::Max();
	double MaxDepthUnderGround = -TNumericLimits<double>::Max();
	float NoPawnSeconds = 0.f;
	int32 PawnChanges = 0;
	TArray<FTNMonkeyEvent> Events;
};

/**
 * Monkey test (Docs/Estres-Monkey-2026-09-29.md): cuelga del controlador de un jugador local (o bot) y le mete entrada
 * aleatoria reproducible por semilla a su tortuga: moverse, correr, saltar, doble salto (panzazo), caparazón, interactuar
 * (coger, lanzar y usar objeto), emotes y pausa. Además vigila la tortuga: atascos y caídas bajo el terreno. Lo crea y lo
 * dirige UTN_MonkeySubsystem (TN.Monkey); no hace nada por sí solo.
 */
UCLASS(NotBlueprintable)
class TORTUNABO_API UTN_MonkeyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTN_MonkeyComponent();

	/** Semilla de la sesión y número de este jugador (0 = el primero). */
	void Configure(int32 InSeed, int32 InPlayerIndex);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Suelta todo lo que estuviera pulsado (correr, interactuar, menú de pausa). */
	void ReleaseAll();

	const FTNMonkeyPlayerStats& GetStats() const { return Stats; }

private:
	void BeginStep(ATortugaCharacter* Turtle);
	void TickStep(ATortugaCharacter* Turtle, float DeltaTime);
	void EndStep(ATortugaCharacter* Turtle);
	void Watch(const ATortugaCharacter* Turtle, float DeltaTime);
	void AddEvent(const TCHAR* Kind, const FVector& Where, const FString& Detail);
	bool CanUsePauseMenu() const;

	int32 Seed = 0;
	TNMonkey::FPlanner Planner;
	TNMonkey::FStep Step;
	float StepElapsed = 0.f;
	bool bStepActive = false;
	bool bSecondJumpDone = false;
	bool bPauseOpened = false;
	bool bSprinting = false;

	FTNMonkeyPlayerStats Stats;
	TWeakObjectPtr<const ATortugaCharacter> LastPawn;
	TNMonkey::FStuckDetector Stuck;
	FVector LastLocation = FVector::ZeroVector;
	bool bHaveLastLocation = false;
	double StartZ = 0.0;
	bool bHaveStartZ = false;
	float DepthClock = 0.f;
	float DeepClock = 0.f;
	bool bUnderReported = false;
	bool bFallReported = false;
	bool bNoPawnReported = false;
	float WatchClock = 0.f;
	float Elapsed = 0.f;
};
