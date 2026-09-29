// Lógica pura de las gaviotas de la carrera (TN_BeachGullTuning.h, ronda 4, tarea 5: el nerf): a qué velocidad sigue el
// blanco en cada tramo del ataque, si una tortuga que anda, corre o cambia de dirección se libra del picado, de la cagada de
// la zona y de la gaviota justiciera, y cuándo la plancha libra. Sin mundo ni actores: se simula el blanco con las mismas
// funciones que usan ATN_BeachGullZone y ATN_RaceGullStrike en el servidor, a 60 pasos por segundo.
// Correr desde Session Frontend (categoría "Tortunabo.Beach.Gull") o sin ventana:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Beach.Gull; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "World/Beach/TN_BeachGullTuning.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TNBeachGullTuningTest
{
	/** Velocidades de la tortuga (cm/s): andando y corriendo (UTN_StaminaComponent). */
	constexpr float TurtleWalk = 450.f;
	constexpr float TurtleRun = 800.f;

	/** Un tramo de la huida: desde Start s, en dirección Dir (unitaria) a Speed cm/s. */
	struct FLeg
	{
		float Start = 0.f;
		FVector2D Dir = FVector2D(1.0, 0.0);
		float Speed = 0.f;
	};

	/**
	 * Simula un ataque: la tortuga sale del origen y se mueve por tramos; el blanco nace sobre ella y la sigue con
	 * SpeedAt(T) hasta EndTime. Devuelve la distancia en planta entre el blanco y la tortuga al llegar el golpe.
	 */
	template <typename FSpeedAt>
	double SimulateMissDistance(const TArray<FLeg>& Legs, float EndTime, FSpeedAt SpeedAt)
	{
		constexpr float Dt = 1.f / 60.f;
		FVector2D Turtle = FVector2D::ZeroVector;
		FVector2D Aim = Turtle;
		for (float T = 0.f; T < EndTime - KINDA_SMALL_NUMBER; T += Dt)
		{
			const float Step = FMath::Min(Dt, EndTime - T);
			// La tortuga se mueve con el último tramo que ya ha empezado.
			const FLeg* Current = nullptr;
			for (const FLeg& Leg : Legs)
			{
				if (T >= Leg.Start)
				{
					Current = &Leg;
				}
			}
			if (Current)
			{
				Turtle += Current->Dir * (Current->Speed * Step);
			}
			// Como en el servidor: después de moverse ella, el blanco va hacia donde está.
			Aim = TNBeachGullTuning::StepToward(Aim, Turtle, SpeedAt(T), Step);
		}
		return FVector2D::Distance(Aim, Turtle);
	}

	double DiveMiss(const TArray<FLeg>& Legs)
	{
		using namespace TNBeachGullTuning;
		const float StrikeTime = DiveClimbTime + DiveTime;
		return SimulateMissDistance(Legs, StrikeTime, [StrikeTime](float T) { return DiveChaseSpeedAt(T, StrikeTime); });
	}

	double PoopMiss(const TArray<FLeg>& Legs)
	{
		using namespace TNBeachGullTuning;
		return SimulateMissDistance(Legs, PoopDropTime + PoopFallTime, [](float T) { return PoopChaseSpeedAt(T, PoopDropTime, PoopFallTime); });
	}

	double StrikeMiss(const TArray<FLeg>& Legs)
	{
		using namespace TNBeachGullTuning;
		return SimulateMissDistance(Legs, StrikeReleaseAge + StrikeFallSeconds,
			[](float T) { return StrikeChaseSpeedAt(T, StrikeReleaseAge, StrikeFallSeconds); });
	}

	/** Lo que coge el pico (tamaño 1) y lo que alcanza cada cagada. */
	bool DiveCatches(double Miss)
	{
		return TNBeachGullTuning::IsInsideHit(Miss, TNBeachGullTuning::GrabRadius, 1.f, TNBeachGullTuning::GrabPad);
	}

	bool PoopHits(double Miss)
	{
		return TNBeachGullTuning::IsInsideHit(Miss, TNBeachGullTuning::SplatRadius, 1.f, TNBeachGullTuning::SplatPad);
	}

	bool StrikeHits(double Miss)
	{
		return TNBeachGullTuning::IsInsideHit(Miss, TNBeachGullTuning::StrikeImpactRadius, 1.f, 0.f);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Tramos del blanco
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNBeachGullChasePhasesTest,
	"Tortunabo.Beach.Gull.ChasePhases",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNBeachGullChasePhasesTest::RunTest(const FString& Parameters)
{
	using namespace TNBeachGullTuning;
	using namespace TNBeachGullTuningTest;
	const float StrikeTime = DiveClimbTime + DiveTime;

	TestTrue(TEXT("Picado: al principio sigue más rápido de lo que se anda y más lento de lo que se corre"),
		DiveChaseSpeedAt(0.5f, StrikeTime) > TurtleWalk && DiveChaseSpeedAt(0.5f, StrikeTime) < TurtleRun);
	TestEqual(TEXT("Picado: en el último tramo ya va lanzado y apenas corrige"),
		DiveChaseSpeedAt(StrikeTime - DiveCommitSeconds * 0.5f, StrikeTime), DiveLateChaseSpeed);
	TestTrue(TEXT("Picado: lo que corrige al final es mucho menos que lo que se anda"), DiveLateChaseSpeed < TurtleWalk * 0.5f);
	TestEqual(TEXT("Picado: al llegar abajo el blanco ya no se mueve"), DiveChaseSpeedAt(StrikeTime + 0.01f, StrikeTime), 0.f);

	TestTrue(TEXT("Cagada: mientras vuela encima sigue más rápido de lo que se anda (andando no se escapa del aviso)"),
		PoopChaseSpeedAt(PoopDropTime * 0.5f, PoopDropTime, PoopFallTime) > TurtleWalk);
	TestTrue(TEXT("Cagada: mientras cae no sigue más rápido de lo que se anda"),
		PoopChaseSpeedAt(PoopDropTime + 0.2f, PoopDropTime, PoopFallTime) <= TurtleWalk);
	TestEqual(TEXT("Cagada: el último tramo de la caída el blanco está quieto"),
		PoopChaseSpeedAt(PoopDropTime + PoopFallTime - PoopLockSeconds * 0.5f, PoopDropTime, PoopFallTime), 0.f);

	TestTrue(TEXT("Justiciera: hasta soltarla sigue más lenta de lo que se corre"),
		StrikeChaseSpeedAt(1.f, StrikeReleaseAge, StrikeFallSeconds) < TurtleRun);
	TestEqual(TEXT("Justiciera: el último tramo de la caída el blanco está quieto"),
		StrikeChaseSpeedAt(StrikeReleaseAge + StrikeFallSeconds - StrikeLockSeconds * 0.5f, StrikeReleaseAge, StrikeFallSeconds), 0.f);

	TestTrue(TEXT("La velocidad del suavizado de los clientes cubre el tramo más rápido"),
		MaxChaseSpeed() >= DiveChaseSpeed && MaxChaseSpeed() >= PoopChaseSpeed);

	{
		const FVector2D Stepped = StepToward(FVector2D(0.0, 0.0), FVector2D(100.0, 0.0), 600.f, 0.1f);
		TestTrue(TEXT("El blanco no se pasa de su objetivo"), Stepped.Equals(FVector2D(60.0, 0.0), 0.01));
		const FVector2D Arrived = StepToward(FVector2D(0.0, 0.0), FVector2D(30.0, 40.0), 600.f, 0.1f);
		TestTrue(TEXT("Si llega en este paso, se queda justo encima"), Arrived.Equals(FVector2D(30.0, 40.0), 0.01));
		const FVector2D Frozen = StepToward(FVector2D(5.0, 5.0), FVector2D(300.0, 0.0), 0.f, 0.1f);
		TestTrue(TEXT("A velocidad 0 el blanco no se mueve"), Frozen.Equals(FVector2D(5.0, 5.0), 0.01));
	}
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Esquivar el picado
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNBeachGullDiveDodgeTest,
	"Tortunabo.Beach.Gull.DiveDodge",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNBeachGullDiveDodgeTest::RunTest(const FString& Parameters)
{
	using namespace TNBeachGullTuning;
	using namespace TNBeachGullTuningTest;
	const float StrikeTime = DiveClimbTime + DiveTime;
	const float Commit = StrikeTime - DiveCommitSeconds;
	const FVector2D Ahead(1.0, 0.0);
	const FVector2D Side(0.0, 1.0);

	TestTrue(TEXT("Quieta: la coge"), DiveCatches(DiveMiss({})));
	TestTrue(TEXT("Andando en línea recta todo el picado: la coge"), DiveCatches(DiveMiss({ { 0.f, Ahead, TurtleWalk } })));
	TestFalse(TEXT("Corriendo en línea recta desde que aparece la sombra: se libra"), DiveCatches(DiveMiss({ { 0.f, Ahead, TurtleRun } })));
	TestFalse(TEXT("Andando y, al lanzarse el picado, corriendo de lado: se libra"),
		DiveCatches(DiveMiss({ { 0.f, Ahead, TurtleWalk }, { Commit, Side, TurtleRun } })));
	TestFalse(TEXT("Andando y, al lanzarse el picado, corriendo hacia atrás: se libra"),
		DiveCatches(DiveMiss({ { 0.f, Ahead, TurtleWalk }, { Commit, -Ahead, TurtleRun } })));
	TestTrue(TEXT("Quieta y echando a correr demasiado tarde (0,1 s antes): la coge"),
		DiveCatches(DiveMiss({ { StrikeTime - 0.1f, Side, TurtleRun } })));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Esquivar la cagada (zona y justiciera)
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNBeachGullPoopDodgeTest,
	"Tortunabo.Beach.Gull.PoopDodge",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNBeachGullPoopDodgeTest::RunTest(const FString& Parameters)
{
	using namespace TNBeachGullTuning;
	using namespace TNBeachGullTuningTest;
	const FVector2D Ahead(1.0, 0.0);
	const FVector2D Side(0.0, 1.0);
	const float PoopLock = PoopDropTime + PoopFallTime - PoopLockSeconds;
	const float StrikeLock = StrikeReleaseAge + StrikeFallSeconds - StrikeLockSeconds;

	TestTrue(TEXT("Cagada: quieta, le da"), PoopHits(PoopMiss({})));
	TestTrue(TEXT("Cagada: andando en línea recta, le da"), PoopHits(PoopMiss({ { 0.f, Ahead, TurtleWalk } })));
	TestFalse(TEXT("Cagada: corriendo desde que la suelta, se libra"), PoopHits(PoopMiss({ { PoopDropTime, Ahead, TurtleRun } })));
	TestFalse(TEXT("Cagada: andando y, cuando el blanco se queda quieto, corriendo de lado, se libra"),
		PoopHits(PoopMiss({ { 0.f, Ahead, TurtleWalk }, { PoopLock, Side, TurtleRun } })));

	TestTrue(TEXT("Justiciera: quieta, le da"), StrikeHits(StrikeMiss({})));
	TestTrue(TEXT("Justiciera: andando en línea recta, le da"), StrikeHits(StrikeMiss({ { 0.f, Ahead, TurtleWalk } })));
	TestFalse(TEXT("Justiciera: corriendo desde que la suelta, se libra"), StrikeHits(StrikeMiss({ { StrikeReleaseAge, Ahead, TurtleRun } })));
	TestFalse(TEXT("Justiciera: andando y, cuando el blanco se queda quieto, corriendo de lado, se libra"),
		StrikeHits(StrikeMiss({ { 0.f, Ahead, TurtleWalk }, { StrikeLock, Side, TurtleRun } })));

	// La plancha: tirarse en el momento justo (en el aire o aún deprisa sobre la tripa), no tumbarse a esperar.
	TestTrue(TEXT("Plancha en el aire: libra"), DodgesByBellyDive(true, true, 0.f));
	TestTrue(TEXT("Plancha arrastrándose aún deprisa: libra"), DodgesByBellyDive(true, false, BellyDodgeMinSpeed + 10.f));
	TestFalse(TEXT("Tumbada casi parada sobre la tripa: no libra"), DodgesByBellyDive(true, false, BellyDodgeMinSpeed * 0.5f));
	TestFalse(TEXT("Sin plancha, aunque vaya por el aire (un salto): no libra"), DodgesByBellyDive(false, true, 900.f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
