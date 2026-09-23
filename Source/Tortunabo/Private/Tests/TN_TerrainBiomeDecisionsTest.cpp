// Lógica pura de biomas y rocas de los módulos de terreno (TN_TerrainBiomeDecisions.h y
// BuildMonolithMesh): reparto de regiones en el camino, encaje de módulos por bioma,
// siembra del bosque de algas, fundido de paletas y malla de los monolitos.
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.TerrainBiome; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "Math/RandomStream.h"
#include "World/TN_TerrainBiomeDecisions.h"
#include "World/TN_TerrainModuleDecisions.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr int32 BiomeTestResolution = 21;
	constexpr double BiomeTestSize = 20000.0;

	TNTerrainModule::FModuleField BiomeTestFlatField(const TArray<uint16>& Heights)
	{
		TNTerrainModule::FModuleField Field;
		Field.Resolution = BiomeTestResolution;
		Field.Size = BiomeTestSize;
		Field.Heights = Heights;
		Field.HeightScale = 0.25;
		Field.HeightZero = 32768;
		return Field;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNTerrainBiomeRegionsTest,
	"Tortunabo.TerrainBiome.Regions",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNTerrainBiomeRegionsTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("camino vacío: plan vacío"), TNTerrainBiome::PlanPathBiomes(0, [](int32 Min, int32) { return Min; }).Num(), 0);

	for (int32 Length = 1; Length <= 20; ++Length)
	{
		for (int32 Seed = 0; Seed < 40; ++Seed)
		{
			FRandomStream Stream(Seed * 97 + Length);
			const TArray<TNTerrainBiome::FCellBiome> Plan = TNTerrainBiome::PlanPathBiomes(Length,
				[&Stream](int32 Min, int32 Max) { return Stream.RandRange(Min, Max); });
			if (!TestEqual(TEXT("una entrada por celda"), Plan.Num(), Length)) { return false; }

			TestFalse(TEXT("el inicio es puro"), Plan[0].IsMixed());
			TSet<ETNTerrainBiome> Seen;
			int32 Borders = 0;
			for (int32 I = 0; I < Length; ++I)
			{
				Seen.Add(Plan[I].Primary);
				TestFalse(TEXT("las algas nunca son región abierta"), Plan[I].bOpen && Plan[I].Primary == ETNTerrainBiome::Algae);
				if (I == 0) { continue; }
				if (Plan[I].Primary == Plan[I - 1].Primary)
				{
					TestEqual(TEXT("toda la región es abierta o cerrada"), Plan[I].bOpen, Plan[I - 1].bOpen);
				}
				const bool bChanged = Plan[I].Primary != Plan[I - 1].Primary;
				// Toda frontera es mixta y funde con el bioma anterior; nada más es mixto.
				TestEqual(TEXT("mixta solo en la frontera"), Plan[I].IsMixed(), bChanged);
				if (bChanged)
				{
					++Borders;
					TestEqual(TEXT("la frontera funde con el anterior"), Plan[I].Secondary, Plan[I - 1].Primary);
				}
			}
			const int32 ExpectedRegions = FMath::Clamp(Length / 4, 1, TNTerrainBiome::NumBiomes);
			TestEqual(TEXT("regiones: una por cada 4 celdas, hasta 3"), Borders + 1, ExpectedRegions);
			TestEqual(TEXT("cada región tiene un bioma distinto"), Seen.Num(), ExpectedRegions);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNTerrainBiomeMatchTest,
	"Tortunabo.TerrainBiome.Match",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNTerrainBiomeMatchTest::RunTest(const FString& Parameters)
{
	using TNTerrainBiome::BiomeMatchScore;
	const ETNTerrainBiome Sand = ETNTerrainBiome::Sand;
	const ETNTerrainBiome Water = ETNTerrainBiome::Water;
	const ETNTerrainBiome Algae = ETNTerrainBiome::Algae;

	const TNTerrainBiome::FCellBiome PureWater{ Water, Water };
	TestEqual(TEXT("puro pedido, puro igual"), BiomeMatchScore(Water, Water, PureWater), 3);
	TestEqual(TEXT("puro pedido, mixto del mismo principal"), BiomeMatchScore(Water, Algae, PureWater), 1);
	TestEqual(TEXT("puro pedido, otro bioma"), BiomeMatchScore(Sand, Sand, PureWater), 0);
	TestEqual(TEXT("puro pedido, mixto con el pedido de secundario"), BiomeMatchScore(Sand, Water, PureWater), 0);

	const TNTerrainBiome::FCellBiome Border{ Algae, Sand };
	TestEqual(TEXT("frontera, misma pareja y orden"), BiomeMatchScore(Algae, Sand, Border), 3);
	TestEqual(TEXT("frontera, misma pareja al revés"), BiomeMatchScore(Sand, Algae, Border), 2);
	TestEqual(TEXT("frontera, puro del principal"), BiomeMatchScore(Algae, Algae, Border), 1);
	TestEqual(TEXT("frontera, puro del secundario"), BiomeMatchScore(Sand, Sand, Border), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNTerrainBiomeEdgesTest,
	"Tortunabo.TerrainBiome.Edges",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNTerrainBiomeEdgesTest::RunTest(const FString& Parameters)
{
	using TNTerrainBiome::EdgeBetween;
	const TNTerrainBiome::FCellBiome OpenSand{ ETNTerrainBiome::Sand, ETNTerrainBiome::Sand, true };
	const TNTerrainBiome::FCellBiome ClosedSand{ ETNTerrainBiome::Sand, ETNTerrainBiome::Sand, false };
	const TNTerrainBiome::FCellBiome OpenWater{ ETNTerrainBiome::Water, ETNTerrainBiome::Water, true };
	const TNTerrainBiome::FCellBiome OpenAlgae{ ETNTerrainBiome::Algae, ETNTerrainBiome::Algae, true };

	TestEqual(TEXT("arena abierta conectada: explanada"), EdgeBetween(OpenSand, OpenSand, true), ETNTerrainEdge::Open);
	TestEqual(TEXT("agua abierta conectada: agua"), EdgeBetween(OpenWater, OpenWater, true), ETNTerrainEdge::Water);
	TestEqual(TEXT("sin conexión: cresta"), EdgeBetween(OpenSand, OpenSand, false), ETNTerrainEdge::Crest);
	TestEqual(TEXT("mar sin conexión: agua (un solo mar, sin fronteras)"), EdgeBetween(OpenWater, OpenWater, false), ETNTerrainEdge::Water);
	TestTrue(TEXT("mar abierto"), TNTerrainBiome::IsOpenWater(OpenWater));
	TestFalse(TEXT("explanada no es mar"), TNTerrainBiome::IsOpenWater(OpenSand));
	TestEqual(TEXT("una región cerrada: cresta"), EdgeBetween(OpenSand, ClosedSand, true), ETNTerrainEdge::Crest);
	TestEqual(TEXT("biomas distintos: cresta"), EdgeBetween(OpenSand, OpenWater, true), ETNTerrainEdge::Crest);
	TestEqual(TEXT("algas: siempre cresta"), EdgeBetween(OpenAlgae, OpenAlgae, true), ETNTerrainEdge::Crest);
	TestEqual(TEXT("simétrico"), EdgeBetween(ClosedSand, OpenSand, true), EdgeBetween(OpenSand, ClosedSand, true));

	// Rotación: un módulo con el Norte abierto, girado un cuarto, lo ofrece al Este.
	UTN_TerrainModuleAsset* Asset = NewObject<UTN_TerrainModuleAsset>();
	Asset->SideEdges = { ETNTerrainEdge::Open, ETNTerrainEdge::Crest, ETNTerrainEdge::Crest, ETNTerrainEdge::Water };
	TestEqual(TEXT("sin girar, Norte abierto"), TNTerrainModule::WorldSideEdge(*Asset, 0, TNGridLogic::SideNorth), ETNTerrainEdge::Open);
	TestEqual(TEXT("un cuarto: el Norte pasa al Este"), TNTerrainModule::WorldSideEdge(*Asset, 1, TNGridLogic::SideEast), ETNTerrainEdge::Open);
	TestEqual(TEXT("un cuarto: el Oeste pasa al Norte"), TNTerrainModule::WorldSideEdge(*Asset, 1, TNGridLogic::SideNorth), ETNTerrainEdge::Water);
	const ETNTerrainEdge Wanted[TNGridLogic::NumSides] = { ETNTerrainEdge::Water, ETNTerrainEdge::Open, ETNTerrainEdge::Crest, ETNTerrainEdge::Crest };
	TestTrue(TEXT("encaja girado un cuarto"), TNTerrainModule::EdgesMatch(*Asset, 1, Wanted));
	TestFalse(TEXT("no encaja sin girar"), TNTerrainModule::EdgesMatch(*Asset, 0, Wanted));

	// Espejo: Este y Oeste se intercambian; las curvas y las T cambian de mano.
	TestEqual(TEXT("espejo: el Oeste pasa al Este"), TNTerrainModule::WorldSideEdge(*Asset, 0, TNGridLogic::SideEast, true), ETNTerrainEdge::Water);
	TestEqual(TEXT("espejo: el Norte no cambia"), TNTerrainModule::WorldSideEdge(*Asset, 0, TNGridLogic::SideNorth, true), ETNTerrainEdge::Open);
	TestEqual(TEXT("espejo de curva izquierda"), TNTerrainModule::MirrorTopology(ETNTerrainModuleTopology::CurveLeft), ETNTerrainModuleTopology::CurveRight);
	TestEqual(TEXT("espejo de T derecha"), TNTerrainModule::MirrorTopology(ETNTerrainModuleTopology::TRight), ETNTerrainModuleTopology::TLeft);
	TestEqual(TEXT("máscara reflejada"), TNTerrainModule::MirrorMask(TNTerrainModule::MaskSouth | TNTerrainModule::MaskWest),
		static_cast<uint8>(TNTerrainModule::MaskSouth | TNTerrainModule::MaskEast));
	TestEqual(TEXT("la máscara de la topología reflejada es la máscara reflejada"),
		TNTerrainModule::ExitMask(TNTerrainModule::MirrorTopology(ETNTerrainModuleTopology::TLeft)),
		TNTerrainModule::MirrorMask(TNTerrainModule::ExitMask(ETNTerrainModuleTopology::TLeft)));
	TestTrue(TEXT("con agua: salidas exactas"), TNTerrainModule::HasWaterEdge(*Asset));

	// Heightfield reflejado: la columna J lee la R-1-J.
	TArray<uint16> Ramp;
	for (int32 I = 0; I < BiomeTestResolution; ++I)
	{
		for (int32 J = 0; J < BiomeTestResolution; ++J) { Ramp.Add(static_cast<uint16>(32768 + 40 * J)); }
	}
	TNTerrainModule::FModuleField Mirrored = BiomeTestFlatField(Ramp);
	Mirrored.bMirrorY = true;
	const TNTerrainModule::FModuleField Plain = BiomeTestFlatField(Ramp);
	TestEqual(TEXT("altura reflejada"), Mirrored.HeightAt(3, 2), Plain.HeightAt(3, BiomeTestResolution - 1 - 2));
	TestTrue(TEXT("muestreo reflejado en Y"), FMath::IsNearlyEqual(
		TNTerrainModule::SampleHeight(Mirrored, FVector2D(1234.0, 3000.0)), TNTerrainModule::SampleHeight(Plain, FVector2D(1234.0, -3000.0)), 0.01));

	UTN_TerrainModuleAsset* Corridor = NewObject<UTN_TerrainModuleAsset>();
	const ETNTerrainEdge AllCrest[TNGridLogic::NumSides] = { ETNTerrainEdge::Crest, ETNTerrainEdge::Crest, ETNTerrainEdge::Crest, ETNTerrainEdge::Crest };
	TestTrue(TEXT("sin SideEdges: los cuatro cresta"), TNTerrainModule::EdgesMatch(*Corridor, 3, AllCrest));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNTerrainBiomeFoliageTest,
	"Tortunabo.TerrainBiome.Foliage",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNTerrainBiomeFoliageTest::RunTest(const FString& Parameters)
{
	// Suelo plano a +2 m (valor 32768 + 800 con escala 0,25).
	TArray<uint16> Heights;
	Heights.Init(32768 + 800, BiomeTestResolution * BiomeTestResolution);
	const TNTerrainModule::FModuleField Field = BiomeTestFlatField(Heights);
	TNTerrainBiome::FFoliageSettings Settings;
	const int32 Cells = FMath::FloorToInt32(BiomeTestSize / Settings.Spacing);

	TArray<uint16> Empty;
	Empty.Init(0, Heights.Num());
	TestEqual(TEXT("densidad 0: sin algas"), TNTerrainBiome::BuildFoliage(Field, Empty, 5, Settings).Num(), 0);
	const TArray<uint16> WrongSize = { 255 };
	TestEqual(TEXT("máscara de otro tamaño: sin algas"), TNTerrainBiome::BuildFoliage(Field, WrongSize, 5, Settings).Num(), 0);

	// Byte alto (bioma) a tope y byte bajo (densidad) a tope: solo cuenta el bajo.
	TArray<uint16> Full;
	Full.Init(0xFFFF, Heights.Num());
	const TArray<TNTerrainBiome::FFoliageInstance> Forest = TNTerrainBiome::BuildFoliage(Field, Full, 5, Settings);
	TestEqual(TEXT("densidad 1: una planta por celda"), Forest.Num(), Cells * Cells);

	bool bInside = true;
	bool bGrounded = true;
	int32 Shapes[TNTerrainBiome::NumFoliageShapes] = {};
	for (const TNTerrainBiome::FFoliageInstance& Plant : Forest)
	{
		const FVector Location = Plant.Transform.GetLocation();
		bInside &= FMath::Abs(Location.X) <= BiomeTestSize * 0.5 && FMath::Abs(Location.Y) <= BiomeTestSize * 0.5;
		// El pivote de las mallas básicas está en su centro: el pie queda en el suelo menos 20 uu.
		const double Foot = Location.Z - (Plant.Shape == TNTerrainBiome::EFoliageShape::Bush
			? Plant.Transform.GetScale3D().X * TNTerrainBiome::EngineShapeSize * 0.2
			: Plant.Transform.GetScale3D().Z * TNTerrainBiome::EngineShapeSize * 0.5);
		bGrounded &= FMath::IsNearlyEqual(Foot, 200.0 - 20.0, 1.0);
		++Shapes[static_cast<int32>(Plant.Shape)];
	}
	TestTrue(TEXT("todas dentro del módulo"), bInside);
	TestTrue(TEXT("todas apoyadas en el suelo"), bGrounded);
	for (int32 Shape = 0; Shape < TNTerrainBiome::NumFoliageShapes; ++Shape)
	{
		TestTrue(TEXT("hay de todas las formas"), Shapes[Shape] > 0);
	}

	// Densidad media: aproximadamente la mitad, y la misma semilla planta lo mismo.
	TArray<uint16> Half;
	Half.Init(128, Heights.Num());
	const TArray<TNTerrainBiome::FFoliageInstance> Sparse = TNTerrainBiome::BuildFoliage(Field, Half, 5, Settings);
	TestTrue(TEXT("densidad 0,5: cerca de la mitad"), FMath::Abs(Sparse.Num() - Cells * Cells / 2) < Cells * Cells / 8);
	const TArray<TNTerrainBiome::FFoliageInstance> Again = TNTerrainBiome::BuildFoliage(Field, Half, 5, Settings);
	bool bSame = Again.Num() == Sparse.Num();
	for (int32 I = 0; bSame && I < Sparse.Num(); ++I)
	{
		bSame &= Again[I].Transform.Equals(Sparse[I].Transform) && Again[I].Shape == Sparse[I].Shape;
	}
	TestTrue(TEXT("determinista por semilla"), bSame);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNTerrainBiomePaletteBlendTest,
	"Tortunabo.TerrainBiome.PaletteBlend",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNTerrainBiomePaletteBlendTest::RunTest(const FString& Parameters)
{
	TArray<uint16> Heights;
	Heights.Init(32768, BiomeTestResolution * BiomeTestResolution);
	const TNTerrainModule::FModuleField Field = BiomeTestFlatField(Heights);
	const TNTerrainModule::FModuleColors Sand = TNTerrainBiome::ColorsFor(ETNTerrainBiome::Sand, -400.0);
	const TNTerrainModule::FModuleColors Algae = TNTerrainBiome::ColorsFor(ETNTerrainBiome::Algae, -400.0);
	TestFalse(TEXT("paletas distintas"), Sand.Floor.Equals(Algae.Floor));

	// Máscara: mitad oeste (J pequeña) peso 0, mitad este peso 255.
	TArray<uint16> Mask;
	Mask.Init(0, Heights.Num());
	for (int32 I = 0; I < BiomeTestResolution; ++I)
	{
		for (int32 J = BiomeTestResolution / 2 + 1; J < BiomeTestResolution; ++J) { Mask[I * BiomeTestResolution + J] = 255 << 8; }
	}

	const TNGridTerrain::FTileMesh Pure = TNTerrainModule::BuildModuleMesh(Field, Sand);
	const TNGridTerrain::FTileMesh Mixed = TNTerrainModule::BuildModuleMesh(Field, Sand, &Algae, Mask);
	const int32 West = 10 * BiomeTestResolution + 2;
	const int32 East = 10 * BiomeTestResolution + BiomeTestResolution - 3;
	TestTrue(TEXT("peso 0: paleta principal"), Mixed.Colors[West].Equals(Pure.Colors[West], 1e-4f));
	TestTrue(TEXT("peso 255: paleta secundaria"), Mixed.Colors[East].Equals(
		TNTerrainModule::SampleModuleColor(Algae, 0.0, Mixed.Normals[East]), 1e-4f));
	TestTrue(TEXT("sin paleta secundaria no hay fundido"),
		TNTerrainModule::BuildModuleMesh(Field, Sand, nullptr, Mask).Colors[East].Equals(Pure.Colors[East], 1e-4f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNTerrainBiomeMonolithMeshTest,
	"Tortunabo.TerrainBiome.MonolithMesh",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNTerrainBiomeMonolithMeshTest::RunTest(const FString& Parameters)
{
	FTNTerrainModuleMonolith Monolith;
	Monolith.Center = FVector2D(-2000.0, 1500.0);
	Monolith.BaseHeight = -100.f;
	Monolith.Radius = 400.f;
	Monolith.Height = 2000.f;
	Monolith.Yaw = 30.f;
	Monolith.Lean = 5.f;

	const TNTerrainModule::FModuleColors Colors;
	const TNTerrainModule::FMonolithShape Shape;
	const TNGridTerrain::FTileMesh Mesh = TNTerrainModule::BuildMonolithMesh(Monolith, 3, Colors, Shape);

	const int32 Expected = (Shape.Stations + 1) * Shape.RingPoints + 2;
	TestEqual(TEXT("vértices: anillos + 2 tapas"), Mesh.Vertices.Num(), Expected);
	TestEqual(TEXT("una normal y un color por vértice"), Mesh.Normals.Num() + Mesh.Colors.Num(), 2 * Expected);
	TestEqual(TEXT("triángulos: laterales + tapas"), Mesh.Triangles.Num(), 3 * (2 * Shape.Stations * Shape.RingPoints + 2 * Shape.RingPoints));
	bool bIndicesValid = true;
	for (const int32 Index : Mesh.Triangles) { bIndicesValid &= Mesh.Vertices.IsValidIndex(Index); }
	TestTrue(TEXT("índices válidos"), bIndicesValid);

	double MinZ = UE_BIG_NUMBER;
	double MaxZ = -UE_BIG_NUMBER;
	for (const FVector& V : Mesh.Vertices)
	{
		MinZ = FMath::Min(MinZ, V.Z);
		MaxZ = FMath::Max(MaxZ, V.Z);
	}
	TestTrue(TEXT("pie a BaseHeight"), FMath::IsNearlyEqual(MinZ, Monolith.BaseHeight, 1.0));
	TestTrue(TEXT("cima sobre BaseHeight + Height"), MaxZ >= Monolith.BaseHeight + Monolith.Height - 1.0);

	// Normal hacia fuera en el anillo central: apunta en contra del eje.
	const int32 Ring = (Shape.Stations / 2) * Shape.RingPoints;
	const FVector Center = (Mesh.Vertices[Ring] + Mesh.Vertices[Ring + Shape.RingPoints / 2]) * 0.5;
	TestTrue(TEXT("normales hacia fuera"), FVector::DotProduct(Mesh.Normals[Ring], Mesh.Vertices[Ring] - Center) > 0.0);
	TestTrue(TEXT("determinista"), TNTerrainModule::BuildMonolithMesh(Monolith, 3, Colors, Shape).Vertices == Mesh.Vertices);
	TestFalse(TEXT("la semilla cambia la roca"), TNTerrainModule::BuildMonolithMesh(Monolith, 4, Colors, Shape).Vertices == Mesh.Vertices);

	FTNTerrainModuleMonolith Degenerate = Monolith;
	Degenerate.Radius = 0.f;
	TestEqual(TEXT("radio 0: sin malla"), TNTerrainModule::BuildMonolithMesh(Degenerate, 3, Colors, Shape).Vertices.Num(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
