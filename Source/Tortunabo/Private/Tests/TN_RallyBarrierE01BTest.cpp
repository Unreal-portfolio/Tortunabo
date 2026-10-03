// Barrera del trazado de E01B (#303, director, 03-10): continua a los dos lados, sin ningún hueco más ancho que una tortuga a
// lo largo de todo el recorrido. Construye la pista del manifest de E01B_espana_rally en un mundo vacío (como la carrera, con
// ATN_RallyTrack::BuildFromVariant), la muestrea como el decorado (TNRallyDressing::SampleTrack) y mide los huecos de la
// barrera planificada (TNRallyDressing::BarrierGapsCm). Solo editor: Scripts/ no se empaqueta. Headless:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Rally.Dressing.E01B; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Rally/TN_RallyLogic.h"
#include "Rally/TN_RallyTrack.h"
#include "Rally/TN_RallyTrackDressing.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TNRallyBarrierE01BTest
{
	const TCHAR* const VariantName = TEXT("E01B_espana_rally");
	/** El mismo paso que ATN_RallyTrackDressing::SampleStepCm por defecto. */
	constexpr double SampleStepCm = 400.0;

	struct FScopedTestWorld
	{
		UWorld* World = nullptr;

		FScopedTestWorld()
		{
			if (!GEngine)
			{
				return;
			}
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("TNRallyBarrierE01BTestWorld"));
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}

		~FScopedTestWorld()
		{
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNRallyBarrierE01BTest, "Tortunabo.Rally.Dressing.E01B.BarrierHasNoGaps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTNRallyBarrierE01BTest::RunTest(const FString& Parameters)
{
	using namespace TNRallyDressing;
	const FString Manifest = TNRally::VariantManifestPath(FName(TNRallyBarrierE01BTest::VariantName));
	if (!TestTrue(*FString::Printf(TEXT("Manifest de E01B en %s"), *Manifest), FPaths::FileExists(Manifest)))
	{
		return false;
	}
	TNRallyBarrierE01BTest::FScopedTestWorld Scoped;
	if (!TestNotNull(TEXT("Mundo de prueba"), Scoped.World))
	{
		return false;
	}
	ATN_RallyTrack* Track = Scoped.World->SpawnActor<ATN_RallyTrack>();
	if (!TestNotNull(TEXT("Pista"), Track) || !TestTrue(TEXT("La pista de E01B se construye"), Track->BuildFromVariant(FName(TNRallyBarrierE01BTest::VariantName))))
	{
		return false;
	}
	const FTrackData Data = SampleTrack(*Track, TNRallyBarrierE01BTest::SampleStepCm);
	Track->ClearTrack();
	if (!TestTrue(TEXT("Trazado muestreado"), Data.Samples.Num() >= 3))
	{
		return false;
	}
	// Sin terreno no hay sondas de caída: las caídas solo acercan la barrera al borde, no la abren ni la cierran.
	const FBarrierPlan Plan = PlanBarriers(Data, TArray<uint8>(), FBarrierParams());
	for (int32 Side = LeftSide; Side <= RightSide; ++Side)
	{
		const TCHAR* SideName = Side == LeftSide ? TEXT("izquierda") : TEXT("derecha");
		const TArray<double> Gaps = BarrierGapsCm(Data, Plan, Side);
		double Widest = 0.0;
		int32 TooWide = 0;
		for (const double Gap : Gaps)
		{
			Widest = FMath::Max(Widest, Gap);
			TooWide += Gap > TurtleWidthCm ? 1 : 0;
		}
		AddInfo(FString::Printf(TEXT("E01B, barrera %s: %d tramos, %d huecos, el mayor de %.0f cm (%.1f km de trazado)."), SideName,
			Plan.Sides[Side].Runs.Num(), Gaps.Num(), Widest, Data.LengthCm / 100000.0));
		TestEqual(*FString::Printf(TEXT("E01B, barrera %s: huecos más anchos que una tortuga (%.0f cm)"), SideName, TurtleWidthCm), TooWide, 0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
