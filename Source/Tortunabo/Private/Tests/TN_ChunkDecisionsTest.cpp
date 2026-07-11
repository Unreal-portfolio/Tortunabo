// Fase 4.3 (ampliación): lógica pura de selección de chunk. Sin mundo, sin
// actores — se testean las funciones de TN_ChunkDecisions.h que ATN_ChunkManager
// usa en producción (RNG inyectado como functor determinista). Correr desde
// Session Frontend (categoría "Tortunabo.Chunks") o headless:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Chunks; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "World/TN_ChunkDecisions.h"

#if WITH_DEV_AUTOMATION_TESTS

// ─────────────────────────────────────────────────────────────────────────────
// Dificultad por progreso
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNChunkDifficultyProgressTest,
	"Tortunabo.Chunks.DifficultyProgress",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNChunkDifficultyProgressTest::RunTest(const FString& Parameters)
{
	using namespace TNChunkLogic;

	TestTrue(TEXT("Al arrancar (0 pasados) → Easy"),
		ComputeDifficultyFromProgress(0, 3, 6) == ETNChunkDifficulty::Easy);

	TestTrue(TEXT("Justo bajo el primer umbral → Easy"),
		ComputeDifficultyFromProgress(2, 3, 6) == ETNChunkDifficulty::Easy);

	// Borde: count == umbral pasa al tier siguiente (los umbrales usan <)
	TestTrue(TEXT("count == EasyToMedium → Medium (borde <)"),
		ComputeDifficultyFromProgress(3, 3, 6) == ETNChunkDifficulty::Medium);

	TestTrue(TEXT("count == MediumToHard → Hard (borde <)"),
		ComputeDifficultyFromProgress(6, 3, 6) == ETNChunkDifficulty::Hard);

	TestTrue(TEXT("Muy por encima de los umbrales → Hard"),
		ComputeDifficultyFromProgress(50, 3, 6) == ETNChunkDifficulty::Hard);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Secuencia personalizada
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNChunkCustomSequenceTest,
	"Tortunabo.Chunks.CustomSequence",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNChunkCustomSequenceTest::RunTest(const FString& Parameters)
{
	using namespace TNChunkLogic;

	const TArray<ETNChunkDifficulty> Seq = {
		ETNChunkDifficulty::Easy, ETNChunkDifficulty::Medium, ETNChunkDifficulty::Easy };

	TestTrue(TEXT("Índice válido devuelve la entrada de la secuencia"),
		ResolveCustomSequenceDifficulty(Seq, 1) == ETNChunkDifficulty::Medium);

	TestTrue(TEXT("Último índice válido"),
		ResolveCustomSequenceDifficulty(Seq, 2) == ETNChunkDifficulty::Easy);

	// Fuera de rango → Hard como fallback (regla del diseño, no un crash)
	TestTrue(TEXT("Índice fuera de rango → Hard"),
		ResolveCustomSequenceDifficulty(Seq, 3) == ETNChunkDifficulty::Hard);

	TestTrue(TEXT("Índice negativo → Hard"),
		ResolveCustomSequenceDifficulty(Seq, -1) == ETNChunkDifficulty::Hard);

	TestTrue(TEXT("Secuencia vacía → Hard"),
		ResolveCustomSequenceDifficulty({}, 0) == ETNChunkDifficulty::Hard);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Orden de fallback de pools
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNChunkPoolFallbackOrderTest,
	"Tortunabo.Chunks.PoolFallbackOrder",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNChunkPoolFallbackOrderTest::RunTest(const FString& Parameters)
{
	using namespace TNChunkLogic;

	const FPoolFallbackOrder EasyOrder = GetPoolFallbackOrder(ETNChunkDifficulty::Easy);
	TestTrue(TEXT("Easy → primario Easy"),      EasyOrder.Primary   == ETNChunkDifficulty::Easy);
	TestTrue(TEXT("Easy → fallback1 Medium"),   EasyOrder.Fallback1 == ETNChunkDifficulty::Medium);
	TestTrue(TEXT("Easy → fallback2 Hard"),     EasyOrder.Fallback2 == ETNChunkDifficulty::Hard);

	const FPoolFallbackOrder MediumOrder = GetPoolFallbackOrder(ETNChunkDifficulty::Medium);
	TestTrue(TEXT("Medium → primario Medium"),  MediumOrder.Primary   == ETNChunkDifficulty::Medium);
	TestTrue(TEXT("Medium → fallback1 Easy"),   MediumOrder.Fallback1 == ETNChunkDifficulty::Easy);
	TestTrue(TEXT("Medium → fallback2 Hard"),   MediumOrder.Fallback2 == ETNChunkDifficulty::Hard);

	const FPoolFallbackOrder HardOrder = GetPoolFallbackOrder(ETNChunkDifficulty::Hard);
	TestTrue(TEXT("Hard → primario Hard"),      HardOrder.Primary   == ETNChunkDifficulty::Hard);
	TestTrue(TEXT("Hard → fallback1 Medium"),   HardOrder.Fallback1 == ETNChunkDifficulty::Medium);
	TestTrue(TEXT("Hard → fallback2 Easy"),     HardOrder.Fallback2 == ETNChunkDifficulty::Easy);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Selección anti-repetición con RNG inyectado
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNChunkSelectIndexTest,
	"Tortunabo.Chunks.SelectIndexAvoidingRepeat",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNChunkSelectIndexTest::RunTest(const FString& Parameters)
{
	using namespace TNChunkLogic;

	// RNG que nunca debería llamarse en los casos triviales
	auto NeverCalled = [this](int32, int32) -> int32
	{
		AddError(TEXT("El RNG no debe consultarse con pool vacío o de tamaño 1"));
		return 0;
	};

	TestEqual(TEXT("Pool vacío → INDEX_NONE"),
		SelectIndexAvoidingRepeat(0, 2, NeverCalled), (int32)INDEX_NONE);

	TestEqual(TEXT("Pool de tamaño 1 → siempre índice 0 (sin consultar RNG)"),
		SelectIndexAvoidingRepeat(1, 0, NeverCalled), 0);

	// RNG determinista: devuelve primero el repetido y luego otro distinto →
	// la selección debe saltarse el repetido
	{
		int32 Calls = 0;
		auto RepeatThenDifferent = [&Calls](int32, int32) -> int32
		{
			return (++Calls == 1) ? 2 : 4; // primer intento repite LastIndex=2
		};
		TestEqual(TEXT("Evita el índice repetido y devuelve el siguiente distinto"),
			SelectIndexAvoidingRepeat(5, 2, RepeatThenDifferent), 4);
	}

	// RNG adversario que repite SIEMPRE: tras 10 intentos se acepta la
	// repetición — la función termina, nunca cuelga
	{
		int32 Calls = 0;
		auto AlwaysRepeat = [&Calls](int32, int32) -> int32 { ++Calls; return 3; };
		TestEqual(TEXT("RNG que insiste → tras los reintentos devuelve el repetido"),
			SelectIndexAvoidingRepeat(5, 3, AlwaysRepeat), 3);
		TestEqual(TEXT("El bucle se corta exactamente en 10 intentos"), Calls, 10);
	}

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Chunk final
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNChunkFinalChunkTest,
	"Tortunabo.Chunks.FinalChunk",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNChunkFinalChunkTest::RunTest(const FString& Parameters)
{
	using namespace TNChunkLogic;

	TestFalse(TEXT("Bajo el total → sigue spawneando chunks normales"),
		ShouldSpawnFinalChunk(5, 10));

	// Borde: alcanzar EXACTAMENTE el total dispara el final (>=)
	TestTrue(TEXT("passed == total → chunk final (borde >=)"),
		ShouldSpawnFinalChunk(10, 10));

	TestTrue(TEXT("Por encima del total → chunk final"),
		ShouldSpawnFinalChunk(11, 10));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
