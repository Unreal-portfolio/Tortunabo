// Fallo de viaje (N-E): el anfitrión vuelve al lobby con todos; el invitado, al menú; un segundo fallo seguido, al menú.
// Se testea la función de TN_TravelFailureDecisions.h que usa UTN_TravelFailureSubsystem.
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Net; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "Multiplayer/TN_TravelFailureDecisions.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNTravelFailureTest,
	"Tortunabo.Net.TravelFailure",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNTravelFailureTest::RunTest(const FString& Parameters)
{
	using namespace TNTravel;

	TestTrue(TEXT("Anfitrión en partida → lobby"),
		DecideTravelFailure(NM_ListenServer, false, 0) == ETravelFailureAction::ReturnHostToLobby);
	TestTrue(TEXT("Servidor dedicado → lobby"),
		DecideTravelFailure(NM_DedicatedServer, false, 0) == ETravelFailureAction::ReturnHostToLobby);
	TestTrue(TEXT("Invitado → menú"),
		DecideTravelFailure(NM_Client, false, 0) == ETravelFailureAction::ReturnToMenu);
	TestTrue(TEXT("Sin red → menú"),
		DecideTravelFailure(NM_Standalone, false, 0) == ETravelFailureAction::ReturnToMenu);
	TestTrue(TEXT("Ya en el menú → se queda"),
		DecideTravelFailure(NM_Client, true, 0) == ETravelFailureAction::StayInMenu);
	TestTrue(TEXT("Anfitrión con un fallo previo → menú (sin bucle lobby → lobby)"),
		DecideTravelFailure(NM_ListenServer, false, 1) == ETravelFailureAction::ReturnToMenu);
	TestEqual(TEXT("Lobby por defecto"), LobbyTravelURL(FString()), FString(TEXT("/Game/Maps/Lobby/LVL_Lobby")));
	TestEqual(TEXT("Lobby del que se salió"), LobbyTravelURL(TEXT("/Game/Maps/Lobby/LVL_HQ")), FString(TEXT("/Game/Maps/Lobby/LVL_HQ")));
	return true;
}

#endif
