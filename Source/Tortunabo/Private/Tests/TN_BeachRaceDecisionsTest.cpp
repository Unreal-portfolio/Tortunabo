// Reglas de la carrera en la playa (TN_BeachRaceDecisions.h), las mismas que usa ATN_BeachRaceGameMode. Correr desde
// Session Frontend (categoría "Tortunabo.BeachRace") o headless:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.BeachRace; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "Game/TN_BeachRaceDecisions.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TNBeachRaceTestHelpers
{
	static TNBeachRaceRules::FTimeLimitCandidate Candidate(bool bEligible, bool bHasPawn, float Progress)
	{
		TNBeachRaceRules::FTimeLimitCandidate Result;
		Result.bEligible = bEligible;
		Result.bHasPawn = bHasPawn;
		Result.Progress = Progress;
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNBeachRaceTimeLimitWinnerTest,
	"Tortunabo.BeachRace.TimeLimitWinner",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNBeachRaceTimeLimitWinnerTest::RunTest(const FString& Parameters)
{
	using namespace TNBeachRaceRules;
	using namespace TNBeachRaceTestHelpers;

	{
		// Lo de siempre: gana la elegible con tortuga más cerca del mar, en ronda normal y en el sprint.
		const TArray<FTimeLimitCandidate> Candidates = { Candidate(true, true, 0.4f), Candidate(true, true, 0.7f), Candidate(false, true, 0.9f) };
		TestEqual(TEXT("Ronda normal: la más cerca del mar (la no elegible no cuenta)"), PickTimeLimitWinner(Candidates, false), 1);
		TestEqual(TEXT("Sprint: la finalista más cerca del mar"), PickTimeLimitWinner(Candidates, true), 1);
	}
	{
		// #56: sprint final con las dos finalistas sin tortuga (caídas bajo KillZ) en el tick del límite. Antes no ganaba nadie
		// y la partida se quedaba parada; ahora gana la primera finalista que quede y la ronda termina.
		const TArray<FTimeLimitCandidate> Candidates = { Candidate(false, true, 0.8f), Candidate(true, false, 0.f), Candidate(true, false, 0.f) };
		TestEqual(TEXT("Sprint sin tortugas: gana la primera finalista"), PickTimeLimitWinner(Candidates, true), 1);
		TestEqual(TEXT("Ronda normal sin tortugas: sin ganadora, como siempre"), PickTimeLimitWinner(Candidates, false), static_cast<int32>(INDEX_NONE));
	}
	{
		// Una finalista con tortuga gana a la que no la tiene, aunque esta vaya primero en la lista.
		const TArray<FTimeLimitCandidate> Candidates = { Candidate(true, false, 0.f), Candidate(true, true, 0.1f) };
		TestEqual(TEXT("Sprint: la finalista con tortuga antes que la que espera su huevo"), PickTimeLimitWinner(Candidates, true), 1);
	}
	{
		const TArray<FTimeLimitCandidate> Candidates = { Candidate(false, false, 0.f), Candidate(false, true, 0.5f) };
		TestEqual(TEXT("Sin nadie elegible: sin ganadora (lo decide CheckSprintForfeit)"), PickTimeLimitWinner(Candidates, true), static_cast<int32>(INDEX_NONE));
		TestEqual(TEXT("Lista vacía: sin ganadora"), PickTimeLimitWinner(TArray<FTimeLimitCandidate>(), true), static_cast<int32>(INDEX_NONE));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNBeachRaceRoundLeftoversTest,
	"Tortunabo.BeachRace.RoundLeftovers",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNBeachRaceRoundLeftoversTest::RunTest(const FString& Parameters)
{
	using namespace TNBeachRaceRules;

	// #71: al preparar la ronda siguiente se quita lo suelto que dejaron las jugadoras (pickups soltados, bolas paradas,
	// conchas trampa, cajas de objetos): todo eso se crea jugando.
	TestTrue(TEXT("Un pickup creado en la ronda se quita"), ShouldClearRoundLeftover(false, false));
	// Lo colocado a mano en el nivel no es de ninguna ronda: no se pierde.
	TestFalse(TEXT("Un pickup colocado en el nivel se queda"), ShouldClearRoundLeftover(true, false));
	// Lo que ya se está destruyendo no se vuelve a destruir.
	TestFalse(TEXT("Lo que ya se destruye se deja en paz"), ShouldClearRoundLeftover(false, true));
	TestFalse(TEXT("Colocado en el nivel y destruyéndose: nada"), ShouldClearRoundLeftover(true, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNBeachRaceLiveTest,
	"Tortunabo.BeachRace.RaceLive",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNBeachRaceLiveTest::RunTest(const FString& Parameters)
{
	using namespace TNBeachRaceRules;

	// #72: es la regla que miran la mina, el cangrejo, la gaviota y ahora también el disco, el protector solar y el rayo antes
	// de golpear (ATN_BeachEnemy::IsRaceLive y TNBeachRideKit::IsRaceLive la usan tal cual): con la carrera parada, ninguno
	// aplica efecto.
	const ETNBeachRacePhase Phases[] = { ETNBeachRacePhase::Waiting, ETNBeachRacePhase::Racing, ETNBeachRacePhase::RoundResults,
		ETNBeachRacePhase::Champion, ETNBeachRacePhase::SprintIntro };
	const ETNBeachFinishCountdown Countdowns[] = { ETNBeachFinishCountdown::None, ETNBeachFinishCountdown::Counting,
		ETNBeachFinishCountdown::TimeUp, ETNBeachFinishCountdown::AllIn };
	for (const ETNBeachRacePhase Phase : Phases)
	{
		for (const ETNBeachFinishCountdown Countdown : Countdowns)
		{
			const bool bPhaseStops = Phase == ETNBeachRacePhase::RoundResults || Phase == ETNBeachRacePhase::Champion
				|| Phase == ETNBeachRacePhase::SprintIntro;
			const bool bCountdownStops = Countdown == ETNBeachFinishCountdown::TimeUp || Countdown == ETNBeachFinishCountdown::AllIn;
			TestEqual(*FString::Printf(TEXT("Fase %d, cuenta %d"), static_cast<int32>(Phase), static_cast<int32>(Countdown)),
				IsRaceLive(Phase, Countdown), !(bPhaseStops || bCountdownStops));
		}
	}

	// Lo que importa, con nombre: en marcha se ataca (también en la espera, a propósito, y durante la cuenta de 10 s tras la
	// primera en el agua) y parada no (tras la cuenta, en el recuento, en el título del sprint y en el podio).
	TestTrue(TEXT("Esperando (3, 2, 1) cuenta como carrera"), IsRaceLive(ETNBeachRacePhase::Waiting, ETNBeachFinishCountdown::None));
	TestTrue(TEXT("Corriendo"), IsRaceLive(ETNBeachRacePhase::Racing, ETNBeachFinishCountdown::None));
	TestTrue(TEXT("Cuenta de meta en marcha: aún se corre"), IsRaceLive(ETNBeachRacePhase::Racing, ETNBeachFinishCountdown::Counting));
	TestFalse(TEXT("«¡TIEMPO!»"), IsRaceLive(ETNBeachRacePhase::Racing, ETNBeachFinishCountdown::TimeUp));
	TestFalse(TEXT("«¡TODAS AL AGUA!»"), IsRaceLive(ETNBeachRacePhase::Racing, ETNBeachFinishCountdown::AllIn));
	TestFalse(TEXT("Recuento de conchas"), IsRaceLive(ETNBeachRacePhase::RoundResults, ETNBeachFinishCountdown::None));
	TestFalse(TEXT("Título del sprint final"), IsRaceLive(ETNBeachRacePhase::SprintIntro, ETNBeachFinishCountdown::None));
	TestFalse(TEXT("Podio"), IsRaceLive(ETNBeachRacePhase::Champion, ETNBeachFinishCountdown::None));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
