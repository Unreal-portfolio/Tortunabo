#include "Lobby/TN_GeneralBriefing.h"
#include "Core/TN_CosmeticLook.h"
#include "Core/TN_Log.h"
#include "Player/MP_GamePlayerController.h"
#include "Animation/AnimationAsset.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "../World/ProcMap/TN_ProcMapRuntimeMesh.h"

namespace TNGeneralDetail
{
	/** Color sRGB 0xRRGGBB para mallas en ejecución con M_CosmeticVertexColor (como en el puesto del tendero). */
	FLinearColor Pal(uint32 Hex)
	{
		return FLinearColor(TNProcRuntimeMesh::SRGBToLinear(((Hex >> 16) & 255) / 255.f), TNProcRuntimeMesh::SRGBToLinear(((Hex >> 8) & 255) / 255.f),
			TNProcRuntimeMesh::SRGBToLinear((Hex & 255) / 255.f), 1.f);
	}

	/** Mesa de madera delante del general (en su +X). */
	constexpr double TableX = 150.0;
	constexpr double TableHalfDepth = 70.0;
	constexpr double TableHalfWidth = 125.0;
	constexpr double TableTop = 92.0;
	/** Maqueta: el lobby (±2500 cm) a escala 1:45 sobre la mesa; la salida del castillo mira al general. */
	constexpr double ModelScale = 0.022;
	constexpr double ModelBase = TableTop + 0.8;
	/** Cartel detrás del general. */
	constexpr double SignX = -48.0;
	constexpr double SignZ = 300.0;
	constexpr double SignHalfW = 130.0;
	constexpr double SignHalfH = 24.0;

	/** Punto del lobby (cm, x a la izquierda de la salida e y hacia la salida) sobre la maqueta, a altura Z sobre su base. */
	FVector ModelPoint(double LobbyX, double LobbyY, double Z)
	{
		return FVector(TableX - LobbyY * ModelScale, LobbyX * ModelScale, ModelBase + Z);
	}

	/** Tramo de muralla de arena con almenas entre dos puntos del lobby (la puerta se deja con dos tramos). */
	void AddWall(TNProcMesh::FTNProcMeshBuffers& B, double Ax, double Ay, double Bx, double By, const FLinearColor& Sand, const FLinearColor& SandDark)
	{
		const FVector A = ModelPoint(Ax, Ay, 0.0);
		const FVector C = ModelPoint(Bx, By, 0.0);
		const FVector Dir = (C - A).GetSafeNormal2D();
		const double Len = FVector::Dist2D(A, C);
		B.AddBox((A + C) * 0.5 + FVector(0.0, 0.0, 3.4), Dir, FVector(Len * 0.5, 1.2, 3.0), Sand);
		const int32 Teeth = FMath::Max(1, static_cast<int32>(Len / 3.2));
		for (int32 t = 0; t < Teeth; t += 2)
		{
			const FVector P = A + Dir * ((t + 0.5) * Len / Teeth);
			B.AddBox(P + FVector(0.0, 0.0, 7.0), Dir, FVector(Len / Teeth * 0.5, 1.3, 0.8), SandDark);
		}
	}

	void AddTower(TNProcMesh::FTNProcMeshBuffers& B, double Lx, double Ly, double Height, const FLinearColor& Sand, const FLinearColor& SandDark)
	{
		const FVector Base = ModelPoint(Lx, Ly, 0.0);
		TNProcMesh::TNProcAddCylinder(B, Base, Base + FVector(0.0, 0.0, Height), 4.2, 3.6, 10, Sand);
		for (int32 k = 0; k < 6; ++k)
		{
			const double A = k * TNProcMap::TwoPi / 6.0;
			const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.0);
			B.AddBox(Base + Dir * 3.2 + FVector(0.0, 0.0, Height + 0.8), Dir, FVector(0.7, 0.9, 0.8), SandDark);
		}
	}

	void AddPin(TNProcMesh::FTNProcMeshBuffers& B, const FVector& Foot, const FLinearColor& Color, const FLinearColor& Stick)
	{
		B.AddBeam(Foot, Foot + FVector(0.0, 0.0, 9.0), 0.25, Stick);
		const FVector Top = Foot + FVector(0.0, 0.0, 9.0);
		B.AddTri(Top, Top - FVector(0.0, 0.0, 3.0), Top + FVector(0.0, 4.0, -1.5), FVector(1.0, 0.0, 0.0), Color);
		B.AddTri(Top, Top - FVector(0.0, 0.0, 3.0), Top + FVector(0.0, 4.0, -1.5), FVector(-1.0, 0.0, 0.0), Color * 0.85f);
	}

	void AddEgg(TNProcMesh::FTNProcMeshBuffers& B, const FVector& Base, double Size, uint32 Seed, const FLinearColor& Color)
	{
		const TArray<double> Z = { 0.0, 0.3 * Size, 0.65 * Size, 1.0 * Size, 1.3 * Size };
		const TArray<double> R = { 0.3 * Size, 0.48 * Size, 0.46 * Size, 0.34 * Size, 0.14 * Size };
		TNProcMesh::TNProcAddLathe(B, Base, Z, R, 0.0, Seed, Color, 10, 0.3);
	}
}

ATN_GeneralBriefing::ATN_GeneralBriefing()
{
	using namespace TNGeneralDetail;
	PrimaryActorTick.bCanEverTick = true;
	NetDormancy = DORM_Awake;
	bAlwaysRelevant = true;
	CooldownSeconds = 0.6f;
	PromptText = NSLOCTEXT("Tortunabo", "GeneralPrompt", "Hablar con el general");
	GeneralName = NSLOCTEXT("Tortunabo", "GeneralName", "General Galápago");
	HeadquartersName = NSLOCTEXT("Tortunabo", "GeneralHQ", "Cuartel general");
	GeneralLook.HelmetId = TEXT("Helmet_Captain");
	GeneralLook.ShellId = TEXT("Shell_Moss");
	GeneralLook.SkinId = TEXT("Body_Forest");

	// Hitbox de interacción (invisible) delante de la mesa; el aviso flota sobre él.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Mesh && Cylinder.Succeeded())
	{
		Mesh->SetStaticMesh(Cylinder.Object);
		Mesh->SetRelativeLocation(FVector(TableX + TableHalfDepth + 60.0, 0.0, 60.0));
		Mesh->SetRelativeScale3D(FVector(2.4f, 2.4f, 1.2f));
		Mesh->SetHiddenInGame(true);
	}
	if (PromptWidgetComponent)
	{
		PromptWidgetComponent->SetUsingAbsoluteScale(true);
		PromptWidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, 160.f));
	}

	General = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("General"));
	General->SetupAttachment(SceneRoot);
	General->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	General->SetRelativeScale3D(FVector(GeneralScale));
	General->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	General->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> TurtleMesh(TEXT("/Game/Meshses/Characters/Player/TotugaDemo_Rig.TotugaDemo_Rig"));
	if (TurtleMesh.Succeeded()) { General->SetSkeletalMeshAsset(TurtleMesh.Object); }

	GeneralHat = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GeneralHat"));
	GeneralHat->SetupAttachment(General);
	GeneralHat->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	GeneralBlock = CreateDefaultSubobject<UCapsuleComponent>(TEXT("GeneralBlock"));
	GeneralBlock->SetupAttachment(SceneRoot);
	GeneralBlock->InitCapsuleSize(55.f, 95.f);
	GeneralBlock->SetRelativeLocation(FVector(0.f, 0.f, 95.f));
	GeneralBlock->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	GeneralBlock->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	Table = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Table"));
	Table->SetupAttachment(SceneRoot);
	Table->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	TableBlock = CreateDefaultSubobject<UBoxComponent>(TEXT("TableBlock"));
	TableBlock->SetupAttachment(SceneRoot);
	TableBlock->InitBoxExtent(FVector(TableHalfDepth + 2.0, TableHalfWidth + 2.0, TableTop * 0.5));
	TableBlock->SetRelativeLocation(FVector(TableX, 0.0, TableTop * 0.5));
	TableBlock->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	TableBlock->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// Cartel detrás del general: mira hacia los reclutas (+X).
	Sign = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Sign"));
	Sign->SetupAttachment(SceneRoot);
	Sign->SetRelativeLocation(FVector(SignX + 6.5, 0.0, SignZ));
	Sign->SetHorizontalAlignment(EHTA_Center);
	Sign->SetVerticalAlignment(EVRTA_TextCenter);
	Sign->SetWorldSize(36.f);
	Sign->SetTextRenderColor(FColor(255, 214, 90));
	Sign->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UAnimationAsset> Idle(TEXT("/Game/Animations/Character/TortugaDemo/Anim/Old_Man_Idle.Old_Man_Idle"));
	static ConstructorHelpers::FObjectFinder<UAnimationAsset> Salute(TEXT("/Game/Animations/Character/TortugaDemo/Anim/Salute.Salute"));
	IdleAnim = Idle.Succeeded() ? Idle.Object : nullptr;
	SaluteAnim = Salute.Succeeded() ? Salute.Object : nullptr;
}

void ATN_GeneralBriefing::BeginPlay()
{
	Super::BeginPlay();
	General->SetRelativeScale3D(FVector(GeneralScale));
	Sign->SetText(FText::FromString(HeadquartersName.ToString().ToUpper()));
	if (IdleAnim) { General->PlayAnimation(IdleAnim, true); }
	UTN_CosmeticLook::ApplyLook(this, General, GeneralHat, GeneralLook, GeneralDefaults);
	BuildTable();
	HideBlockout();
}

FVector ATN_GeneralBriefing::GetInteractionPoint() const
{
	using namespace TNGeneralDetail;
	return GetActorTransform().TransformPosition(FVector(TableX + TableHalfDepth + 60.0, 0.0, 0.0));
}

void ATN_GeneralBriefing::BuildTable()
{
	using namespace TNGeneralDetail;
	TNProcMesh::FTNProcMeshBuffers B;
	const FVector AxisX(1.0, 0.0, 0.0);
	const FLinearColor Wood = Pal(0xB07A4A);
	const FLinearColor WoodDark = Pal(0x7A4E2B);
	const FLinearColor WoodTop = Pal(0xD9A066);
	const FLinearColor SeaCloth = Pal(0x3AA7C9);
	const FLinearColor SandLight = Pal(0xF5DDA8);
	const FLinearColor SandShade = Pal(0xD9B77A);
	const FLinearColor Cream = Pal(0xFFF6E0);
	const FLinearColor Coral = Pal(0xFF6A52);
	const FLinearColor Navy = Pal(0x12305A);
	const FLinearColor Gold = Pal(0xFFCB3D);
	const FLinearColor Bottle = Pal(0x3FA66B);
	const FLinearColor Teal = Pal(0x2EC4B6);

	// Mesa: tablero, faldón y cuatro patas torneadas.
	B.AddBox(FVector(TableX, 0.0, TableTop - 3.5), AxisX, FVector(TableHalfDepth, TableHalfWidth, 3.5), WoodTop);
	B.AddBox(FVector(TableX, 0.0, TableTop - 11.0), AxisX, FVector(TableHalfDepth - 6.0, TableHalfWidth - 6.0, 4.5), Wood);
	for (const double Sx : { -1.0, 1.0 })
	{
		for (const double Sy : { -1.0, 1.0 })
		{
			const FVector Foot(TableX + Sx * (TableHalfDepth - 10.0), Sy * (TableHalfWidth - 10.0), 0.0);
			TNProcMesh::TNProcAddCylinder(B, Foot, Foot + FVector(0.0, 0.0, TableTop - 7.0), 5.5, 4.0, 8, WoodDark);
		}
	}

	// Tapete azul (el mar) y la maqueta del castillo de arena encima.
	B.AddBox(FVector(TableX, 0.0, TableTop + 0.4), AxisX, FVector(TableHalfDepth - 6.0, 66.0, 0.4), SeaCloth);
	B.AddBox(ModelPoint(0.0, 0.0, 1.2), AxisX, FVector(57.0, 57.0, 1.2), SandLight);

	// Murallas con almenas (la puerta, en y = +2350, queda abierta en el centro) y torres en las esquinas y a la puerta.
	const double W = 2350.0;
	const double Gate = 360.0;
	AddWall(B, -W, -W, W, -W, SandLight, SandShade);
	AddWall(B, W, -W, W, W, SandLight, SandShade);
	AddWall(B, -W, W, -W, -W, SandLight, SandShade);
	AddWall(B, W, W, Gate, W, SandLight, SandShade);
	AddWall(B, -Gate, W, -W, W, SandLight, SandShade);
	for (const double Tx : { -W, W })
	{
		for (const double Ty : { -W, W })
		{
			AddTower(B, Tx, Ty, 10.0, SandLight, SandShade);
		}
	}
	AddTower(B, -Gate - 120.0, W, 12.0, SandLight, SandShade);
	AddTower(B, Gate + 120.0, W, 12.0, SandLight, SandShade);
	// Puerta abierta: dos hojas de madera giradas hacia dentro.
	for (const double Side : { -1.0, 1.0 })
	{
		// Hacia dentro del castillo es +X de la maqueta: cada hoja se abre hacia dentro y hacia su lado.
		const FVector Hinge = ModelPoint(Side * Gate, W, 0.0);
		const FVector Dir = FVector(0.8, Side * 0.6, 0.0).GetSafeNormal2D();
		B.AddBox(Hinge + Dir * 3.6 + FVector(0.0, 0.0, 3.0), Dir, FVector(3.6, 0.4, 3.0), WoodDark);
	}

	// Sala de espera con los huevos junto a la puerta.
	uint32 Seed = 11u;
	for (const FVector2D EggSpot : { FVector2D(-260.0, 1950.0), FVector2D(-90.0, 2080.0), FVector2D(90.0, 2080.0), FVector2D(260.0, 1950.0) })
	{
		AddEgg(B, ModelPoint(EggSpot.X, EggSpot.Y, 2.4), 3.2, Seed++, Cream);
	}
	AddPin(B, ModelPoint(0.0, 1820.0, 2.4), Gold, WoodDark);

	// Tienda a la izquierda: puesto con toldo de rayas.
	{
		const FVector Shop = ModelPoint(900.0, 1500.0, 2.4);
		B.AddBox(Shop + FVector(0.0, 0.0, 1.2), AxisX, FVector(2.2, 3.6, 1.2), Pal(0xB07A4A));
		for (int32 k = 0; k < 4; ++k)
		{
			const double Y0 = -4.0 + k * 2.0;
			B.AddBox(Shop + FVector(-0.4, Y0 + 1.0, 4.6), AxisX, FVector(2.8, 1.0, 0.3), (k % 2) ? Cream : Coral);
		}
		AddPin(B, Shop + FVector(0.0, 0.0, 5.0), Coral, WoodDark);
	}

	// El general y su mesa a la derecha, y los probadores (botellas) detrás.
	{
		const FVector Here = ModelPoint(-892.0, 1479.0, 2.4);
		B.AddBox(Here + FVector(0.0, 0.0, 1.0), AxisX, FVector(1.6, 2.6, 1.0), WoodDark);
		AddPin(B, Here + FVector(-2.0, 0.0, 2.0), Navy, WoodDark);
	}
	for (const FVector2D BottleSpot : { FVector2D(-1400.0, 1100.0), FVector2D(-1800.0, 700.0), FVector2D(-1600.0, 200.0), FVector2D(-1800.0, -300.0) })
	{
		const FVector Foot = ModelPoint(BottleSpot.X, BottleSpot.Y, 2.4);
		TNProcMesh::TNProcAddCylinder(B, Foot, Foot + FVector(0.0, 0.0, 3.6), 1.5, 1.5, 8, Bottle);
		TNProcMesh::TNProcAddCylinder(B, Foot + FVector(0.0, 0.0, 3.6), Foot + FVector(0.0, 0.0, 5.4), 1.5, 0.6, 8, Bottle);
		TNProcMesh::TNProcAddCylinder(B, Foot + FVector(0.0, 0.0, 5.4), Foot + FVector(0.0, 0.0, 6.2), 0.6, 0.6, 6, Coral);
	}

	// Parkour: escalones, pilares de cubo y una pasarela a lo largo de las murallas.
	for (int32 s = 0; s < 5; ++s)
	{
		const FVector Step = ModelPoint(1400.0 + s * 180.0, -1900.0 + s * 260.0, 2.4);
		B.AddBox(Step + FVector(0.0, 0.0, 0.6 + s * 0.55), AxisX, FVector(2.0, 2.0, 0.6 + s * 0.55), SandShade);
	}
	for (int32 p = 0; p < 4; ++p)
	{
		const FVector Pillar = ModelPoint(-1500.0 + p * 700.0, -1700.0 + (p % 2) * 300.0, 2.4);
		TNProcMesh::TNProcAddCylinder(B, Pillar, Pillar + FVector(0.0, 0.0, 3.0 + p * 0.8), 1.8, 2.2, 8, (p % 2) ? Teal : Coral);
	}
	B.AddBox(ModelPoint(2000.0, 0.0, 6.8), FVector(0.0, 1.0, 0.0), FVector(18.0, 0.8, 0.25), WoodTop);

	// Puntero de madera con la punta roja y una taza de café a un lado.
	B.AddBeam(FVector(TableX + 40.0, -104.0, TableTop + 1.2), FVector(TableX - 8.0, -72.0, TableTop + 1.6), 0.9, WoodDark);
	B.AddBeam(FVector(TableX - 8.0, -72.0, TableTop + 1.6), FVector(TableX - 13.0, -69.0, TableTop + 1.7), 1.0, Coral);
	{
		const FVector Mug(TableX + 30.0, 98.0, TableTop);
		TNProcMesh::TNProcAddCylinder(B, Mug, Mug + FVector(0.0, 0.0, 10.0), 5.0, 5.4, 12, Cream);
		B.AddBeam(Mug + FVector(0.0, 5.6, 7.5), Mug + FVector(0.0, 8.4, 5.0), 0.9, Cream);
		B.AddBeam(Mug + FVector(0.0, 8.4, 5.0), Mug + FVector(0.0, 5.6, 2.5), 0.9, Cream);
	}

	// Cartel detrás del general (tablero azul marino con marco dorado sobre dos postes) y mástil con bandera.
	B.AddBox(FVector(SignX, 0.0, SignZ), AxisX, FVector(4.0, SignHalfW, SignHalfH), Navy);
	B.AddBox(FVector(SignX - 1.5, 0.0, SignZ), AxisX, FVector(4.0, SignHalfW + 7.0, SignHalfH + 7.0), Gold);
	for (const double LegY : { -SignHalfW * 0.7, SignHalfW * 0.7 })
	{
		B.AddBeam(FVector(SignX - 2.0, LegY, 0.0), FVector(SignX - 2.0, LegY, SignZ - SignHalfH), 4.5, WoodDark);
	}
	{
		const FVector PoleFoot(-70.0, -170.0, 0.0);
		const FVector PoleTop = PoleFoot + FVector(0.0, 0.0, 400.0);
		TNProcMesh::TNProcAddCylinder(B, PoleFoot, PoleTop, 4.0, 3.0, 8, Cream);
		TNProcMesh::TNProcAddCylinder(B, PoleTop, PoleTop + FVector(0.0, 0.0, 8.0), 6.0, 6.0, 8, Gold);
		const FVector F0 = PoleTop - FVector(0.0, 0.0, 12.0);
		const FVector F1 = F0 + FVector(0.0, 110.0, -8.0);
		const FVector F2 = F1 - FVector(0.0, 0.0, 70.0);
		const FVector F3 = F0 - FVector(0.0, 0.0, 74.0);
		B.AddQuad(F0, F1, F2, F3, FVector(1.0, 0.0, 0.0), Navy);
		B.AddQuad(F0, F1, F2, F3, FVector(-1.0, 0.0, 0.0), Navy * 0.85f);
		// Franja dorada en medio de la bandera (las dos caras).
		const FVector G0 = FMath::Lerp(F0, F3, 0.42), G1 = FMath::Lerp(F1, F2, 0.42);
		const FVector G2 = FMath::Lerp(F1, F2, 0.58), G3 = FMath::Lerp(F0, F3, 0.58);
		B.AddQuad(G0 + FVector(0.6, 0.0, 0.0), G1 + FVector(0.6, 0.0, 0.0), G2 + FVector(0.6, 0.0, 0.0), G3 + FVector(0.6, 0.0, 0.0), FVector(1.0, 0.0, 0.0), Gold);
		B.AddQuad(G0 - FVector(0.6, 0.0, 0.0), G1 - FVector(0.6, 0.0, 0.0), G2 - FVector(0.6, 0.0, 0.0), G3 - FVector(0.6, 0.0, 0.0), FVector(-1.0, 0.0, 0.0), Gold * 0.85f);
	}

	UMaterialInterface* VertexColorMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Cosmetics/Materials/M_CosmeticVertexColor.M_CosmeticVertexColor"));
	if (!VertexColorMat)
	{
		VertexColorMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
	}
	Table->SetStaticMesh(TNProcRuntimeMesh::MakeStaticMesh(this, B, VertexColorMat));
}

void ATN_GeneralBriefing::HideBlockout()
{
	// La tortuga de la maqueta queda debajo de este general y su mesa («Boolean», «Boolean2») al lado.
	for (TActorIterator<ASkeletalMeshActor> It(GetWorld()); It; ++It)
	{
		ASkeletalMeshActor* Blockout = *It;
		const USkeletalMeshComponent* Comp = Blockout ? Blockout->GetSkeletalMeshComponent() : nullptr;
		const USkinnedAsset* Asset = Comp ? Comp->GetSkinnedAsset() : nullptr;
		if (Asset && Asset->GetName().Contains(TEXT("TotugaDemo")) && FVector::Dist2D(Blockout->GetActorLocation(), GetActorLocation()) < 150.0)
		{
			Blockout->SetActorHiddenInGame(true);
			Blockout->SetActorEnableCollision(false);
		}
	}
	for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
	{
		AStaticMeshActor* Piece = *It;
		const UStaticMeshComponent* Comp = Piece ? Piece->GetStaticMeshComponent() : nullptr;
		const UStaticMesh* PieceMesh = Comp ? Comp->GetStaticMesh() : nullptr;
		const bool bTablePiece = Piece && (Piece->GetName().Contains(TEXT("Boolean")) || (PieceMesh && PieceMesh->GetName().Contains(TEXT("Boolean"))));
		if (bTablePiece && FVector::Dist2D(Piece->GetActorLocation(), GetActorLocation()) < 400.0)
		{
			Piece->SetActorHiddenInGame(true);
			Piece->SetActorEnableCollision(false);
		}
	}
}

void ATN_GeneralBriefing::OnInteracted_Implementation(APawn* Interactor)
{
	Super::OnInteracted_Implementation(Interactor);
	if (!HasAuthority() || !Interactor) { return; }
	if (AMP_GamePlayerController* PC = Cast<AMP_GamePlayerController>(Interactor->GetController()))
	{
		UE_LOG(LogTortunabo, Log, TEXT("[General] %s abre la sesión informativa."), *GetNameSafe(Interactor));
		PC->ClientOpenBriefing(this);
	}
}

void ATN_GeneralBriefing::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() == NM_DedicatedServer) { return; }

	// Se gira hacia el jugador local si está cerca y le saluda de vez en cuando (a lo militar).
	const APlayerController* LocalPC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const APawn* LocalPawn = LocalPC ? LocalPC->GetPawn() : nullptr;
	float TargetYaw = 0.f;
	if (LocalPawn)
	{
		const FVector Local = GetActorTransform().InverseTransformPosition(LocalPawn->GetActorLocation());
		const float Dist = Local.Size2D();
		if (Dist < 900.f && Local.X > -50.f)
		{
			TargetYaw = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X)), -55.f, 55.f);
			if (Dist < 650.f && SaluteCooldown <= 0.f && SaluteAnim)
			{
				General->PlayAnimation(SaluteAnim, false);
				SaluteTimeLeft = SaluteAnim->GetPlayLength();
				SaluteCooldown = 14.f;
			}
		}
	}
	LookYaw = FMath::FInterpTo(LookYaw, TargetYaw, DeltaSeconds, 3.f);
	General->SetRelativeRotation(FRotator(0.f, -90.f + LookYaw, 0.f));
	SaluteCooldown -= DeltaSeconds;
	if (SaluteTimeLeft > 0.f)
	{
		SaluteTimeLeft -= DeltaSeconds;
		if (SaluteTimeLeft <= 0.f && IdleAnim) { General->PlayAnimation(IdleAnim, true); }
	}
}
