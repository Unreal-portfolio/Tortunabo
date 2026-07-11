// Fase 4.3 (ampliación): lógica pura de scoring — persistencia por delta y orden
// del scoreboard. Sin mundo, sin actores — se testean las funciones de
// TN_ScoreDecisions.h que TN_CoopGameState usa en producción. Correr desde
// Session Frontend (categoría "Tortunabo.Score") o headless:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Score; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "Core/TN_ScoreDecisions.h"

#if WITH_DEV_AUTOMATION_TESTS

// ─────────────────────────────────────────────────────────────────────────────
// Persistencia por delta (anti-race de replicación en Results)
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNScorePersistDeltaTest,
	"Tortunabo.Score.PersistDelta",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNScorePersistDeltaTest::RunTest(const FString& Parameters)
{
	using namespace TNScoreLogic;

	// Primer persist de la carrera: nada acumulado → se persiste todo el score
	TestEqual(TEXT("Primer persist devuelve el score completo"),
		ComputePersistDelta(250, 0), 250);

	// Delta incremental: llegó más score tras el primer persist
	TestEqual(TEXT("Segundo persist devuelve solo lo nuevo"),
		ComputePersistDelta(300, 250), 50);

	// IDEMPOTENCIA: persistir dos veces el mismo estado no duplica nada.
	// (Es la garantía que protege contra el doble-persist Results + OnRep_RaceScore.)
	TestEqual(TEXT("Re-persistir sin cambios devuelve 0"),
		ComputePersistDelta(300, 300), 0);

	// Score visible menor que lo persistido (estado stale / reset) → nunca negativo
	TestEqual(TEXT("Score menor que lo persistido devuelve 0, nunca negativo"),
		ComputePersistDelta(100, 300), 0);

	// Score 0 con nada persistido → 0 (no se persiste basura)
	TestEqual(TEXT("Score 0 sin persistir devuelve 0"),
		ComputePersistDelta(0, 0), 0);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Orden del scoreboard (ranks → sin-rank → eliminados)
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNScoreResultSortKeyTest,
	"Tortunabo.Score.ResultSortKey",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNScoreResultSortKeyTest::RunTest(const FString& Parameters)
{
	using namespace TNScoreLogic;

	// Ranks válidos ordenan entre sí por posición
	TestTrue(TEXT("Rank 1 va antes que rank 2"),
		ComputeResultSortKey(false, 1) < ComputeResultSortKey(false, 2));
	TestTrue(TEXT("Rank 2 va antes que rank 4"),
		ComputeResultSortKey(false, 2) < ComputeResultSortKey(false, 4));

	// Sin-rank (FinishRank=0, aún corriendo) va después de cualquier rank válido
	TestTrue(TEXT("Rank válido va antes que sin-rank"),
		ComputeResultSortKey(false, 4) < ComputeResultSortKey(false, 0));

	// Eliminado va al final, incluso detrás de los sin-rank
	TestTrue(TEXT("Sin-rank va antes que eliminado"),
		ComputeResultSortKey(false, 0) < ComputeResultSortKey(true, 0));

	// Eliminado con rank alto sigue al final (la eliminación pisa el rank)
	TestTrue(TEXT("Eliminado con rank 1 sigue detrás de un rank válido"),
		ComputeResultSortKey(false, 4) < ComputeResultSortKey(true, 1));
	TestTrue(TEXT("Eliminado con rank 1 sigue detrás de un sin-rank"),
		ComputeResultSortKey(false, 0) < ComputeResultSortKey(true, 1));

	// FinishRank negativo se trata como sin-rank (borde defensivo)
	TestEqual(TEXT("Rank negativo equivale a sin-rank"),
		ComputeResultSortKey(false, -1), ComputeResultSortKey(false, 0));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
