// ─────────────────────────────────────────────────────────────────────────────
// ATN_ProcMapGenerator — vegetación y rocas sueltas: una malla estática por bioma,
// especie y variante (generada en ejecución, low-poly de caras planas con color de
// vértice) instanciada con HISM sobre el reparto puro de TN_ProcMapFlora.h.
// ─────────────────────────────────────────────────────────────────────────────

#include "World/ProcMap/TN_ProcMapGenerator.h"
#include "World/ProcMap/TN_ProcMapFlora.h"
#include "World/ProcMap/TN_ProcMapTypes.h"
#include "Core/TN_Log.h"
#include "TN_ProcMapMeshKit.h"
#include "TN_ProcMapFloraMeshes.h"
#include "TN_ProcMapPropMeshes.h"
#include "TN_ProcMapKeepOut.h"
#include "Async/ParallelFor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"

using namespace TNProcMesh;
using namespace TNFloraMesh;

namespace
{
	/** Inversa de la codificación sRGB de un canal. */
	float TNFloraSRGBToLinear(float C)
	{
		const float V = FMath::Clamp(C, 0.f, 1.f);
		return V <= 0.04045f ? V / 12.92f : FMath::Pow((V + 0.055f) / 1.055f, 2.4f);
	}

	/**
	 * Malla estática en ejecución a partir de unos buffers de caras planas (con una caja de colisión si se
	 * pide). El alfa del color de vértice es el peso de balanceo del viento del material: Wind x (altura
	 * relativa)^Exponent, 0 en la base (troncos, raíces) y Wind en lo más alto.
	 */
	UStaticMesh* TNFloraMakeStaticMesh(UObject* Outer, const FTNProcMeshBuffers& B, UMaterialInterface* Material, bool bBoxCollision = false,
		float Wind = 0.f, float WindExponent = 1.5f)
	{
		if (B.IsEmpty()) { return nullptr; }
		double MinZ = 1e18, MaxZ = -1e18;
		for (const FVector& V : B.Verts) { MinZ = FMath::Min(MinZ, V.Z); MaxZ = FMath::Max(MaxZ, V.Z); }
		const double SpanZ = FMath::Max(1.0, MaxZ - FMath::Max(0.0, MinZ));
		static const FName SlotName(TEXT("Flora"));
		FMeshDescription Desc;
		FStaticMeshAttributes Attributes(Desc);
		Attributes.Register();
		TVertexAttributesRef<FVector3f> Positions = Attributes.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> Normals = Attributes.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector3f> Tangents = Attributes.GetVertexInstanceTangents();
		TVertexInstanceAttributesRef<float> Signs = Attributes.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesRef<FVector4f> Colors = Attributes.GetVertexInstanceColors();
		TVertexInstanceAttributesRef<FVector2f> UVs = Attributes.GetVertexInstanceUVs();
		UVs.SetNumChannels(1);
		const FPolygonGroupID Group = Desc.CreatePolygonGroup();
		Attributes.GetPolygonGroupMaterialSlotNames()[Group] = SlotName;

		const int32 NumVerts = B.Verts.Num();
		Desc.ReserveNewVertices(NumVerts);
		Desc.ReserveNewVertexInstances(NumVerts);
		Desc.ReserveNewTriangles(B.Tris.Num() / 3);
		TArray<FVertexInstanceID> Instances;
		Instances.SetNum(NumVerts);
		for (int32 i = 0; i < NumVerts; ++i)
		{
			const FVertexID V = Desc.CreateVertex();
			Positions[V] = FVector3f(B.Verts[i]);
			const FVertexInstanceID VI = Desc.CreateVertexInstance(V);
			const FVector N = B.Normals[i];
			const FVector T = FVector::CrossProduct(FMath::Abs(N.Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector, N).GetSafeNormal();
			Normals[VI] = FVector3f(N);
			Tangents[VI] = FVector3f(T);
			Signs[VI] = 1.f;
			// La malla guarda el color en sRGB (ToFColor(true)) y el nodo VertexColor lo lee tal cual:
			// se decodifica antes para que el material reciba el color lineal de la paleta (como el
			// de las mallas procedurales) y no uno aclarado.
			const double Rel = FMath::Clamp((B.Verts[i].Z - FMath::Max(0.0, MinZ)) / SpanZ, 0.0, 1.0);
			const float Sway = Wind > 0.f ? Wind * static_cast<float>(FMath::Pow(Rel, static_cast<double>(WindExponent))) : 0.f;
			Colors[VI] = FVector4f(TNFloraSRGBToLinear(B.Colors[i].R), TNFloraSRGBToLinear(B.Colors[i].G), TNFloraSRGBToLinear(B.Colors[i].B), Sway);
			UVs.Set(VI, 0, FVector2f(B.UVs[i]));
			Instances[i] = VI;
		}
		for (int32 t = 0; t + 2 < B.Tris.Num(); t += 3)
		{
			FVertexInstanceID Tri[3] = { Instances[B.Tris[t]], Instances[B.Tris[t + 1]], Instances[B.Tris[t + 2]] };
			Desc.CreateTriangle(Group, MakeArrayView(Tri, 3));
		}

		UStaticMesh* Mesh = NewObject<UStaticMesh>(Outer, NAME_None, RF_Transient);
		Mesh->GetStaticMaterials().Add(FStaticMaterial(Material, SlotName));
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bMarkPackageDirty = false;
		Params.bBuildSimpleCollision = bBoxCollision;
		Params.bCommitMeshDescription = false;
		Params.bFastBuild = true;
		TArray<const FMeshDescription*> Descs;
		Descs.Add(&Desc);
		Mesh->BuildFromMeshDescriptions(Descs, Params);
		return Mesh;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Vegetación procedural
// ─────────────────────────────────────────────────────────────────────────────

void ATN_ProcMapGenerator::BuildFlora()
{
	using namespace TNProcMap;
	if (Settings && !Settings->bProceduralFlora) { return; }
	if (Heights.Num() != LatticeNX * LatticeNY || LatticeNX < 2) { return; }
	const double T0 = FPlatformTime::Seconds();

	TArray<FFloraSpecies> Tables[NumBiomes];
	for (int32 b = 0; b < NumBiomes; ++b) { FloraSpeciesFor(BiomeFromIndex(b), Tables[b]); }

	FTNProcKeepOut Keep;
	Keep.AddLayout(Layout);

	// Consultas del reparto: el terreno ya calculado (las mismas alturas que la malla).
	struct FFloraQuery
	{
		const ATN_ProcMapGenerator* Gen = nullptr;
		const FTNProcKeepOut* KeepOut = nullptr;
		double Height(const FVector2D& P) const { return Gen->TerrainHeightMap(P); }
		FVector Normal(const FVector2D& P) const { return Gen->TerrainNormalMap(P); }
		double Edge(const FVector2D& P) const { return Gen->PathDistanceMap(P); }
		bool Blocked(const FVector2D& P) const { return KeepOut->Blocked(P); }
	};
	FFloraQuery Query;
	Query.Gen = this;
	Query.KeepOut = &Keep;

	// Reparto en paralelo por bandas de filas (cada celda con su generador: mismo resultado).
	const FVector2D LatMin = LatticeOrigin;
	const FVector2D LatMax = LatticeOrigin + FVector2D((LatticeNX - 1) * LatticeSpacing, (LatticeNY - 1) * LatticeSpacing);
	const double DensityScale = Settings ? static_cast<double>(Settings->FloraDensity) : 1.0;
	TArray<FFloraInstance> Placed;
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		const FFloraGrid Grid = FloraGridFor(LatMin, LatMax, Pass);
		const int32 Bands = FMath::Clamp(Grid.NY / 32, 1, 128);
		TArray<TArray<FFloraInstance>> Parts;
		Parts.SetNum(Bands);
		ParallelFor(Bands, [&](int32 Band)
		{
			PlaceFloraRows(Layout, Tables, Query, Pass, Grid, Grid.NY * Band / Bands, Grid.NY * (Band + 1) / Bands, Parts[Band], DensityScale);
		});
		for (TArray<FFloraInstance>& Part : Parts) { Placed.Append(MoveTemp(Part)); }
	}
	const double T1 = FPlatformTime::Seconds();

	// Aspecto de cada especie (los objetos sueltos, el de su prop; nunca se estiran).
	auto LookOf = [](const FFloraSpecies& Sp)
	{
		FTNFloraLook Look = TNFloraLookOf(Sp.Shape);
		if (Sp.Shape == EFloraShape::Prop)
		{
			const TNPropMesh::FTNPropLook Prop = TNPropMesh::TNPropLookOf(Sp.Prop);
			Look.Cull = Prop.Cull;
			Look.bShadow = Prop.bShadow;
			Look.bStretch = false;
		}
		return Look;
	};

	// Transformadas por malla (bioma, especie, variante), en espacio del mapa = local del generador.
	TMap<int32, TArray<FTransform>> ByMesh;
	for (const FFloraInstance& I : Placed)
	{
		const FFloraSpecies& Sp = Tables[I.Biome][I.Species];
		const FTNFloraLook Look = LookOf(Sp);
		// Altura propia de cada ejemplar (+-12 %): no todos iguales aunque compartan escala.
		const double Stretch = Look.bStretch ? 0.88 + 0.24 * (0.5 + 0.5 * TNProcHashNoise(FMath::RoundToInt32(I.Location.X), FMath::RoundToInt32(I.Location.Y), 0x5EEDu)) : 1.0;
		const FQuat Yaw(FVector::UpVector, FMath::DegreesToRadians(I.Yaw));
		const FQuat Lean(FVector(-I.LeanDir.Y, I.LeanDir.X, 0.0), FMath::DegreesToRadians(I.LeanDeg));
		const int32 Key = (I.Biome * 64 + I.Species) * FloraVariants + I.Variant;
		ByMesh.FindOrAdd(Key).Add(FTransform(Lean * Yaw, I.Location, FVector(I.Scale, I.Scale, I.Scale * Stretch)));
	}

	UMaterialInterface* Material = Settings && Settings->FoliageMaterial ? Settings->FoliageMaterial.Get() : nullptr;
	if (!Material) { Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ProcMap/Materials/M_ProcFoliage.M_ProcFoliage")); }
	if (!Material) { Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial")); }

	int32 Meshes = 0;
	int32 Total = 0;
	for (TPair<int32, TArray<FTransform>>& Entry : ByMesh)
	{
		const int32 Variant = Entry.Key % FloraVariants;
		const int32 Species = (Entry.Key / FloraVariants) % 64;
		const int32 BiomeIdx = Entry.Key / FloraVariants / 64;
		const FFloraSpecies& Sp = Tables[BiomeIdx][Species];
		FLinearColor Ground, PathC, RockC, Bed;
		ResolveBiomeColors(BiomeFromIndex(BiomeIdx), Ground, PathC, RockC, Bed);
		FTNProcMeshBuffers Buffers;
		const uint32 MeshSeed = HashCell(static_cast<uint32>(Layout.Params.Seed) ^ 0xF10Au, (BiomeIdx * 64 + static_cast<int32>(Sp.Shape)) * 64 + static_cast<int32>(Sp.Prop), Variant);
		const bool bProp = Sp.Shape == EFloraShape::Prop;
		const bool bSolid = bProp && TNPropMesh::TNPropSolid(Sp.Prop);
		if (bProp)
		{
			const ETNProcBiome Biome = BiomeFromIndex(BiomeIdx);
			TNPropMesh::TNPropBuild(Buffers, Sp.Prop, Variant, MeshSeed, TNPropMesh::TNPropCrystalColor(Biome), Biome == ETNProcBiome::Volcanic);
			// La paleta de los objetos es la de las mallas procedurales (obstáculos, formaciones), que se ve como
			// sRGB: se decodifica una vez más para que un cono o una caja se vean igual en los dos sitios.
			for (FLinearColor& Col : Buffers.Colors)
			{
				Col = FLinearColor(TNFloraSRGBToLinear(Col.R), TNFloraSRGBToLinear(Col.G), TNFloraSRGBToLinear(Col.B), Col.A);
			}
		}
		else
		{
			TNFloraBuild(Buffers, Sp.Shape, TNFloraPaletteFor(BiomeFromIndex(BiomeIdx), Ground, RockC), Variant, MeshSeed);
		}
		const FTNFloraWind Wind = TNFloraWindOf(Sp.Shape);
		UStaticMesh* Mesh = TNFloraMakeStaticMesh(this, Buffers, Material, bSolid, Wind.Stiffness, Wind.Exponent);
		if (!Mesh) { continue; }
		FloraMeshes.Add(Mesh);

		const FTNFloraLook Look = LookOf(Sp);
		UHierarchicalInstancedStaticMeshComponent* HISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
		HISM->SetupAttachment(RootComponent);
		HISM->SetStaticMesh(Mesh);
		// La vegetación crece fuera del suelo del camino y se atraviesa; los objetos macizos (cajas,
		// barriles, pacas, bancos, farolas...) bloquean con una caja de su tamaño.
		HISM->SetCollisionEnabled(bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		if (bSolid) { HISM->SetCollisionProfileName(TEXT("BlockAll")); }
		HISM->SetCanEverAffectNavigation(false);
		HISM->SetCastShadow(Look.bShadow);
		// Viento: solo se evalúa cerca (lo lejano apenas se ve moverse); las rocas y los objetos, nunca.
		if (Wind.Stiffness > 0.f)
		{
			HISM->WorldPositionOffsetDisableDistance = Wind.DisableDistance;
		}
		else
		{
			HISM->bEvaluateWorldPositionOffset = false;
		}
		if (Look.Cull > 0.f)
		{
			HISM->SetCullDistances(FMath::RoundToInt(Look.Cull * 0.8f), FMath::RoundToInt(Look.Cull));
		}
		HISM->RegisterComponent();
		HISM->AddInstances(Entry.Value, false, false);
		ScatterComponents.Add(HISM);
		++Meshes;
		Total += Entry.Value.Num();
	}
	UE_LOG(LogTortunabo, Log, TEXT("[ProcMap] Vegetación procedural: %d plantas y rocas en %d mallas (reparto %.2fs, mallas %.2fs)."),
		Total, Meshes, T1 - T0, FPlatformTime::Seconds() - T1);
}
