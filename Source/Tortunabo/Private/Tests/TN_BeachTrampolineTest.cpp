// Rebote del trampolín de la playa (#21): las reglas puras de TN_BeachTrampolineRules.h que aplica el movimiento de la
// tortuga (UTN_TurtleMovementComponent::TickTrampolineBounce) al empezar cada paso. Sin mundo ni actores: una caída sobre
// el sensor simulada paso a paso como la hacen el cliente dueño (un paso por fotograma), el servidor (los pasos le llegan
// de dos en dos y en otro momento) y el cliente al repetir los pasos tras una corrección; las tres rebotan en el mismo paso
// y acaban en el mismo sitio, y nunca rebotan dos veces en el mismo contacto.
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Beach.Trampoline; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "World/Beach/TN_BeachTrampolineRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TNBeachTrampolineTest
{
	using namespace TNTrampolineRules;

	/** Cara de arriba del sensor (cm): la cápsula lo toca con los pies por debajo. */
	constexpr double SensorTopZ = 315.0;
	constexpr double GravityZ = -2400.0;
	constexpr double StepSeconds = 1.0 / 60.0;

	/** Lo que el movimiento sabe al empezar un paso: dónde están los pies y a qué velocidad va. */
	struct FStepState
	{
		FVector Feet = FVector(0.0, 0.0, 700.0);
		FVector Velocity = FVector(150.0, 0.0, 0.0);
	};

	/** Un paso del movimiento: el rebote al empezar (si toca el sensor y no sube) y después la caída. */
	bool SimulateStep(FStepState& State, const FBounceTuning& Tuning)
	{
		bool bBounced = false;
		if (State.Feet.Z <= SensorTopZ && CanBounce(State.Velocity))
		{
			State.Velocity = BounceVelocity(State.Velocity, FVector::ForwardVector, Tuning);
			bBounced = true;
		}
		State.Velocity.Z += GravityZ * StepSeconds;
		State.Feet += State.Velocity * StepSeconds;
		// El cuerpo del trampolín para la caída (un poco por debajo de la cara del sensor).
		if (State.Feet.Z < SensorTopZ - 15.0)
		{
			State.Feet.Z = SensorTopZ - 15.0;
			State.Velocity.Z = 0.0;
		}
		return bBounced;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNBeachTrampolineBounceTest,
	"Tortunabo.Beach.Trampoline.Bounce",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNBeachTrampolineBounceTest::RunTest(const FString& Parameters)
{
	using namespace TNTrampolineRules;
	const FBounceTuning Tuning;

	TestTrue(TEXT("Cayendo, rebota"), CanBounce(FVector(0.0, 0.0, -900.0)));
	TestTrue(TEXT("Parada encima, rebota"), CanBounce(FVector::ZeroVector));
	TestFalse(TEXT("Subiendo de un salto, no rebota"), CanBounce(FVector(0.0, 0.0, 600.0)));

	const FVector FromRest = BounceVelocity(FVector::ZeroVector, FVector::ForwardVector, Tuning);
	TestEqual(TEXT("Sin caída: la vertical de base"), FromRest.Z, Tuning.BaseUp);
	TestEqual(TEXT("Sin caída: el empujón hacia el mar"), FromRest.X, Tuning.Push);
	TestFalse(TEXT("Justo después del rebote ya no puede rebotar otra vez (sube más rápido que el límite)"), CanBounce(FromRest));

	const FVector FromFall = BounceVelocity(FVector(0.0, 0.0, -1000.0), FVector::ForwardVector, Tuning);
	TestEqual(TEXT("Cayendo a 1000 cm/s rebota más: base + 0,55 · 700"), FromFall.Z, Tuning.BaseUp + Tuning.FallGain * 700.0);
	TestEqual(TEXT("Cayendo de muy alto: el tope"), BounceVelocity(FVector(0.0, 0.0, -9000.0), FVector::ForwardVector, Tuning).Z, Tuning.MaxUp);

	const FVector Fast = BounceVelocity(FVector(4000.0, 0.0, -500.0), FVector::ForwardVector, Tuning);
	TestTrue(TEXT("La horizontal no pasa del tope"), FVector(Fast.X, Fast.Y, 0.0).Size() <= Tuning.MaxHorizontal + 0.01);

	TestEqual(TEXT("Fuerza del efecto con el tope"), BounceStrength(Tuning.MaxUp, Tuning), 1.f);
	TestEqual(TEXT("Fuerza del efecto nunca por debajo de 0,35"), BounceStrength(0.0, Tuning), 0.35f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNBeachTrampolineSameStepTest,
	"Tortunabo.Beach.Trampoline.SameStepEverywhere",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNBeachTrampolineSameStepTest::RunTest(const FString& Parameters)
{
	using namespace TNBeachTrampolineTest;
	const FBounceTuning Tuning;
	constexpr int32 Steps = 150;

	// Cliente dueño: un paso por fotograma, apuntando en qué pasos rebota.
	FStepState Client;
	TArray<int32> ClientBounces;
	TArray<FStepState> ClientStarts;
	for (int32 Step = 0; Step < Steps; ++Step)
	{
		ClientStarts.Add(Client);
		if (SimulateStep(Client, Tuning))
		{
			ClientBounces.Add(Step);
		}
	}
	TestTrue(TEXT("La caída llega al trampolín y rebota"), ClientBounces.Num() >= 1);
	for (int32 i = 1; i < ClientBounces.Num(); ++i)
	{
		TestTrue(TEXT("Nunca dos rebotes en pasos seguidos del mismo contacto"), ClientBounces[i] - ClientBounces[i - 1] > 1);
	}

	// Servidor: los mismos pasos, que le llegan de dos en dos (o los tres de un paquete perdido juntos) en otro momento.
	FStepState Server;
	TArray<int32> ServerBounces;
	int32 Step = 0;
	while (Step < Steps)
	{
		const int32 Bunch = (Step % 7 == 3) ? 3 : 2;
		for (int32 k = 0; k < Bunch && Step < Steps; ++k, ++Step)
		{
			if (SimulateStep(Server, Tuning))
			{
				ServerBounces.Add(Step);
			}
		}
	}
	TestTrue(TEXT("El servidor rebota en los mismos pasos que el cliente"), ServerBounces == ClientBounces);
	TestTrue(TEXT("Y acaba en el mismo sitio: sin corrección"), Server.Feet.Equals(Client.Feet, 0.01) && Server.Velocity.Equals(Client.Velocity, 0.01));

	// Corrección justo antes del rebote: el cliente vuelve al estado del servidor y repite los pasos; el rebote se repite.
	if (ClientBounces.Num() > 0)
	{
		const int32 From = FMath::Max(0, ClientBounces[0] - 2);
		FStepState Replay = ClientStarts[From];
		TArray<int32> ReplayBounces;
		for (int32 Again = From; Again < Steps; ++Again)
		{
			if (SimulateStep(Replay, Tuning))
			{
				ReplayBounces.Add(Again);
			}
		}
		TestTrue(TEXT("Al repetir los pasos tras una corrección, rebota en el mismo paso"), ReplayBounces == ClientBounces);
		TestTrue(TEXT("Y acaba en el mismo sitio"), Replay.Feet.Equals(Client.Feet, 0.01));
	}
	return true;
}

#endif
