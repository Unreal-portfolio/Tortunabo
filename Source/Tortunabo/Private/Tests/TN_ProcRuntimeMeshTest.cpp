// Mallas construidas en ejecución (TNProcRuntimeMesh::MakeStaticMesh): cada material sale con sus datos de
// UV del streaming de texturas inicializados y con la densidad calculada, también sin datos de editor (#390).
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.ProcRuntimeMesh; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "UObject/Package.h"
#include "World/ProcMap/TN_ProcMapRuntimeMesh.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Cuadrado de Size x Size en el plano XY: AddQuad le da UV = posición / 400. */
	TNProcMesh::FTNProcMeshBuffers MakeSquare(double Size)
	{
		TNProcMesh::FTNProcMeshBuffers B;
		B.AddQuad(FVector(0, 0, 0), FVector(Size, 0, 0), FVector(Size, Size, 0), FVector(0, Size, 0), FVector::UpVector, FLinearColor::White);
		return B;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNProcRuntimeMeshUVChannelTest,
	"Tortunabo.ProcRuntimeMesh.UVChannelData",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNProcRuntimeMeshUVChannelTest::RunTest(const FString& Parameters)
{
	using namespace TNProcRuntimeMesh;

	// 400 unidades de mundo por unidad de UV: AddQuad divide la posición entre 400.
	TestEqual(TEXT("Densidad de un cuadrado con UV = posición / 400"), ComputeUVDensity(MakeSquare(800.0)), 400.f, 0.01f);

	TNProcMesh::FTNProcMeshBuffers Flat = MakeSquare(800.0);
	for (FVector2D& UV : Flat.UVs) { UV = FVector2D::ZeroVector; }
	TestEqual(TEXT("Sin área en UV la densidad es 0"), ComputeUVDensity(Flat), 0.f);

	UMaterialInterface* Material = UMaterial::GetDefaultMaterial(MD_Surface);
	UStaticMesh* Mesh = MakeStaticMesh(GetTransientPackage(), MakeSquare(800.0), Material);
	if (!TestNotNull(TEXT("Malla construida"), Mesh)) { return false; }
	if (!TestEqual(TEXT("Un material"), Mesh->GetStaticMaterials().Num(), 1)) { return false; }

	// En el editor UStaticMesh::InitResources marca bInitialized aunque la construcción rápida no calcule
	// densidades; sin datos de editor no lo hace nadie. Se exige que la densidad venga de la propia malla.
	const FMeshUVChannelInfo& UVData = Mesh->GetStaticMaterials()[0].UVChannelData;
	TestTrue(TEXT("UVChannelData marcado como inicializado"), UVData.bInitialized);
	TestTrue(TEXT("UVChannelData con densidad (IsInitialized)"), UVData.IsInitialized());
	TestEqual(TEXT("Densidad del canal 0"), UVData.LocalUVDensities[0], 400.f, 0.01f);

	UStaticMesh* FlatMesh = MakeStaticMesh(GetTransientPackage(), Flat, Material);
	if (TestNotNull(TEXT("Malla sin UV construida"), FlatMesh))
	{
		TestTrue(TEXT("Sin UV también queda inicializado"), FlatMesh->GetStaticMaterials()[0].UVChannelData.bInitialized);
	}
	return true;
}

#endif
