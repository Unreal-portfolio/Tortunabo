// Adornos de las caras de la fortaleza de arena (TNBeachFortressKit::AddFaceShells): ninguna estrella ni concha cae
// delante de un hueco (puerta, ventanas, estandartes), con ninguna semilla.
// Correr desde Session Frontend (categoría "Tortunabo.Beach.FortressDecor") o sin ventana:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Beach.FortressDecor; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "World/Beach/TN_BeachFortressKit.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNBeachFortressDecorTest,
	"Tortunabo.Beach.FortressDecor.KeepOut",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNBeachFortressDecorTest::RunTest(const FString& Parameters)
{
	using namespace TNBeachFortressKit;
	const FVector Normal = FVector::ForwardVector;
	const FVector Along = FVector::RightVector;
	const FFaceKeepOut Gate = { -300.0, 300.0, 0.0, 400.0 };
	for (uint32 Seed = 1; Seed <= 200; ++Seed)
	{
		FBuffers Free;
		AddFaceShells(Free, FVector::ZeroVector, Normal, Along, 1500.0, 0.0, 800.0, 10, Seed);
		TestTrue(TEXT("Sin huecos se adorna la cara"), Free.Verts.Num() > 0);

		FBuffers Kept;
		AddFaceShells(Kept, FVector::ZeroVector, Normal, Along, 1500.0, 0.0, 800.0, 10, Seed, { Gate });
		for (const FVector& V : Kept.Verts)
		{
			if (FMath::Abs(V.Y) < Gate.U1 && V.Z < Gate.Z1)
			{
				AddError(FString::Printf(TEXT("Adorno dentro del hueco (semilla %u): %s"), Seed, *V.ToString()));
				return false;
			}
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
