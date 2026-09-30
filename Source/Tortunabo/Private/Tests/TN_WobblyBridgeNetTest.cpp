// Red del puente tambaleante (issue #19): la agitación del servidor viaja en un byte (TNWobblyBridgeNet, TN_WobblyBridge.h).
// Correr desde Session Frontend (categoría "Tortunabo.Playground.WobblyBridge") o headless:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Playground.WobblyBridge; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "Lobby/Playground/TN_WobblyBridge.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNWobblyBridgeExcitationNetTest,
	"Tortunabo.Playground.WobblyBridge.ExcitationNet",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNWobblyBridgeExcitationNetTest::RunTest(const FString& Parameters)
{
	using namespace TNWobblyBridgeNet;
	const float Step = MaxExcitation / 255.f;

	TestEqual(TEXT("Quieto: 0"), QuantizeExcitation(0.f), static_cast<uint8>(0));
	TestEqual(TEXT("Al máximo: 255"), QuantizeExcitation(MaxExcitation), static_cast<uint8>(255));
	TestEqual(TEXT("Por encima del máximo: 255"), QuantizeExcitation(MaxExcitation * 3.f), static_cast<uint8>(255));
	TestEqual(TEXT("Negativa: 0"), QuantizeExcitation(-1.f), static_cast<uint8>(0));

	// Ida y vuelta: el cliente recibe la del servidor con un error de medio paso como mucho (menos de 1,5 mm de vaivén).
	float WorstError = 0.f;
	for (float Excitation = 0.f; Excitation <= MaxExcitation; Excitation += 0.013f)
	{
		WorstError = FMath::Max(WorstError, FMath::Abs(DequantizeExcitation(QuantizeExcitation(Excitation)) - Excitation));
	}
	TestTrue(TEXT("Error de ida y vuelta de medio paso como mucho"), WorstError <= Step * 0.5f + KINDA_SMALL_NUMBER);
	TestTrue(TEXT("En el vaivén (24 cm por unidad), menos de 1,5 mm"), WorstError * 24.f < 0.15f);

	// El reposo (IdleWobble 0,3 de serie) llega casi igual.
	TestEqual(TEXT("Reposo 0,3"), DequantizeExcitation(QuantizeExcitation(0.3f)), 0.3f, Step * 0.5f + KINDA_SMALL_NUMBER);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
