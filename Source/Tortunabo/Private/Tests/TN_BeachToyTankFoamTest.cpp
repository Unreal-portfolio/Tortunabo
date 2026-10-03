// Bolitas de espuma del tanque de juguete (issue #247): rebote puro contra una pared (TNBeachTankFoam::BounceOffWall, el que
// usa ATN_BeachToyTank al chocar con lo que hay por medio). La bolita no cruza la pared: sale hacia fuera y más despacio.
// Correr desde Session Frontend (categoría "Tortunabo.Beach.ToyTank") o sin ventana:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Beach.ToyTank; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "World/Beach/TN_BeachToyTank.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNBeachToyTankFoamBounceTest, "Tortunabo.Beach.ToyTank.FoamBounce",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNBeachToyTankFoamBounceTest::RunTest(const FString& Parameters)
{
	using namespace TNBeachTankFoam;

	// De frente contra una muralla (su cara mira a -X): vuelve hacia atrás, más despacio, y no sigue hacia dentro.
	{
		const FVector Normal(-1.0, 0.0, 0.0);
		const FVector In(1900.0, 0.0, -300.0);
		const FVector Out = BounceOffWall(In, Normal);
		TestTrue(TEXT("Sale de la pared, no la cruza"), FVector::DotProduct(Out, Normal) > 0.0);
		TestTrue(TEXT("Pierde fuerza al rebotar"), Out.Size() < In.Size());
		TestTrue(TEXT("Rebota con su parte de restitución"), FMath::IsNearlyEqual(Out.X, -1900.0 * WallRestitution, 0.5));
		TestTrue(TEXT("Lo que lleva a lo largo de la pared se frena, no cambia de sentido"), FMath::IsNearlyEqual(Out.Z, -300.0 * WallFriction, 0.5));
	}

	// En diagonal contra una pared girada: tampoco la cruza.
	{
		const FVector Normal = FVector(-1.0, -1.0, 0.0).GetSafeNormal();
		const FVector In(1500.0, 600.0, 200.0);
		const FVector Out = BounceOffWall(In, Normal);
		TestTrue(TEXT("En diagonal sale de la pared"), FVector::DotProduct(Out, Normal) > 0.0);
		TestTrue(TEXT("En diagonal pierde fuerza"), Out.Size() < In.Size());
	}

	// Contra lo alto de una muralla o una duna (normal hacia arriba): bota hacia arriba.
	{
		const FVector Out = BounceOffWall(FVector(800.0, 0.0, -900.0), FVector::UpVector);
		TestTrue(TEXT("En lo alto bota hacia arriba"), Out.Z > 0.0);
		TestTrue(TEXT("En lo alto sigue hacia delante"), Out.X > 0.0);
	}

	// Si ya se alejaba de la superficie (la rozaba al salir), sigue igual.
	{
		const FVector In(-500.0, 0.0, 100.0);
		const FVector Out = BounceOffWall(In, FVector(-1.0, 0.0, 0.0));
		TestTrue(TEXT("Alejándose, no cambia"), Out.Equals(In, 0.01));
	}
	return true;
}

#endif
