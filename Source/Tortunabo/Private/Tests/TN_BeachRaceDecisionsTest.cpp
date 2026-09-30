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

#endif // WITH_DEV_AUTOMATION_TESTS
