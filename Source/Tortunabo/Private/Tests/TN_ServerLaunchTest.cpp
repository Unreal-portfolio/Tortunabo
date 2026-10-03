// Lanzamientos que decide el servidor y estrena el cliente dueño (FTNServerLaunch, #18). Headless:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Movement.ServerLaunch; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "Player/TN_ServerLaunch.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNServerLaunchServerTest,
	"Tortunabo.Movement.ServerLaunch.Server",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNServerLaunchServerTest::RunTest(const FString& Parameters)
{
	FTNServerLaunch Launch;
	FVector Sent;
	const uint8 Id = Launch.Grant(FVector(612.4, -233.6, 371.5), 10.0, Sent);
	TestNotEqual(TEXT("El número nunca es 0"), static_cast<int32>(Id), 0);
	TestEqual(TEXT("Se manda en cm/s enteros"), Sent, FVector(612.0, -234.0, 372.0));

	FVector Taken;
	TestFalse(TEXT("Un número que no es el concedido no lanza"), Launch.Take(static_cast<uint8>(Id + 1), Taken));
	TestFalse(TEXT("Sin número no lanza"), Launch.Take(0, Taken));
	TestTrue(TEXT("El movimiento con el número lanza"), Launch.Take(Id, Taken));
	TestEqual(TEXT("Con la misma velocidad que se mandó"), Taken, Sent);
	TestFalse(TEXT("Solo una vez (un movimiento repetido no lanza otra vez)"), Launch.Take(Id, Taken));

	// Otro concedido sustituye al anterior sin usar.
	const uint8 First = Launch.Grant(FVector(100.0, 0.0, 0.0), 20.0, Sent);
	const uint8 Second = Launch.Grant(FVector(200.0, 0.0, 0.0), 20.1, Sent);
	TestFalse(TEXT("El sustituido ya no lanza"), Launch.Take(First, Taken));
	TestFalse(TEXT("Dentro del plazo no caduca"), Launch.TakeExpired(20.1 + FTNServerLaunch::TimeoutSeconds * 0.5, Taken));
	TestTrue(TEXT("Pasado el plazo lo aplica el servidor"), Launch.TakeExpired(20.1 + FTNServerLaunch::TimeoutSeconds + 0.1, Taken));
	TestEqual(TEXT("El último concedido"), Taken, FVector(200.0, 0.0, 0.0));
	TestFalse(TEXT("Caducado ya no lo estrena el dueño"), Launch.Take(Second, Taken));

	// Tras 255 concesiones el número da la vuelta sin pasar por 0.
	for (int32 Index = 0; Index < 300; ++Index)
	{
		TestNotEqual(TEXT("Nunca 0"), static_cast<int32>(Launch.Grant(FVector::ZeroVector, 30.0, Sent)), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNServerLaunchClientTest,
	"Tortunabo.Movement.ServerLaunch.Client",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNServerLaunchClientTest::RunTest(const FString& Parameters)
{
	const FVector Velocity(450.0, 120.0, 300.0);
	FTNServerLaunch Launch;
	FVector Out;
	TestFalse(TEXT("Sin nada recibido no lanza"), Launch.BeginMove(0.0, Out));

	Launch.Receive(7, Velocity, 5.0);
	TestTrue(TEXT("El siguiente movimiento lo estrena"), Launch.BeginMove(5.05, Out));
	TestEqual(TEXT("Con la velocidad recibida"), Out, Velocity);
	TestTrue(TEXT("Espera a su movimiento"), Launch.IsStarting());
	TestTrue(TEXT("El movimiento lo aplica"), Launch.ConfirmMove(1.25f, Velocity));
	TestEqual(TEXT("Ese movimiento lleva el número"), static_cast<int32>(Launch.GetIdForMove(1.25f)), 7);
	TestEqual(TEXT("Los demás, no"), static_cast<int32>(Launch.GetIdForMove(1.27f)), 0);
	TestTrue(TEXT("Al repetirlo tras una corrección se vuelve a lanzar"), Launch.GetForMove(1.25f, Out));
	TestEqual(TEXT("Igual"), Out, Velocity);
	TestFalse(TEXT("Al repetir otro movimiento, no"), Launch.GetForMove(1.27f, Out));
	TestFalse(TEXT("Un movimiento nuevo no lo estrena otra vez"), Launch.BeginMove(5.1, Out));

	// Otro lanzamiento ocupa el movimiento: el concedido se reintenta en el siguiente.
	Launch.Receive(8, Velocity, 6.0);
	TestTrue(TEXT("Empieza"), Launch.BeginMove(6.01, Out));
	TestFalse(TEXT("Otra velocidad en el movimiento: no cuenta"), Launch.ConfirmMove(2.f, FVector(0.0, 0.0, 900.0)));
	TestEqual(TEXT("Ese movimiento no lleva número"), static_cast<int32>(Launch.GetIdForMove(2.f)), 0);
	TestTrue(TEXT("Se reintenta en el siguiente"), Launch.BeginMove(6.02, Out));
	Launch.RetryMove();
	TestTrue(TEXT("Sin movimiento hecho, también"), Launch.BeginMove(6.03, Out));
	TestTrue(TEXT("Y entra"), Launch.ConfirmMove(2.05f, Velocity));
	TestEqual(TEXT("Con su número"), static_cast<int32>(Launch.GetIdForMove(2.05f)), 8);

	// Pasado el plazo el servidor ya la ha lanzado: el dueño no lo aplica.
	Launch.Receive(9, Velocity, 10.0);
	TestFalse(TEXT("Caducado no se estrena"), Launch.BeginMove(10.0 + FTNServerLaunch::TimeoutSeconds + 0.1, Out));
	TestFalse(TEXT("Ni después"), Launch.BeginMove(10.0 + FTNServerLaunch::TimeoutSeconds + 0.2, Out));
	return true;
}

#endif
