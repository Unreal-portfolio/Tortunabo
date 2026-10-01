// Carrocería de tortuga del buggy del Rally (#297), generada en C++ (TN_BuggyArt): cada pieza de cada modelo tiene
// geometría, nombre estable, códigos de zona válidos para M_BuggyPaint y medidas que casan con el chasis de Chaos, y deja
// libres el barrido del cañón de la torreta y las cabezas de las dos tortugas. Sin mundo ni assets:
//   UnrealEditor-Win64-DebugGame-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Rally.Buggy.Art; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "Vehicles/TN_Buggy.h"
#include "Vehicles/TN_BuggyCosmetics.h"
#include "Vehicles/TN_BuggyTurretComponent.h"
#include "Vehicles/TN_RallyTurretLogic.h"
#include "../Vehicles/TN_BuggyArt.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TNBuggyArtTest
{
	const ETNBuggyBodyStyle Styles[] = { ETNBuggyBodyStyle::Classic, ETNBuggyBodyStyle::Offroad, ETNBuggyBodyStyle::Racer };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNBuggyArtPiecesTest,
	"Tortunabo.Rally.Buggy.Art.Pieces",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNBuggyArtPiecesTest::RunTest(const FString& Parameters)
{
	using namespace TNBuggyArt;
	namespace F = TNBuggyArt::Frame;
	TestEqual(TEXT("Asiento de la conductora igual que ATN_Buggy"), F::DriverSeat, FVector(ATN_Buggy::DriverSeatLocal));
	TestEqual(TEXT("Asiento de la artillera igual que ATN_Buggy"), F::GunnerSeat, FVector(ATN_Buggy::GunnerSeatLocal));
	TestEqual(TEXT("Pivote de la torreta"), F::TurretPivotZ, static_cast<double>(ATN_Buggy::GunnerSeatLocal.Z + UTN_BuggyTurretComponent::PivotAboveSeatCm), 0.01);
	TestEqual(TEXT("Largo del cañón"), F::CannonLength, static_cast<double>(UTN_BuggyTurretComponent::MuzzleDistanceCm), 0.01);
	TestEqual(TEXT("Cabeceo mínimo del cañón"), F::CannonMinPitchDeg, static_cast<double>(TNRallyTurret::MinPitchDeg), 0.01);

	const double SinPitch = FMath::Sin(FMath::DegreesToRadians(-F::CannonMinPitchDeg));
	const FVector2D Pivot(F::GunnerSeat.X, F::GunnerSeat.Y);
	TSet<FName> Names;
	for (const ETNBuggyBodyStyle Style : TNBuggyArtTest::Styles)
	{
		int32 BodyTris = 0;
		for (int32 p = 0; p < static_cast<int32>(EPiece::Count); ++p)
		{
			const EPiece Piece = static_cast<EPiece>(p);
			const FName Name = PieceName(Style, Piece);
			const FString Who = Name.ToString();
			TestTrue(Who + TEXT(": nombre estable Rally.Buggy.<Pieza>.<Modelo>"), Who.StartsWith(TEXT("Rally.Buggy.")));
			TestFalse(Who + TEXT(": nombre repetido"), Names.Contains(Name));
			Names.Add(Name);
			const TNProcMesh::FTNProcMeshBuffers B = BuildPiece(Style, Piece);
			TestFalse(Who + TEXT(": tiene geometría"), B.IsEmpty());
			const int32 N = B.Verts.Num();
			TestTrue(Who + TEXT(": buffers del mismo tamaño"), B.Normals.Num() == N && B.Colors.Num() == N && B.UVs.Num() == N);
			TestEqual(Who + TEXT(": triángulos completos"), B.Tris.Num() % 3, 0);
			bool bIndices = true;
			for (const int32 T : B.Tris) { bIndices &= T >= 0 && T < N; }
			TestTrue(Who + TEXT(": índices dentro"), bIndices);
			bool bZones = true;
			for (const FLinearColor& C : B.Colors)
			{
				// Octavos: 0 mate, 2 metal, 4 luz, 6 equipo, 7 pintura sin dibujo, 8 pintura.
				const float Code = C.A * 8.f;
				const int32 Rounded = FMath::RoundToInt(Code);
				bZones &= FMath::Abs(Code - Rounded) < 0.02f && (Rounded == 0 || Rounded == 2 || Rounded == 4 || Rounded == 6 || Rounded == 7 || Rounded == 8);
				bZones &= C.R >= 0.f && C.R <= 1.f && C.G >= 0.f && C.G <= 1.f && C.B >= 0.f && C.B <= 1.f;
			}
			TestTrue(Who + TEXT(": códigos de zona de M_BuggyPaint"), bZones);
			const bool bBody = p < NumBodyPieces;
			if (bBody) { BodyTris += B.Tris.Num() / 3; }
			FBox Box(ForceInit);
			int32 InSweep = 0;
			int32 InDriver = 0;
			int32 InGunner = 0;
			for (const FVector& V : B.Verts)
			{
				Box += V;
				if (!bBody) { continue; }
				// Barrido del cañón: a la distancia horizontal D del pivote, el cañón más bajo (cabeceo mínimo) pasa por
				// Z = pivote - D sen(10°), con su radio y un margen.
				const double D = FVector2D::Distance(FVector2D(V.X, V.Y), Pivot);
				if (D <= F::CannonLength + F::CannonRadius && V.Z > F::TurretPivotZ - D * SinPitch - F::CannonRadius - 0.5) { ++InSweep; }
				// Hueco de la conductora (de los hombros arriba) y de la artillera, de pie sobre su sillín.
				if (V.X > 4.0 && V.X < 46.0 && V.Y > -58.0 && V.Y < -12.0 && V.Z > 100.0 && V.Z < 175.0) { ++InDriver; }
				if (V.X > -98.0 && V.X < -62.0 && FMath::Abs(V.Y) < 18.0 && V.Z > F::GunnerSeat.Z + 1.0 && V.Z < 215.0) { ++InGunner; }
			}
			if (bBody)
			{
				TestEqual(Who + TEXT(": deja libre el barrido del cañón"), InSweep, 0);
				TestEqual(Who + TEXT(": deja libre la cabeza de la conductora"), InDriver, 0);
				TestEqual(Who + TEXT(": deja libre a la artillera"), InGunner, 0);
				TestTrue(Who + TEXT(": dentro del largo del buggy"), Box.Min.X > -265.0 && Box.Max.X < 265.0);
				TestTrue(Who + TEXT(": dentro del ancho"), Box.Min.Y > -178.0 && Box.Max.Y < 178.0);
				TestTrue(Who + TEXT(": por encima del suelo"), Box.Min.Z > 25.0);
				TestTrue(Who + TEXT(": por debajo de la artillera"), Box.Max.Z < 175.0);
			}
			else if (Piece == EPiece::Wheel)
			{
				const double Radius = FMath::Max(FMath::Max(FMath::Abs(Box.Min.X), Box.Max.X), FMath::Max(FMath::Abs(Box.Min.Z), Box.Max.Z));
				TestTrue(Who + TEXT(": radio de la rueda de Chaos"), Radius > F::WheelRadius - 1.0 && Radius < F::WheelRadius + 1.5);
				TestTrue(Who + TEXT(": ancho de la rueda de Chaos"), Box.Max.Y - Box.Min.Y < F::WheelWidth + 1.0);
				TestTrue(Who + TEXT(": la llanta mira a +Y"), Box.Max.Y > 16.5);
				TestTrue(Who + TEXT(": presupuesto de la rueda"), B.Tris.Num() / 3 < 2500);
			}
			else if (Piece == EPiece::Cannon)
			{
				TestTrue(Who + TEXT(": a lo largo de +X desde el pivote"), Box.Min.X > -1.0 && FMath::IsNearlyEqual(Box.Max.X, F::CannonLength, 1.0));
				TestTrue(Who + TEXT(": grosor"), Box.Max.Z < F::CannonRadius + 1.0);
			}
		}
		TestTrue(FString::Printf(TEXT("Carrocería %d: presupuesto de triángulos"), static_cast<int32>(Style)), BodyTris > 1500 && BodyTris < 16000);
		const FVector Mount = AntennaMount(Style);
		TestTrue(FString::Printf(TEXT("Carrocería %d: la antena fuera del barrido del cañón"), static_cast<int32>(Style)),
			FVector2D::Distance(FVector2D(Mount.X, Mount.Y), Pivot) > F::CannonLength + 30.0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
