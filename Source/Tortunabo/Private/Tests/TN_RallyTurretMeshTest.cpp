// Torreta del buggy construida en ejecución (#435): boca en su sitio, sin meterse en la cabeza de la artillera, en el
// respaldo ni en el arco de la conductora. Headless:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Rally.Turret; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "Vehicles/TN_BuggyTurretComponent.h"
#include "Vehicles/TN_RallyTurretLogic.h"
#include "../Vehicles/TN_BuggyTurretMesh.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TNTurretMeshTest
{
	using TNProcMesh::FTNProcMeshBuffers;

	/**
	 * Medio ancho (cm) de la cabeza de la artillera en reposo cada 5 cm por delante del pivote (desde 5 cm), entre 12 cm
	 * por debajo y 30 cm por encima de él (Scripts/tools/data/turtle_geo.json a escala 2,5). Se toma el mayor de la
	 * franja y la siguiente.
	 */
	float HeadHalfWidth(double Forward)
	{
		static const float Slices[] = { 22.f, 26.f, 22.f, 24.f, 20.f, 15.f, 8.f, 5.f, 5.f, 3.f, 0.f };
		constexpr int32 Count = UE_ARRAY_COUNT(Slices);
		const int32 Index = FMath::Clamp(FMath::FloorToInt(Forward / 5.0) - 1, 0, Count - 1);
		const int32 Next = FMath::Min(Index + 1, Count - 1);
		return FMath::Max(Slices[Index], Slices[Next]);
	}

	/** Vértices que caen dentro de la cabeza (con la cabeza mirando al frente, como la torreta). */
	int32 CountInsideHead(const FTNProcMeshBuffers& Mesh)
	{
		int32 Inside = 0;
		for (const FVector& V : Mesh.Verts)
		{
			if (V.X >= 5.0 && V.X <= 55.0 && V.Z >= -12.0 && V.Z <= 30.0 && FMath::Abs(V.Y) <= HeadHalfWidth(V.X))
			{
				++Inside;
			}
		}
		return Inside;
	}

	FTNProcMeshBuffers Build(void (*Builder)(FTNProcMeshBuffers&))
	{
		FTNProcMeshBuffers Mesh;
		Builder(Mesh);
		return Mesh;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNRallyTurretMuzzleTest,
	"Tortunabo.Rally.Turret.Muzzle",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNRallyTurretMuzzleTest::RunTest(const FString& Parameters)
{
	using namespace TNTurretMeshTest;
	constexpr float Forward = UTN_BuggyTurretComponent::MuzzleDistanceCm;
	constexpr float Side = UTN_BuggyTurretComponent::MuzzleSideCm;

	// La boca de la caña: lo más adelantado está a MuzzleDistanceCm, centrado en (MuzzleSideCm, 0).
	const FTNProcMeshBuffers Barrel = Build(&TNBuggyTurretMesh::BuildBarrel);
	TestFalse(TEXT("la caña tiene malla"), Barrel.IsEmpty());
	double MaxX = -1.0e9;
	for (const FVector& V : Barrel.Verts)
	{
		MaxX = FMath::Max(MaxX, V.X);
	}
	FVector MouthCenter = FVector::ZeroVector;
	int32 MouthCount = 0;
	for (const FVector& V : Barrel.Verts)
	{
		if (V.X >= MaxX - 0.01)
		{
			MouthCenter += V;
			++MouthCount;
		}
	}
	MouthCenter /= static_cast<double>(FMath::Max(1, MouthCount));
	TestTrue(FString::Printf(TEXT("la boca está a MuzzleDistanceCm (%.2f)"), MaxX), FMath::IsNearlyEqual(MaxX, static_cast<double>(Forward), 0.01));
	TestTrue(TEXT("la boca está centrada a MuzzleSideCm a la derecha y a la altura del pivote"),
		FMath::IsNearlyEqual(MouthCenter.Y, static_cast<double>(Side), 0.1) && FMath::Abs(MouthCenter.Z) < 0.1);

	// El proyectil sale de la boca visible: la caña girada con el apuntado acaba donde dice MuzzleWorldLocation.
	const FVector Pivot(100.0, -50.0, 30.0);
	const FRotator BuggyRotation(0.f, 90.f, 0.f);
	TestTrue(TEXT("buggy girado 90° y apuntado al frente: boca delante y a la derecha del buggy"),
		TNRallyTurret::MuzzleWorldLocation(Pivot, BuggyRotation, FRotator::ZeroRotator, Forward, Side)
			.Equals(Pivot + FVector(-Side, Forward, 0.0), 0.01));
	for (const FRotator& Aim : { FRotator(30.f, 0.f, 0.f), FRotator(-10.f, 135.f, 0.f), FRotator(45.f, -170.f, 0.f) })
	{
		const FVector Expected = Pivot + (BuggyRotation.Quaternion() * Aim.Quaternion()).RotateVector(MouthCenter);
		const FVector Muzzle = TNRallyTurret::MuzzleWorldLocation(Pivot, BuggyRotation, Aim, Forward, Side);
		TestTrue(FString::Printf(TEXT("apuntado %s: la boca visible y la salida del proyectil coinciden"), *Aim.ToString()),
			Muzzle.Equals(Expected, 0.1));
		const FVector Dir = TNRallyTurret::AimWorldDirection(BuggyRotation, Aim);
		TestTrue(TEXT("la boca está a MuzzleDistanceCm por delante del pivote en la dirección del disparo"),
			FMath::IsNearlyEqual((Muzzle - Pivot) | Dir, static_cast<double>(Forward), 0.01));
	}
	// Caso negativo: un cabeceo fuera de rango se limita también para la boca (no sale de un cañón imposible).
	TestTrue(TEXT("cabeceo de 80° se limita a 45° también para la boca"),
		TNRallyTurret::MuzzleWorldLocation(FVector::ZeroVector, FRotator::ZeroRotator, FRotator(80.f, 0.f, 0.f), Forward, Side)
			.Equals(TNRallyTurret::MuzzleWorldLocation(FVector::ZeroVector, FRotator::ZeroRotator, FRotator(45.f, 0.f, 0.f), Forward, Side), 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNRallyTurretMeshClearanceTest,
	"Tortunabo.Rally.Turret.MeshClearance",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNRallyTurretMeshClearanceTest::RunTest(const FString& Parameters)
{
	using namespace TNTurretMeshTest;
	const FTNProcMeshBuffers Ring = Build(&TNBuggyTurretMesh::BuildRing);
	const FTNProcMeshBuffers Mount = Build(&TNBuggyTurretMesh::BuildMount);
	const FTNProcMeshBuffers Gun = Build(&TNBuggyTurretMesh::BuildGun);
	const FTNProcMeshBuffers Barrel = Build(&TNBuggyTurretMesh::BuildBarrel);
	for (const FTNProcMeshBuffers* Mesh : { &Ring, &Mount, &Gun, &Barrel })
	{
		TestFalse(TEXT("cada pieza tiene malla"), Mesh->IsEmpty());
		TestEqual(TEXT("buffers coherentes"), Mesh->Normals.Num(), Mesh->Verts.Num());
	}

	// Nada de la torreta dentro de la cabeza de la artillera (gira y cabecea con ella dentro de su giro máximo).
	TestEqual(TEXT("ningún vértice del cañón dentro de la cabeza"), CountInsideHead(Gun), 0);
	TestEqual(TEXT("ningún vértice de la caña dentro de la cabeza"), CountInsideHead(Barrel), 0);
	TestEqual(TEXT("ningún vértice del carro dentro de la cabeza"), CountInsideHead(Mount), 0);

	// El aro y el carro pasan por encima del respaldo de la artillera (acaba 27,4 cm bajo el pivote, 25 cm a cada lado).
	double LowestOverSeat = 1.0e9;
	for (const FTNProcMeshBuffers* Mesh : { &Ring, &Mount })
	{
		for (const FVector& V : Mesh->Verts)
		{
			if (FMath::Abs(V.Y) < 26.0 && FMath::Abs(V.X) < 45.0)
			{
				LowestOverSeat = FMath::Min(LowestOverSeat, V.Z);
			}
		}
	}
	TestTrue(FString::Printf(TEXT("aro y carro por encima del respaldo (%.1f cm)"), LowestOverSeat), LowestOverSeat > -27.0);

	// Apuntando al frente con el cabeceo mínimo, nada toca el travesaño del arco de la conductora (66 cm delante del
	// pivote, a la altura de las barandillas, 4 cm de radio).
	const FQuat Down = FRotator(TNRallyTurret::MinPitchDeg, 0.f, 0.f).Quaternion();
	double Closest = 1.0e9;
	for (const FTNProcMeshBuffers* Mesh : { &Gun, &Barrel })
	{
		for (const FVector& V : Mesh->Verts)
		{
			const FVector P = Down.RotateVector(V);
			Closest = FMath::Min(Closest, FVector2D(P.X - 66.0, P.Z - TNBuggyTurretMesh::RailZ).Size());
		}
	}
	TestTrue(FString::Printf(TEXT("cabeceo mínimo: el cañón no toca el arco de la conductora (%.1f cm del eje)"), Closest), Closest > 4.5);

	// Los tirantes llegan a las barandillas.
	double RingReach = 0.0;
	for (const FVector& V : Ring.Verts)
	{
		RingReach = FMath::Max(RingReach, FMath::Abs(V.Y));
	}
	TestTrue(TEXT("los tirantes llegan a las barandillas"), RingReach >= TNBuggyTurretMesh::RailY);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
