// Reglas puras del Rally (Rally/TN_RallyLogic.h), las mismas que usan ATN_RallyGameMode, ATN_RallyTrack y el piloto IA.
// Correr desde Session Frontend (categoría "Tortunabo.Rally") o headless:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Rally; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Rally/TN_RallyLogic.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TNRallyTestHelpers
{
	static TNRally::FStandingKey Key(int32 Id, int32 Lap, int32 Gates, double Progress, bool bFinished = false, double Time = 0.0)
	{
		TNRally::FStandingKey Result;
		Result.Id = Id;
		Result.Lap = Lap;
		Result.GatesPassed = Gates;
		Result.SegmentProgressCm = Progress;
		Result.bFinished = bFinished;
		Result.FinishTime = Time;
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNRallyStandingsTest, "Tortunabo.Rally.Logic.Standings",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNRallyStandingsTest::RunTest(const FString& Parameters)
{
	using namespace TNRally;
	using TNRallyTestHelpers::Key;

	TArray<FStandingKey> Keys;
	Keys.Add(Key(0, 1, 3, 500.0));              // vuelta 1, 3 puertas
	Keys.Add(Key(1, 2, 9, 100.0));              // vuelta 2: por delante aunque tenga menos arco
	Keys.Add(Key(2, 1, 3, 900.0));              // misma puerta que el 0, más arco
	Keys.Add(Key(3, 2, 15, 0.0, true, 95.0));   // terminado, más lento
	Keys.Add(Key(4, 2, 15, 0.0, true, 90.0));   // terminado, más rápido
	Keys.Add(Key(5, 1, 3, 500.0));              // empate exacto con el 0: menor Id primero
	FStandingKey Retired = Key(6, 2, 12, 0.0);
	Retired.bRetired = true;
	Keys.Add(Retired);

	const TArray<int32> Order = SortStandings(Keys);
	const TArray<int32> Expected = { 4, 3, 1, 2, 0, 5, 6 };
	TestEqual(TEXT("Terminados por tiempo, luego vuelta, puerta, arco e Id; retirados al final"), Order, Expected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNRallyGateTest, "Tortunabo.Rally.Logic.GateOrderAndShortcut",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNRallyGateTest::RunTest(const FString& Parameters)
{
	using namespace TNRally;
	TestTrue(TEXT("Siguiente puerta, hacia delante, recorrido completo"), CheckGate(2, 2, true, 20000.0, 20000.0, true) == EGateCheck::Valid);
	TestTrue(TEXT("Saltarse una puerta no cuenta"), CheckGate(3, 2, true, 40000.0, 20000.0, true) == EGateCheck::WrongGate);
	TestTrue(TEXT("Cruzarla al revés no cuenta"), CheckGate(2, 2, false, 20000.0, 20000.0, true) == EGateCheck::WrongDirection);
	TestTrue(TEXT("Menos del 60 % de la spline es atajo"), CheckGate(2, 2, true, 11999.0, 20000.0, true) == EGateCheck::Shortcut);
	TestTrue(TEXT("Justo el 60 % vale"), CheckGate(2, 2, true, 12000.0, 20000.0, true) == EGateCheck::Valid);
	TestTrue(TEXT("En la salida no se aplica la regla del 60 %"), CheckGate(0, 0, true, 10.0, 20000.0, false) == EGateCheck::Valid);

	// Cruce geométrico: puerta en el origen mirando a +X, 24 × 10 m (semiextensiones 12 y 5 m).
	const FTransform Gate(FRotator::ZeroRotator, FVector::ZeroVector);
	const FVector Half(200.0, 1200.0, 500.0);
	double Alpha = 0.0;
	bool bForward = false;
	TestTrue(TEXT("Atraviesa por el centro"), SegmentCrossesGate(FVector(-100, 0, 0), FVector(300, 0, 0), Gate, Half, Alpha, bForward));
	TestTrue(TEXT("Hacia delante"), bForward);
	TestEqual(TEXT("Fracción del cruce"), Alpha, 0.25, 1e-9);
	TestTrue(TEXT("Al revés también se detecta"), SegmentCrossesGate(FVector(300, 0, 0), FVector(-100, 0, 0), Gate, Half, Alpha, bForward));
	TestFalse(TEXT("...pero no es hacia delante"), bForward);
	TestFalse(TEXT("Por fuera del ancho no cruza"), SegmentCrossesGate(FVector(-100, 1300, 0), FVector(100, 1300, 0), Gate, Half, Alpha, bForward));
	TestFalse(TEXT("Por encima del alto no cruza"), SegmentCrossesGate(FVector(-100, 0, 600), FVector(100, 0, 600), Gate, Half, Alpha, bForward));
	TestFalse(TEXT("Sin llegar al plano no cruza"), SegmentCrossesGate(FVector(-300, 0, 0), FVector(-100, 0, 0), Gate, Half, Alpha, bForward));
	const FTransform Turned(FRotator(0.0, 90.0, 0.0), FVector(1000, 0, 0));
	TestTrue(TEXT("Puerta girada 90°: se cruza hacia +Y"), SegmentCrossesGate(FVector(1000, -50, 0), FVector(1000, 50, 0), Turned, Half, Alpha, bForward) && bForward);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNRallyLapsTest, "Tortunabo.Rally.Logic.Laps",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNRallyLapsTest::RunTest(const FString& Parameters)
{
	using namespace TNRally;
	FLapRules Circuit;
	Circuit.NumGates = 7;
	Circuit.Laps = 2;
	Circuit.bCircuit = true;
	TestEqual(TEXT("Circuito 7 puertas × 2 vueltas: salida + 14"), Circuit.GatesToFinish(), 15);
	TestEqual(TEXT("Antes de la salida, vuelta 0"), Circuit.LapForGates(0), 0);
	TestEqual(TEXT("Antes de la salida toca la puerta 0"), Circuit.NextGateIndex(0), 0);
	TestEqual(TEXT("Tras la salida, vuelta 1"), Circuit.LapForGates(1), 1);
	TestEqual(TEXT("Tras la salida toca la 1"), Circuit.NextGateIndex(1), 1);
	TestEqual(TEXT("Con 7 puertas la meta (0) es la siguiente"), Circuit.NextGateIndex(7), 0);
	TestEqual(TEXT("Sigue en la vuelta 1 hasta cruzar la meta"), Circuit.LapForGates(7), 1);
	TestEqual(TEXT("Cruzar la meta abre la vuelta 2"), Circuit.LapForGates(8), 2);
	TestFalse(TEXT("A una puerta de acabar no ha terminado"), Circuit.IsFinished(14));
	TestTrue(TEXT("Meta de la última vuelta"), Circuit.IsFinished(15));
	TestEqual(TEXT("La vuelta no pasa de Laps"), Circuit.LapForGates(15), 2);
	TestEqual(TEXT("Reaparición en la última puerta validada"), Circuit.LastGateIndex(9), 1);
	TestEqual(TEXT("Sin puertas validadas no hay puerta de reaparición"), Circuit.LastGateIndex(0), -1);

	FLapRules PointToPoint;
	PointToPoint.NumGates = 5;
	PointToPoint.Laps = 3;
	PointToPoint.bCircuit = false;
	TestEqual(TEXT("Punto a punto: meta en la última puerta, sin vueltas"), PointToPoint.GatesToFinish(), 5);
	TestEqual(TEXT("Punto a punto: vuelta 1 tras la salida"), PointToPoint.LapForGates(3), 1);
	TestEqual(TEXT("Punto a punto: siguiente puerta"), PointToPoint.NextGateIndex(3), 3);
	TestTrue(TEXT("Punto a punto: terminado en la meta"), PointToPoint.IsFinished(5));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNRallyWrongWayTest, "Tortunabo.Rally.Logic.WrongWay",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNRallyWrongWayTest::RunTest(const FString& Parameters)
{
	using namespace TNRally;
	FWrongWayState State;
	int32 Warnings = 0;
	bool bTurned = false;
	double TurnedAt = 0.0;
	double Time = 0.0;
	for (int32 Step = 0; Step < 25 && !bTurned; ++Step)
	{
		Time += 0.2;
		const EWrongWayEvent Event = UpdateWrongWay(State, -0.9, 40.0, 0.2);
		if (Event == EWrongWayEvent::WarningOn)
		{
			++Warnings;
			TestTrue(TEXT("El aviso sale a los 1,5 s"), FMath::IsNearlyEqual(Time, 1.6, 0.21));
		}
		if (Event == EWrongWayEvent::TurnAround)
		{
			bTurned = true;
			TurnedAt = Time;
		}
	}
	TestEqual(TEXT("Un solo aviso"), Warnings, 1);
	TestTrue(TEXT("A los 4 s el servidor lo gira"), bTurned && FMath::IsNearlyEqual(TurnedAt, 4.0, 0.21));
	TestFalse(TEXT("Tras girarlo el estado se reinicia"), State.bWarning);

	FWrongWayState Slow;
	for (int32 Step = 0; Step < 30; ++Step)
	{
		TestTrue(TEXT("Marcha atrás a menos de 20 km/h no dispara"), UpdateWrongWay(Slow, -1.0, 15.0, 0.2) == EWrongWayEvent::None);
	}
	FWrongWayState Side;
	for (int32 Step = 0; Step < 30; ++Step)
	{
		TestTrue(TEXT("De lado (dot -0,3) no es contramano"), UpdateWrongWay(Side, -0.3, 60.0, 0.2) == EWrongWayEvent::None);
	}
	FWrongWayState Fix;
	for (int32 Step = 0; Step < 10; ++Step) { UpdateWrongWay(Fix, -0.9, 40.0, 0.2); }
	TestTrue(TEXT("Con 2 s en contramano está avisado"), Fix.bWarning);
	TestTrue(TEXT("Al corregir se apaga el aviso"), UpdateWrongWay(Fix, 0.9, 40.0, 0.2) == EWrongWayEvent::WarningOff);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNRallyStuckOffTrackTest, "Tortunabo.Rally.Logic.StuckAndOffTrack",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNRallyStuckOffTrackTest::RunTest(const FString& Parameters)
{
	using namespace TNRally;
	FStuckState Stuck;
	bool bStuck = false;
	int32 Steps = 0;
	while (!bStuck && Steps < 100)
	{
		// Se mueve 2 cm por paso: casi parado.
		bStuck = UpdateStuck(Stuck, FVector(2.0 * Steps, 0, 0), 0.2);
		++Steps;
	}
	TestTrue(TEXT("Casi parado 8 s es atasco"), bStuck);
	TestTrue(TEXT("...a los 8 s (+ la muestra de anclaje)"), Steps >= 40 && Steps <= 42);

	FStuckState Moving;
	bool bFalsePositive = false;
	for (int32 Step = 0; Step < 200; ++Step)
	{
		bFalsePositive |= UpdateStuck(Moving, FVector(300.0 * Step, 0, 0), 0.2);
	}
	TestFalse(TEXT("Avanzando 15 m/s no se atasca"), bFalsePositive);

	FOffTrackState Off;
	TestFalse(TEXT("A 39 m del eje sigue en pista"), UpdateOffTrack(Off, 3900.0, 5.0));
	TestFalse(TEXT("A 41 m, el primer medio segundo aún no"), UpdateOffTrack(Off, 4100.0, 0.5));
	TestTrue(TEXT("A 41 m durante 1 s, fuera de pista"), UpdateOffTrack(Off, 4100.0, 0.5));
	TestFalse(TEXT("Volver a la pista reinicia"), UpdateOffTrack(Off, 1000.0, 0.2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNRallyPointsAmmoTest, "Tortunabo.Rally.Logic.PointsAndAmmo",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNRallyPointsAmmoTest::RunTest(const FString& Parameters)
{
	using namespace TNRally;
	const int32 ExpectedPoints[] = { 10, 8, 6, 5, 4, 3, 2, 1 };
	for (int32 Place = 1; Place <= 8; ++Place)
	{
		TestEqual(FString::Printf(TEXT("Puntos del %d.º"), Place), PointsForPlace(Place, true), ExpectedPoints[Place - 1]);
	}
	TestEqual(TEXT("9.º no puntúa"), PointsForPlace(9, true), 0);
	TestEqual(TEXT("Sin llegar no puntúa"), PointsForPlace(1, false), 0);

	const FAmmoWeights First = AmmoWeightsForPlace(1, 8);
	const FAmmoWeights Last = AmmoWeightsForPlace(8, 8);
	TestTrue(TEXT("El primero saca más Alga que el último"), First.Alga > Last.Alga);
	TestTrue(TEXT("El primero saca más Tinta que el último"), First.Tinta > Last.Tinta);
	TestTrue(TEXT("El último saca más Mortero que el primero"), Last.Mortero > First.Mortero);
	TestTrue(TEXT("El último saca más Burbuja que el primero"), Last.Burbuja > First.Burbuja);
	TestTrue(TEXT("El último: Mortero es lo más probable"), Last.Mortero >= Last.Alga && Last.Mortero >= Last.Tinta && Last.Mortero >= Last.Burbuja);
	TestTrue(TEXT("El primero: Alga es lo más probable"), First.Alga >= First.Mortero && First.Alga >= First.Tinta && First.Alga >= First.Burbuja);

	// Frecuencias con tiradas uniformes: el reparto sale en proporción a los pesos.
	int32 LastMortero = 0;
	int32 FirstMortero = 0;
	constexpr int32 Rolls = 1000;
	for (int32 Index = 0; Index < Rolls; ++Index)
	{
		const float Roll = (Index + 0.5f) / Rolls;
		LastMortero += PickAmmo(Last, Roll) == ETNRallyAmmo::Mortero ? 1 : 0;
		FirstMortero += PickAmmo(First, Roll) == ETNRallyAmmo::Mortero ? 1 : 0;
	}
	TestTrue(TEXT("El último recibe Mortero al menos el triple de veces que el primero"), LastMortero >= 3 * FirstMortero);
	TestTrue(TEXT("Nunca sale Coco ni None de una caja"), PickAmmo(First, 0.f) != ETNRallyAmmo::None && PickAmmo(First, 1.f) != ETNRallyAmmo::Coco);
	TestEqual(TEXT("Alga: 2 cargas"), ChargesFor(ETNRallyAmmo::Alga), 2);
	TestEqual(TEXT("Burbuja: 1 carga"), ChargesFor(ETNRallyAmmo::Burbuja), 1);
	TestEqual(TEXT("Mortero: 1 carga"), ChargesFor(ETNRallyAmmo::Mortero), 1);
	TestEqual(TEXT("Tinta: 2 cargas"), ChargesFor(ETNRallyAmmo::Tinta), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNRallyArcWindowTest, "Tortunabo.Rally.Logic.ArcWindow",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNRallyArcWindowTest::RunTest(const FString& Parameters)
{
	using namespace TNRally;
	// Circunferencia de 100 m de radio.
	const double Radius = 10000.0;
	const double Length = 2.0 * UE_DOUBLE_PI * Radius;
	auto Circle = [Radius](double S)
	{
		const double Angle = S / Radius;
		return FVector(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), 0.0);
	};
	const FVector Point = Circle(5000.0) * 1.05;
	const double Found = FindArcInWindow(Circle, Length, true, Point, 4500.0, ArcWindowBehindCm, ArcWindowAheadCm, 200.0);
	TestEqual(TEXT("Arco más cercano en la ventana"), Found, 5000.0, 5.0);

	const double Wrapped = FindArcInWindow(Circle, Length, true, Circle(100.0), Length - 300.0, ArcWindowBehindCm, ArcWindowAheadCm, 200.0);
	TestEqual(TEXT("Cruzar la meta envuelve el arco"), Wrapped, 100.0, 5.0);

	// Lazo: una recta que vuelve por encima de sí misma a 10 m de altura (paso superior). El buggy va por abajo en s = 3000;
	// el tramo de arriba (s ≈ 17000) está más cerca en planta pero fuera de la ventana: no salta de arco.
	auto Loop = [](double S)
	{
		if (S <= 10000.0) { return FVector(S, 0.0, 0.0); }
		return FVector(20000.0 - S, 0.0, 1000.0);
	};
	const double Under = FindArcInWindow(Loop, 20000.0, false, FVector(3000.0, 0.0, 900.0), 2800.0, ArcWindowBehindCm, ArcWindowAheadCm, 200.0);
	TestEqual(TEXT("Bajo un paso superior se queda en su tramo"), Under, 3000.0, 5.0);

	TestEqual(TEXT("ForwardArc da la vuelta en circuito"), ForwardArc(Length - 100.0, 100.0, Length, true), 200.0, 1e-6);
	TestEqual(TEXT("WrapArc recorta en punto a punto"), WrapArc(-50.0, 1000.0, false), 0.0, 1e-9);

	const FVector2D Slot0 = GridSlotOffset(0);
	const FVector2D Slot7 = GridSlotOffset(7);
	TestEqual(TEXT("Parrilla: primera fila a 10 m"), Slot0.X, GridFirstRowBackCm, 1e-9);
	TestEqual(TEXT("Parrilla: cuarta fila a 10 + 3 × 8 m"), Slot7.X, GridFirstRowBackCm + 3.0 * GridRowSpacingCm, 1e-9);
	TestTrue(TEXT("Parrilla: dos columnas"), Slot0.Y < 0.0 && Slot7.Y > 0.0);

	TestTrue(TEXT("IA: objetivo a la derecha → dirección positiva"), SteerToward(FVector::ForwardVector, FVector(1, 1, 0), 45.f) > 0.99f);
	TestTrue(TEXT("IA: objetivo a la izquierda → dirección negativa"), SteerToward(FVector::ForwardVector, FVector(1, -0.2, 0), 45.f) < 0.f);
	TestTrue(TEXT("IA: frena más en curva cerrada"), CornerSpeedKmh(FVector(1, 0, 0), FVector(0, 1, 0), 90.f, 35.f) < CornerSpeedKmh(FVector(1, 0, 0), FVector(1, 0.1, 0), 90.f, 35.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNRallyManifestTest, "Tortunabo.Rally.Logic.ManifestI03R",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNRallyManifestTest::RunTest(const FString& Parameters)
{
	using namespace TNRally;
	FString Text;
	const FString Path = VariantManifestPath(TEXT("I03R_tortuga_magna"));
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		AddError(FString::Printf(TEXT("No se puede leer %s"), *Path));
		return false;
	}
	FTrackSource Source;
	FString Error;
	TestTrue(TEXT("El manifest de I03R se lee"), ParseTrackManifest(Text, Source, Error));
	TestEqual(TEXT("7 checkpoints"), Source.Checkpoints.Num(), 7);
	TestTrue(TEXT("Trae la cota del agua"), Source.bHasWater);
	bool bCircuit = false;
	const TArray<FGateDef> Gates = BuildGateList(Source, bCircuit);
	TestTrue(TEXT("I03R es un circuito (start_uu == end_uu)"), bCircuit);
	TestEqual(TEXT("7 puertas: el primer checkpoint ya es la salida"), Gates.Num(), 7);
	if (Gates.Num() == 7)
	{
		TestTrue(TEXT("La puerta 0 está en la salida"), FVector::Dist(Gates[0].Location, Source.Start) < 100.0);
		TestEqual(TEXT("Rumbo de la puerta 1 (del manifest)"), Gates[1].YawDeg, -44.0, 1e-6);
	}

	// Punto a punto sin checkpoints (como E01_espana): salida y meta.
	FTrackSource PointToPoint;
	TestTrue(TEXT("Solo start_uu y end_uu se lee"), ParseTrackManifest(
		TEXT("{\"start_uu\":[0,0,0],\"end_uu\":[100000,0,0],\"water_uu\":-400}"), PointToPoint, Error));
	bool bP2PCircuit = true;
	const TArray<FGateDef> P2PGates = BuildGateList(PointToPoint, bP2PCircuit);
	TestFalse(TEXT("Salida y meta distintas: punto a punto"), bP2PCircuit);
	TestEqual(TEXT("Punto a punto: salida y meta"), P2PGates.Num(), 2);
	if (P2PGates.Num() == 2)
	{
		TestEqual(TEXT("Rumbo deducido hacia la meta"), P2PGates[0].YawDeg, 0.0, 1e-6);
	}
	FTrackSource Broken;
	TestFalse(TEXT("JSON sin puertas ni salida no vale"), ParseTrackManifest(TEXT("{\"water_uu\":0}"), Broken, Error));
	return true;
}

#endif
