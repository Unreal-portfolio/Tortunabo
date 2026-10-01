// Red de la plataforma tambaleante (issue #20): la pose de la tabla del servidor viaja en tres bytes (TNWobblyPlatformNet,
// TN_BeachWobblyPlatform.h). Correr desde Session Frontend (categoría "Tortunabo.Beach.WobblyPlatform") o headless:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Beach.WobblyPlatform; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "World/Beach/TN_BeachWobblyPlatform.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TNWobblyPlatformNetTestHelpers
{
	/** Esquina de la tabla (semilargo X, semiancho Y) con su pose: alabeo y cabeceo en grados y hundimiento en cm. */
	FVector BoardCorner(double HalfL, double HalfW, float Roll, float Pitch, float Sag)
	{
		return FRotator(Pitch, 0.f, Roll).RotateVector(FVector(HalfL, HalfW, 0.0)) - FVector(0.0, 0.0, Sag);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNWobblyPlatformPoseNetTest,
	"Tortunabo.Beach.WobblyPlatform.PoseNet",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNWobblyPlatformPoseNetTest::RunTest(const FString& Parameters)
{
	using namespace TNWobblyPlatformNet;

	// Quieta: todo a cero.
	TestEqual(TEXT("Alabeo 0"), QuantizeRoll(0.f), static_cast<int8>(0));
	TestEqual(TEXT("Cabeceo 0"), QuantizePitch(0.f), static_cast<int8>(0));
	TestEqual(TEXT("Hundimiento 0"), QuantizeSag(0.f), static_cast<uint8>(0));

	// Lo que puede dar el muelle cabe sin recortar: alabeo hasta MaxRollDeg (15 como mucho) x 1,4, cabeceo hasta 3° y
	// hundimiento hasta 18 cm (ocho tortugas y la grieta a punto).
	for (const float Roll : { -21.f, -7.f, -0.33f, 0.1f, 4.5f, 9.8f, 21.f })
	{
		TestEqual(FString::Printf(TEXT("Alabeo %.2f ida y vuelta"), Roll), DequantizeRoll(QuantizeRoll(Roll)), Roll, RollStepDeg * 0.5f + KINDA_SMALL_NUMBER);
	}
	for (const float Pitch : { -3.f, -1.2f, 0.01f, 2.f, 3.f })
	{
		TestEqual(FString::Printf(TEXT("Cabeceo %.2f ida y vuelta"), Pitch), DequantizePitch(QuantizePitch(Pitch)), Pitch, PitchStepDeg * 0.5f + KINDA_SMALL_NUMBER);
	}
	for (const float Sag : { 0.f, 1.5f, 3.f, 9.f, 18.f })
	{
		TestEqual(FString::Printf(TEXT("Hundimiento %.1f ida y vuelta"), Sag), DequantizeSag(QuantizeSag(Sag)), Sag, SagStepCm * 0.5f + KINDA_SMALL_NUMBER);
	}

	// Fuera de rango se recorta (nunca da la vuelta en el byte).
	TestEqual(TEXT("Alabeo enorme: tope positivo"), QuantizeRoll(90.f), static_cast<int8>(127));
	TestEqual(TEXT("Alabeo enorme negativo: tope negativo"), QuantizeRoll(-90.f), static_cast<int8>(-127));
	TestEqual(TEXT("Cabeceo enorme: tope"), QuantizePitch(10.f), static_cast<int8>(127));
	TestEqual(TEXT("Hundimiento negativo: 0"), QuantizeSag(-4.f), static_cast<uint8>(0));
	TestEqual(TEXT("Hundimiento enorme: 255"), QuantizeSag(100.f), static_cast<uint8>(255));

	// La esquina de la tabla más grande (434 x 150 cm de semiejes) con la pose recibida, contra la del servidor: menos de
	// 5 mm de error en todo el recorrido del muelle (el desfase pedido es menos de 5 cm).
	double WorstCorner = 0.0;
	for (float Roll = -21.f; Roll <= 21.f; Roll += 0.37f)
	{
		for (float Pitch = -3.f; Pitch <= 3.f; Pitch += 0.29f)
		{
			const float Sag = FMath::Abs(Roll) * 0.4f;
			const FVector Server = TNWobblyPlatformNetTestHelpers::BoardCorner(434.0, 150.0, Roll, Pitch, Sag);
			const FVector Client = TNWobblyPlatformNetTestHelpers::BoardCorner(434.0, 150.0, DequantizeRoll(QuantizeRoll(Roll)),
				DequantizePitch(QuantizePitch(Pitch)), DequantizeSag(QuantizeSag(Sag)));
			WorstCorner = FMath::Max(WorstCorner, FVector::Dist(Server, Client));
		}
	}
	TestTrue(FString::Printf(TEXT("Esquina de la tabla: %.2f cm de error como mucho (< 0,5)"), WorstCorner), WorstCorner < 0.5);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
