// Túnel en capas (TN_TerrainTunnelDecisions.h): techo y bóveda cosidos al suelo en una
// sola malla, sin piezas aparte.
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.TerrainTunnel; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "World/TN_TerrainBiomeDecisions.h"
#include "World/TN_TerrainModuleDecisions.h"
#include "World/TN_TerrainTunnelDecisions.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr int32 TunnelTestResolution = 21;
	constexpr double TunnelTestSize = 2000.0;     // 100 uu entre vértices
	constexpr int32 TunnelTestZero = 32768;
	constexpr int32 UnitsPerMeter = 400;          // HeightScale 0,25 uu

	/** Huella: filas 5..15 (a lo largo del pasillo), columnas 6..14 (anillo lateral en 6 y 14). */
	struct FTunnelTestData
	{
		TArray<uint16> Floor;
		TArray<uint16> Roof;
		TArray<uint16> Ceiling;
	};

	FTunnelTestData MakeTunnelTestData()
	{
		const int32 R = TunnelTestResolution;
		FTunnelTestData Data;
		Data.Floor.Init(TunnelTestZero, R * R);
		Data.Roof.Init(0, R * R);
		Data.Ceiling.Init(0, R * R);
		for (int32 I = 5; I <= 15; ++I)
		{
			for (int32 J = 6; J <= 14; ++J)
			{
				const int32 K = I * R + J;
				const double Across = FMath::Abs(J - 10) / 4.0;                   // 0 en el eje, 1 en el anillo
				const double Vault = 5.0 * FMath::Sqrt(FMath::Max(0.0, 1.0 - Across * Across));
				const double Top = 10.0 * (1.0 - Across * Across);
				Data.Roof[K] = static_cast<uint16>(TunnelTestZero + FMath::RoundToInt(Top * UnitsPerMeter));
				Data.Ceiling[K] = static_cast<uint16>(TunnelTestZero + FMath::RoundToInt(FMath::Min(Vault, Top) * UnitsPerMeter));
			}
		}
		return Data;
	}

	TNTerrainModule::FModuleField TunnelTestField(const TArray<uint16>& Heights)
	{
		TNTerrainModule::FModuleField Field;
		Field.Resolution = TunnelTestResolution;
		Field.Size = TunnelTestSize;
		Field.Heights = Heights;
		Field.HeightScale = 0.25;
		Field.HeightZero = TunnelTestZero;
		return Field;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNTerrainTunnelLayeredTest,
	"Tortunabo.TerrainTunnel.LayeredMesh",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNTerrainTunnelLayeredTest::RunTest(const FString& Parameters)
{
	const int32 R = TunnelTestResolution;
	const FTunnelTestData Data = MakeTunnelTestData();
	const TNTerrainModule::FModuleField Field = TunnelTestField(Data.Floor);
	const TNTerrainModule::FModuleColors Colors = TNTerrainBiome::ColorsFor(ETNTerrainBiome::Sand, -400.0);
	const TNGridTerrain::FTileMesh Floor = TNTerrainModule::BuildModuleMesh(Field, Colors);

	TNGridTerrain::FTileMesh Mesh = Floor;
	const TNTerrainTunnel::FTunnelLayers Layers{ Data.Roof, Data.Ceiling, Data.Floor };
	TestTrue(TEXT("se cose"), TNTerrainTunnel::AppendTunnelLayers(Mesh, Field, Layers, Colors));
	TestTrue(TEXT("añade vértices de techo y bóveda"), Mesh.Vertices.Num() > R * R);
	TestEqual(TEXT("mismos arrays por vértice"), Mesh.Normals.Num(), Mesh.Vertices.Num());
	TestEqual(TEXT("colores por vértice"), Mesh.Colors.Num(), Mesh.Vertices.Num());

	// El suelo queda intacto: mismos vértices y los triángulos del suelo al principio.
	bool bFloorKept = true;
	for (int32 Index = 0; Index < R * R; ++Index)
	{
		bFloorKept &= Mesh.Vertices[Index].Equals(Floor.Vertices[Index]);
	}
	for (int32 Index = 0; Index < Floor.Triangles.Num(); ++Index)
	{
		bFloorKept &= Mesh.Triangles[Index] == Floor.Triangles[Index];
	}
	TestTrue(TEXT("el suelo no cambia"), bFloorKept);

	// El casquete (techo + bóveda + bocas) es cerrado y orientable: cada arista dirigida
	// aparece una vez y su inversa otra.
	TMap<TPair<int32, int32>, int32> Directed;
	for (int32 T = Floor.Triangles.Num(); T < Mesh.Triangles.Num(); T += 3)
	{
		for (int32 E = 0; E < 3; ++E)
		{
			Directed.FindOrAdd(TPair<int32, int32>(Mesh.Triangles[T + E], Mesh.Triangles[T + (E + 1) % 3]))++;
		}
	}
	int32 Open = 0;
	int32 Duplicated = 0;
	for (const TPair<TPair<int32, int32>, int32>& Edge : Directed)
	{
		Duplicated += Edge.Value > 1 ? 1 : 0;
		Open += Directed.Contains(TPair<int32, int32>(Edge.Key.Value, Edge.Key.Key)) ? 0 : 1;
	}
	TestTrue(TEXT("hay casquete"), Directed.Num() > 0);
	TestEqual(TEXT("sin aristas abiertas"), Open, 0);
	TestEqual(TEXT("sin aristas repetidas en el mismo sentido"), Duplicated, 0);

	// Bajo la clave de la bóveda hay paso: 5 m sobre el suelo en el eje.
	double MinClearance = TNumericLimits<double>::Max();
	double MinTopOnAxis = TNumericLimits<double>::Max();
	for (int32 Index = R * R; Index < Mesh.Vertices.Num(); ++Index)
	{
		const FVector& V = Mesh.Vertices[Index];
		const int32 J = FMath::RoundToInt((V.Y + TunnelTestSize * 0.5) / (TunnelTestSize / (R - 1)));
		if (J != 10) { continue; }
		if (Mesh.Normals[Index].Z < 0.0) { MinClearance = FMath::Min(MinClearance, V.Z); }
		else { MinTopOnAxis = FMath::Min(MinTopOnAxis, V.Z); }
	}
	TestTrue(TEXT("bóveda a 5 m en el eje"), FMath::IsNearlyEqual(MinClearance, 500.0, 1.0));
	TestTrue(TEXT("techo por encima de la bóveda"), MinTopOnAxis > MinClearance);

	// La cara del techo mira hacia arriba y la de la bóveda hacia abajo (cara visible de
	// Unreal = opuesta a (B-A)x(C-A)).
	int32 WrongFacing = 0;
	for (int32 T = Floor.Triangles.Num(); T < Mesh.Triangles.Num(); T += 3)
	{
		const FVector& A = Mesh.Vertices[Mesh.Triangles[T]];
		const FVector& B = Mesh.Vertices[Mesh.Triangles[T + 1]];
		const FVector& C = Mesh.Vertices[Mesh.Triangles[T + 2]];
		const FVector Visible = -FVector::CrossProduct(B - A, C - A);
		const FVector AverageNormal = Mesh.Normals[Mesh.Triangles[T]] + Mesh.Normals[Mesh.Triangles[T + 1]] + Mesh.Normals[Mesh.Triangles[T + 2]];
		if (FMath::Abs(Visible.Z) > FMath::Abs(Visible.X) + FMath::Abs(Visible.Y) && FVector::DotProduct(Visible, AverageNormal) < 0.0)
		{
			++WrongFacing;
		}
	}
	TestEqual(TEXT("techo hacia arriba y bóveda hacia abajo"), WrongFacing, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNTerrainTunnelRejectTest,
	"Tortunabo.TerrainTunnel.Reject",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNTerrainTunnelRejectTest::RunTest(const FString& Parameters)
{
	const FTunnelTestData Data = MakeTunnelTestData();
	const TNTerrainModule::FModuleField Field = TunnelTestField(Data.Floor);
	const TNTerrainModule::FModuleColors Colors = TNTerrainBiome::ColorsFor(ETNTerrainBiome::Sand, -400.0);
	const TNGridTerrain::FTileMesh Floor = TNTerrainModule::BuildModuleMesh(Field, Colors);

	// Capas de otro tamaño: no se tocan ni la malla ni sus arrays.
	TArray<uint16> Short = Data.Roof;
	Short.Pop();
	TNGridTerrain::FTileMesh Mesh = Floor;
	TestFalse(TEXT("capas de tamaño equivocado"),
		TNTerrainTunnel::AppendTunnelLayers(Mesh, Field, { Short, Data.Ceiling, Data.Floor }, Colors));
	TestEqual(TEXT("malla intacta"), Mesh.Vertices.Num(), Floor.Vertices.Num());

	// Malla que no es la del campo (p. ej. la fundida con vecinos): tampoco.
	TNGridTerrain::FTileMesh Other = Floor;
	Other.Vertices.Pop();
	TestFalse(TEXT("malla ajena"), TNTerrainTunnel::AppendTunnelLayers(Other, Field, { Data.Roof, Data.Ceiling, Data.Floor }, Colors));

	// Sin huella (todo 0): se cose pero no añade nada.
	TArray<uint16> Empty;
	Empty.Init(0, Data.Roof.Num());
	TNGridTerrain::FTileMesh Bare = Floor;
	TestTrue(TEXT("sin techo"), TNTerrainTunnel::AppendTunnelLayers(Bare, Field, { Empty, Empty, Data.Floor }, Colors));
	TestEqual(TEXT("sin vértices nuevos"), Bare.Vertices.Num(), Floor.Vertices.Num());
	TestEqual(TEXT("sin triángulos nuevos"), Bare.Triangles.Num(), Floor.Triangles.Num());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
