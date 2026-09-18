// Lógica pura del salto. Sin mundo, sin actores — se testean las funciones de
// TN_JumpTuning.h que ATortugaCharacter::ApplyJumpTuning usa en producción.
// Correr desde Session Frontend (categoría "Tortunabo.Jump") o headless:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Jump; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "Player/TN_JumpTuning.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TNJumpTuningTest
{
	constexpr float TestWorldGravityZ = -980.f;
	constexpr float TestWalkSpeed     = 450.f;
	constexpr float TestSprintSpeed   = 800.f;

	/** Altura máxima de un tiro vertical: v² / 2g. */
	float SimulatedApexHeight(const TNJumpLogic::FJumpTuning& Tuning)
	{
		const float Gravity = FMath::Abs(TestWorldGravityZ) * Tuning.GravityScale;
		return FMath::Square(Tuning.JumpZVelocity) / (2.f * Gravity);
	}

	/** Tiempo hasta volver a la altura de despegue: 2v / g. */
	float SimulatedAirTime(const TNJumpLogic::FJumpTuning& Tuning)
	{
		const float Gravity = FMath::Abs(TestWorldGravityZ) * Tuning.GravityScale;
		return 2.f * Tuning.JumpZVelocity / Gravity;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Los valores derivados producen la altura y la distancia pedidas
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNJumpTuningMatchesTargetsTest,
	"Tortunabo.Jump.TuningMatchesTargets",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNJumpTuningMatchesTargetsTest::RunTest(const FString& Parameters)
{
	using namespace TNJumpLogic;
	using namespace TNJumpTuningTest;

	FJumpTuning Tuning;
	TestTrue(TEXT("Entradas por defecto → salto definible"),
		ComputeJumpTuning(120.f, 500.f, TestSprintSpeed, TestWorldGravityZ, Tuning));

	TestEqual(TEXT("Tiempo de vuelo = distancia / velocidad"), Tuning.AirTime, 0.625f, 0.001f);
	TestEqual(TEXT("La física reproduce la altura pedida"), SimulatedApexHeight(Tuning), 120.f, 0.1f);
	TestEqual(TEXT("La física reproduce el tiempo de vuelo"), SimulatedAirTime(Tuning), Tuning.AirTime, 0.001f);
	TestEqual(TEXT("Distancia a sprint = 500 cm"), ComputeJumpDistance(TestSprintSpeed, Tuning.AirTime), 500.f, 0.1f);
	TestEqual(TEXT("Distancia andando = 500 × 450/800"), ComputeJumpDistance(TestWalkSpeed, Tuning.AirTime), 281.25f, 0.1f);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Altura y distancia son independientes entre sí
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNJumpTuningIndependentAxesTest,
	"Tortunabo.Jump.HeightAndDistanceAreIndependent",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNJumpTuningIndependentAxesTest::RunTest(const FString& Parameters)
{
	using namespace TNJumpLogic;
	using namespace TNJumpTuningTest;

	FJumpTuning Low, High, Far;
	ComputeJumpTuning(120.f, 500.f, TestSprintSpeed, TestWorldGravityZ, Low);
	ComputeJumpTuning(200.f, 500.f, TestSprintSpeed, TestWorldGravityZ, High);
	ComputeJumpTuning(120.f, 800.f, TestSprintSpeed, TestWorldGravityZ, Far);

	TestEqual(TEXT("Subir la altura no cambia el tiempo de vuelo"), High.AirTime, Low.AirTime, 0.001f);
	TestEqual(TEXT("Subir la altura se refleja en la física"), SimulatedApexHeight(High), 200.f, 0.1f);
	TestEqual(TEXT("Subir la distancia no cambia la altura"), SimulatedApexHeight(Far), 120.f, 0.1f);
	TestTrue(TEXT("Más distancia → menos gravedad"), Far.GravityScale < Low.GravityScale);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Entradas no válidas
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNJumpTuningInvalidInputsTest,
	"Tortunabo.Jump.InvalidInputsAreRejected",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNJumpTuningInvalidInputsTest::RunTest(const FString& Parameters)
{
	using namespace TNJumpLogic;
	using namespace TNJumpTuningTest;

	FJumpTuning Tuning;
	Tuning.JumpZVelocity = 123.f;

	TestFalse(TEXT("Altura 0 → rechazado"), ComputeJumpTuning(0.f, 500.f, TestSprintSpeed, TestWorldGravityZ, Tuning));
	TestFalse(TEXT("Distancia 0 → rechazado (evita división por cero)"), ComputeJumpTuning(120.f, 0.f, TestSprintSpeed, TestWorldGravityZ, Tuning));
	TestFalse(TEXT("Velocidad 0 → rechazado (evita división por cero)"), ComputeJumpTuning(120.f, 500.f, 0.f, TestWorldGravityZ, Tuning));
	TestFalse(TEXT("Gravedad 0 → rechazado"), ComputeJumpTuning(120.f, 500.f, TestSprintSpeed, 0.f, Tuning));
	TestEqual(TEXT("La salida no se toca al rechazar"), Tuning.JumpZVelocity, 123.f);

	FJumpTuning Positive;
	TestTrue(TEXT("Gravedad con signo positivo se acepta por valor absoluto"),
		ComputeJumpTuning(120.f, 500.f, TestSprintSpeed, 980.f, Positive));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
