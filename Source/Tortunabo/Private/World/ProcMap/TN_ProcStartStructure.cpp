#include "World/ProcMap/TN_ProcStartStructure.h"
#include "Core/TN_Log.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Scene.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"
#include "../../Lobby/TN_CastleKit.h"

namespace TNProcStartDetail
{
	/** Suelo de la sala y del montículo sobre el origen (que va a ras del terreno). */
	constexpr double StructureFloorZ = 4.0;
	/** Cuánto bajan paredes, pilares, torres y zócalo de la puerta doble por debajo del origen (terreno irregular). */
	constexpr double GatehouseSink = 450.0;
	/** La puerta 1 queda a esta distancia del borde del claro, hacia dentro (contra el talud). */
	constexpr double GateEdgeInset = 150.0;
	/** Montículo de los huevos: su centro, a esta distancia del borde del claro (y como mucho a MoundMaxBack del centro). */
	constexpr double MoundEdgeInset = 650.0;
	constexpr double MoundMaxBack = 1300.0;
	/** Holgura entre el suelo y la cápsula al colocar a un jugador. */
	constexpr double SpawnClearance = 2.0;
	/** Puerta 2: grados que gira cada hoja hacia fuera y segundos que tarda. */
	constexpr float GateOpenDegrees = 100.f;
	constexpr float GateOpenSeconds = 1.25f;
	/** Huevos: segundos entre que se rompe uno y el siguiente. */
	constexpr double HatchStagger = 0.12;
	/** Salto con el que sale cada tortuga de su huevo (cm/s) y a qué distancia de su eje se cuenta que está dentro. */
	constexpr float EggLaunchSpeedXY = 380.f;
	constexpr float EggLaunchSpeedZ = 620.f;
	constexpr double EggLaunchRadius = 120.0;
	/** Paredes invisibles de cada huevo: radio (algo más que la cáscara) y alto (más que cualquier salto). */
	constexpr double HolderRadius = 112.0;
	constexpr double HolderHeight = 700.0;
	/** Tapas: salto hacia arriba y hacia fuera (cm/s), gravedad (cm/s²), giros (grados/s) y lo que tardan en esfumarse. */
	constexpr double LidSpeedZ = 520.0;
	constexpr double LidSpeedXY = 340.0;
	constexpr double LidGravity = 1400.0;
	constexpr double LidSpinYaw = 540.0;
	constexpr double LidTumble = 320.0;
	constexpr double LidShrinkSeconds = 0.3;
	/** Altura del origen de la tapa (la costura) sobre el suelo al posarse: los dientes bajan 18 cm. */
	constexpr double LidRestHeight = 20.0;
	/** Giro de cada tapa cerrada: múltiplo de 22,5° para que sus dientes encajen entre los de la base. */
	constexpr double LidClosedYawStep = 45.0;

	static_assert(ATN_ProcStartStructure::NumSpots == TNCastleKit::EggMound::NumEggs, "Un sitio de salida por huevo de la pila.");

	/** Punto del suelo (local) del sitio Slot: en la sala de la puerta doble o en la base de su huevo. */
	FVector SpotLocal(ETNMatchStartStyle InStyle, int32 Slot)
	{
		if (InStyle == ETNMatchStartStyle::Gate)
		{
			return TNCastleKit::Gatehouse::SpawnSpot(Slot) + FVector(0.0, 0.0, StructureFloorZ);
		}
		return TNCastleKit::EggMoundSpot(Slot, FVector::ZeroVector, StructureFloorZ);
	}

	/** Hacia fuera de la pila (local, horizontal) desde el huevo Index del piso bajo (el mismo ángulo que EggMoundSpot). */
	FVector RadialLocal(int32 Index)
	{
		const double A = TNProcMap::TwoPi * Index / 3.0 + 0.35;
		return FVector(FMath::Cos(A), FMath::Sin(A), 0.0);
	}

	/**
	 * Dirección horizontal (local) con la que sale despedida la tortuga del huevo Index: la del piso de arriba, hacia el
	 * camino (+Y); las del piso bajo, a medias entre fuera de la pila y el camino (la de detrás sale de lado y no choca
	 * con el piso alto).
	 */
	FVector EggLaunchDirLocal(int32 Index)
	{
		const FVector Forward(0.0, 1.0, 0.0);
		if (Index >= TNCastleKit::EggMound::NumEggs - 1)
		{
			return Forward;
		}
		const FVector Radial = RadialLocal(Index);
		const FVector Blend = Radial + Forward;
		return Blend.SizeSquared() < 1e-4 ? Radial : Blend.GetSafeNormal();
	}

	/**
	 * Dirección horizontal (local) hacia la que salta la tapa del huevo Index: de lado respecto al salto de su tortuga (no
	 * la atraviesa) y hacia fuera de la pila; la de arriba, al hueco de atrás a la derecha, por donde no sale nadie.
	 */
	FVector LidDirLocal(int32 Index)
	{
		if (Index >= TNCastleKit::EggMound::NumEggs - 1)
		{
			const double A = FMath::DegreesToRadians(-65.0);
			return FVector(FMath::Cos(A), FMath::Sin(A), 0.0);
		}
		const FVector Hop = EggLaunchDirLocal(Index);
		const FVector Across(Hop.Y, -Hop.X, 0.0);
		return FVector::DotProduct(Across, RadialLocal(Index)) >= 0.0 ? Across : -Across;
	}

	/** Origen (local) de la tapa cerrada del huevo Index: el centro de su costura. */
	FVector LidClosedLocation(int32 Index)
	{
		return SpotLocal(ETNMatchStartStyle::Eggs, Index) + FVector(0.0, 0.0, TNCastleKit::EggSeam);
	}

	/** Segundos que vuela la tapa del huevo Index hasta posarse en el piso bajo del montículo o en la arena. */
	double LidFlightSeconds(int32 Index)
	{
		const FVector LidStart = LidClosedLocation(Index);
		const FVector Toward = LidDirLocal(Index);
		auto TimeToHeight = [&LidStart](double Height)
		{
			const double Drop = FMath::Max(0.0, LidStart.Z - Height);
			return (LidSpeedZ + FMath::Sqrt(LidSpeedZ * LidSpeedZ + 2.0 * LidGravity * Drop)) / LidGravity;
		};
		// Primero el piso bajo del montículo; si para entonces ya ha salido de él, cae hasta la arena.
		const double OnTier = TimeToHeight(StructureFloorZ + TNCastleKit::EggMound::Tier1H + LidRestHeight);
		const FVector AtTier = LidStart + Toward * (LidSpeedXY * OnTier);
		if (FVector2D(AtTier.X, AtTier.Y).Size() < TNCastleKit::EggMound::Tier1R)
		{
			return OnTier;
		}
		return TimeToHeight(StructureFloorZ + LidRestHeight);
	}

	/** Pared invisible (cilindro abierto por arriba) que sujeta a la tortuga dentro de su huevo hasta que se rompe. */
	void AddEggHolder(TNProcMesh::FTNProcMeshBuffers& Holder, const FVector& Spot)
	{
		constexpr int32 Seg = 12;
		const FVector Rise(0.0, 0.0, HolderHeight + 10.0);
		for (int32 k = 0; k < Seg; ++k)
		{
			const double A0 = TNProcMap::TwoPi * k / Seg;
			const double A1 = TNProcMap::TwoPi * (k + 1) / Seg;
			const FVector P0 = Spot + FVector(FMath::Cos(A0) * HolderRadius, FMath::Sin(A0) * HolderRadius, -10.0);
			const FVector P1 = Spot + FVector(FMath::Cos(A1) * HolderRadius, FMath::Sin(A1) * HolderRadius, -10.0);
			const double Am = (A0 + A1) * 0.5;
			// Cara hacia dentro (la colisión de la malla procedural es de doble cara).
			Holder.AddQuad(P0, P1, P1 + Rise, P0 + Rise, FVector(-FMath::Cos(Am), -FMath::Sin(Am), 0.0), TNCastleKit::SandC());
		}
	}
}

ATN_ProcStartStructure::ATN_ProcStartStructure()
{
	using namespace TNCastleKit;
	using namespace TNProcStartDetail;
	PrimaryActorTick.bCanEverTick = true;
	// Solo hace falta mientras se abre (hojas y tapas en movimiento, huevos que se rompen uno tras otro).
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	SetNetUpdateFrequency(5.f);

	StructureRoot = CreateDefaultSubobject<USceneComponent>(TEXT("StructureRoot"));
	SetRootComponent(StructureRoot);

	// Las mallas se generan en ejecución y no se guardan nunca: RF_Transient. El segundo parámetro de
	// CreateDefaultSubobject no la pone (solo evita copiar la plantilla del arquetipo).
	SolidMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SolidMesh"));
	SolidMesh->SetFlags(RF_Transient);
	SolidMesh->SetupAttachment(StructureRoot);
	// Colisión cocinada al momento: los jugadores aparecen sobre ella nada más crearse.
	SolidMesh->bUseAsyncCooking = false;
	SolidMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	DecorMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("DecorMesh"));
	DecorMesh->SetFlags(RF_Transient);
	DecorMesh->SetupAttachment(StructureRoot);
	DecorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DecorMesh->SetCastShadow(true);

	BarrierMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BarrierMesh"));
	BarrierMesh->SetFlags(RF_Transient);
	BarrierMesh->SetupAttachment(StructureRoot);
	BarrierMesh->bUseAsyncCooking = false;
	BarrierMesh->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	BarrierMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	BarrierMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	BarrierMesh->SetVisibility(false);
	BarrierMesh->SetHiddenInGame(true);

	auto MakeLeaf = [this](const TCHAR* Name)
	{
		UStaticMeshComponent* Leaf = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Leaf->SetupAttachment(StructureRoot);
		Leaf->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return Leaf;
	};
	Gate1LeafLeft = MakeLeaf(TEXT("Gate1LeafLeft"));
	Gate1LeafRight = MakeLeaf(TEXT("Gate1LeafRight"));
	Gate2LeafLeft = MakeLeaf(TEXT("Gate2LeafLeft"));
	Gate2LeafRight = MakeLeaf(TEXT("Gate2LeafRight"));

	// Las hojas no chocan: cada puerta la cierra una caja invisible del tamaño del hueco.
	auto MakeBlock = [this](const TCHAR* Name, double GateY)
	{
		UBoxComponent* Block = CreateDefaultSubobject<UBoxComponent>(Name);
		Block->SetupAttachment(StructureRoot);
		Block->InitBoxExtent(FVector(Gatehouse::HalfW, 40.0, Gatehouse::GateH * 0.5));
		Block->SetRelativeLocation(FVector(0.0, GateY, StructureFloorZ + Gatehouse::GateH * 0.5));
		Block->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
		Block->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		Block->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		Block->SetHiddenInGame(true);
		return Block;
	};
	Gate1Block = MakeBlock(TEXT("Gate1Block"), 0.0);
	Gate2Block = MakeBlock(TEXT("Gate2Block"), Gatehouse::Depth);

	for (int32 i = 0; i < NumSpots; ++i)
	{
		UStaticMeshComponent* Lid = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("EggLid%d"), i));
		Lid->SetupAttachment(StructureRoot);
		Lid->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		EggLids.Add(Lid);
	}

	GateSignText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("GateSignText"));
	GateSignText->SetupAttachment(StructureRoot);
	GateSignText->SetHorizontalAlignment(EHTA_Center);
	GateSignText->SetVerticalAlignment(EVRTA_TextCenter);
	GateSignText->SetTextRenderColor(FColor(255, 214, 90));
	GateSignText->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Luz cálida de antorcha, sin sombras (como la de la sala del lobby).
	RoomLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("RoomLight"));
	RoomLight->SetupAttachment(StructureRoot);
	RoomLight->SetRelativeLocation(FVector(0.0, Gatehouse::Depth * 0.5, StructureFloorZ + 420.0));
	RoomLight->SetIntensityUnits(ELightUnits::Lumens);
	RoomLight->SetIntensity(2600.f);
	RoomLight->SetAttenuationRadius(900.f);
	RoomLight->SetLightColor(FLinearColor(1.f, 0.7f, 0.42f));
	RoomLight->SetCastShadows(false);
}

void ATN_ProcStartStructure::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_ProcStartStructure, Style);
	DOREPLIFETIME(ATN_ProcStartStructure, bOpen);
}

double ATN_ProcStartStructure::GetBackDistance(ETNMatchStartStyle InStyle, double ClearingRadius)
{
	using namespace TNProcStartDetail;
	if (InStyle == ETNMatchStartStyle::Gate)
	{
		return FMath::Max(0.0, ClearingRadius - GateEdgeInset);
	}
	return FMath::Max(0.0, FMath::Min(ClearingRadius - MoundEdgeInset, MoundMaxBack));
}

void ATN_ProcStartStructure::GetFootprintSamples(ETNMatchStartStyle InStyle, TArray<FVector2D>& OutLocalPoints, bool& bOutUseHighest)
{
	using namespace TNCastleKit;
	OutLocalPoints.Reset();
	if (InStyle == ETNMatchStartStyle::Gate)
	{
		// Suelo de la sala y de los umbrales.
		bOutUseHighest = true;
		for (const double X : { -Gatehouse::FloorHalfW, 0.0, Gatehouse::FloorHalfW })
		{
			for (const double Y : { -Gatehouse::FacadeHalfT, Gatehouse::Depth * 0.5, Gatehouse::Depth + Gatehouse::FacadeHalfT })
			{
				OutLocalPoints.Add(FVector2D(X, Y));
			}
		}
		return;
	}
	// Base del piso bajo del montículo.
	bOutUseHighest = false;
	OutLocalPoints.Add(FVector2D::ZeroVector);
	for (int32 k = 0; k < 8; ++k)
	{
		const double A = TNProcMap::TwoPi * k / 8.0;
		OutLocalPoints.Add(FVector2D(FMath::Cos(A), FMath::Sin(A)) * EggMound::Tier1R);
	}
}

void ATN_ProcStartStructure::SetStyle(ETNMatchStartStyle InStyle)
{
	if (!HasAuthority() || Style == InStyle)
	{
		return;
	}
	Style = InStyle;
	if (HasActorBegunPlay())
	{
		OnRep_Style();
		ForceNetUpdate();
	}
}

void ATN_ProcStartStructure::BeginPlay()
{
	Super::BeginPlay();
	Build();
	// Quien la recibe ya abierta (se une tarde) la ve abierta del todo, sin que nadie salga despedido.
	if (bOpen)
	{
		StartOpening(false);
	}
	else
	{
		ApplyClosedPose();
	}
}

void ATN_ProcStartStructure::OnRep_Style()
{
	// Antes de BeginPlay no hace falta: BeginPlay construye con el estilo que haya llegado.
	if (!HasActorBegunPlay())
	{
		return;
	}
	Build();
	if (bOpen)
	{
		StartOpening(false);
	}
	else
	{
		ApplyClosedPose();
	}
}

void ATN_ProcStartStructure::OnRep_Open()
{
	if (!HasActorBegunPlay())
	{
		return;
	}
	if (bOpen)
	{
		StartOpening(true);
	}
	else
	{
		ApplyClosedPose();
	}
}

void ATN_ProcStartStructure::Open()
{
	if (!HasAuthority() || bOpen)
	{
		return;
	}
	bOpen = true;
	StartOpening(true);
	ForceNetUpdate();
	UE_LOG(LogTortunabo, Log, TEXT("[ProcMap] Se abre la salida (%s)."), Style == ETNMatchStartStyle::Eggs ? TEXT("huevos") : TEXT("puerta doble"));
}

void ATN_ProcStartStructure::Close()
{
	if (!HasAuthority() || !bOpen)
	{
		return;
	}
	bOpen = false;
	ApplyClosedPose();
	ForceNetUpdate();
}

bool ATN_ProcStartStructure::GetSpawnTransform(int32 Slot, float CapsuleHalfHeight, FTransform& OutTransform) const
{
	using namespace TNProcStartDetail;
	if (Slot < 0 || Slot >= NumSpots)
	{
		return false;
	}
	FVector Local = SpotLocal(Style, Slot);
	Local.Z += FMath::Max(0.0, static_cast<double>(CapsuleHalfHeight)) + SpawnClearance;
	const FTransform& Xf = GetActorTransform();
	// Mirando al camino: +Y local de la estructura.
	OutTransform = FTransform(FRotator(0.0, Xf.Rotator().Yaw + 90.0, 0.0), Xf.TransformPosition(Local));
	return true;
}

void ATN_ProcStartStructure::Build()
{
	using namespace TNCastleKit;
	using namespace TNProcStartDetail;
	UMaterialInterface* Mat = VertexColorMaterial();
	const bool bGate = Style == ETNMatchStartStyle::Gate;

	FBuffers Solid;
	FBuffers Decor;
	FBuffers Barrier;
	if (bGate)
	{
		// La misma puerta doble que la del lobby, con los muros hundidos en el terreno.
		BuildGatehouse(Solid, Decor, Barrier, FVector::ZeroVector, StructureFloorZ, 0, GatehouseSink);
	}
	else
	{
		// El mismo montículo con su pila de huevos que el del lobby, con el escalón hacia el camino.
		BuildEggMound(Solid, Decor, FVector::ZeroVector, StructureFloorZ, FVector(0.0, 1.0, 0.0));
		for (int32 i = 0; i < NumSpots; ++i)
		{
			const FVector Spot = SpotLocal(ETNMatchStartStyle::Eggs, i);
			BuildEggCup(Decor, Spot, Col(0xFFF3DC), Col(EggAccent(i)));
			AddEggHolder(Barrier, Spot);
		}
	}
	UploadSection(SolidMesh, Solid, true, Mat);
	UploadSection(DecorMesh, Decor, false, Mat);
	UploadSection(BarrierMesh, Barrier, true, nullptr);

	// Puerta doble: las hojas izquierdas (bisagra en -X) cerradas apuntan a +X; las derechas, a -X.
	UStaticMesh* LeafMesh = nullptr;
	if (bGate)
	{
		FBuffers Leaf;
		BuildLeaf(Leaf, Gatehouse::HalfW, Gatehouse::GateH);
		LeafMesh = TNProcRuntimeMesh::MakeStaticMesh(this, Leaf, Mat);
	}
	auto PlaceLeaf = [LeafMesh, bGate](UStaticMeshComponent* Leaf, double HingeX, double GateY, float ClosedYaw)
	{
		if (!Leaf) { return; }
		Leaf->SetStaticMesh(LeafMesh);
		Leaf->SetRelativeLocationAndRotation(FVector(HingeX, GateY, StructureFloorZ), FRotator(0.f, ClosedYaw, 0.f));
		Leaf->SetVisibility(bGate);
	};
	PlaceLeaf(Gate1LeafLeft, -Gatehouse::HalfW, 0.0, 0.f);
	PlaceLeaf(Gate1LeafRight, Gatehouse::HalfW, 0.0, 180.f);
	PlaceLeaf(Gate2LeafLeft, -Gatehouse::HalfW, Gatehouse::Depth, 0.f);
	PlaceLeaf(Gate2LeafRight, Gatehouse::HalfW, Gatehouse::Depth, 180.f);
	// La puerta 1 no se abre nunca; la 2 la gobierna ApplyClosedPose / StartOpening.
	if (Gate1Block)
	{
		Gate1Block->SetCollisionEnabled(bGate ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}

	// Huevos: una tapa por huevo, con el color de acento de cada uno.
	for (int32 i = 0; i < EggLids.Num(); ++i)
	{
		UStaticMeshComponent* Lid = EggLids[i];
		if (!Lid) { continue; }
		UStaticMesh* LidMesh = nullptr;
		if (!bGate && i < NumSpots)
		{
			FBuffers LidBuffers;
			BuildEggLid(LidBuffers, Pal(0xFFF3DC), Pal(EggAccent(i)));
			LidMesh = TNProcRuntimeMesh::MakeStaticMesh(this, LidBuffers, Mat);
		}
		Lid->SetStaticMesh(LidMesh);
		Lid->SetVisibility(LidMesh != nullptr);
	}

	// Rótulo del cartel de la puerta 2 por la cara del camino (+Y): siempre dentro de la tabla (se encoge si es largo).
	if (GateSignText)
	{
		GateSignText->SetVisibility(bGate);
		if (bGate)
		{
			GateSignText->SetRelativeLocationAndRotation(GatehouseSignText(Gatehouse::Depth, 1.0, StructureFloorZ), FRotator(0.f, 90.f, 0.f));
			GateSignText->SetWorldSize(88.f);
			GateSignText->SetText(NSLOCTEXT("Tortunabo", "ProcStartGateName", "TORTUNAVY"));
			const double SignWidth = GateSignText->GetTextLocalSize().Y;
			if (SignWidth > 540.0)
			{
				GateSignText->SetWorldSize(static_cast<float>(88.0 * 540.0 / SignWidth));
			}
		}
	}
	if (RoomLight)
	{
		RoomLight->SetVisibility(bGate);
	}
}

void ATN_ProcStartStructure::SetGate2Blocking(bool bBlock)
{
	if (Gate2Block)
	{
		const bool bOn = bBlock && Style == ETNMatchStartStyle::Gate;
		Gate2Block->SetCollisionEnabled(bOn ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
}

void ATN_ProcStartStructure::ApplyClosedPose()
{
	using namespace TNProcStartDetail;
	OpenStartTime = 0.0;
	HatchedMask = 0;
	bLaunchOnHatch = false;
	SetActorTickEnabled(false);

	if (Gate2LeafLeft) { Gate2LeafLeft->SetRelativeRotation(FRotator::ZeroRotator); }
	if (Gate2LeafRight) { Gate2LeafRight->SetRelativeRotation(FRotator(0.f, 180.f, 0.f)); }
	SetGate2Blocking(true);
	// Barreras de la sala (siempre) o paredes de los huevos (hasta que se rompen).
	if (BarrierMesh)
	{
		BarrierMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	const bool bEggs = Style == ETNMatchStartStyle::Eggs;
	for (int32 i = 0; i < EggLids.Num() && i < NumSpots; ++i)
	{
		UStaticMeshComponent* Lid = EggLids[i];
		if (!Lid) { continue; }
		Lid->SetRelativeLocationAndRotation(LidClosedLocation(i), FRotator(0.0, LidClosedYawStep * i, 0.0));
		Lid->SetRelativeScale3D(FVector::OneVector);
		Lid->SetVisibility(bEggs && Lid->GetStaticMesh() != nullptr);
	}
}

void ATN_ProcStartStructure::StartOpening(bool bLive)
{
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	// Sin vivirla (se une con la estructura ya abierta), directamente al final.
	OpenStartTime = bLive ? Now : Now - 1000.0;
	bLaunchOnHatch = bLive;
	HatchedMask = 0;
	// La puerta 2 deja de bloquear en cuanto empieza a abrirse: se sale mientras giran las hojas.
	SetGate2Blocking(false);
	SetActorTickEnabled(UpdateOpening());
}

void ATN_ProcStartStructure::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bOpen || !UpdateOpening())
	{
		SetActorTickEnabled(false);
	}
}

bool ATN_ProcStartStructure::UpdateOpening()
{
	using namespace TNProcStartDetail;
	const UWorld* World = GetWorld();
	const double Elapsed = (World ? World->GetTimeSeconds() : OpenStartTime) - OpenStartTime;

	if (Style == ETNMatchStartStyle::Gate)
	{
		// Puerta 2: las hojas giran hacia fuera (+Y), la izquierda a +Ángulo y la derecha a 180° - Ángulo.
		const float Alpha = static_cast<float>(FMath::Clamp(Elapsed / GateOpenSeconds, 0.0, 1.0));
		const float Angle = GateOpenDegrees * TNCastleKit::SmoothStep01(Alpha);
		if (Gate2LeafLeft) { Gate2LeafLeft->SetRelativeRotation(FRotator(0.f, Angle, 0.f)); }
		if (Gate2LeafRight) { Gate2LeafRight->SetRelativeRotation(FRotator(0.f, 180.f - Angle, 0.f)); }
		return Alpha < 1.f;
	}

	// Huevos: se rompen uno tras otro; cada tapa salta, vuela dando vueltas, se posa y se esfuma.
	bool bRunning = false;
	for (int32 i = 0; i < NumSpots; ++i)
	{
		const double T = Elapsed - HatchStagger * i;
		if (T < 0.0)
		{
			bRunning = true;
			continue;
		}
		if ((HatchedMask & (1 << i)) == 0)
		{
			HatchedMask |= 1 << i;
			HatchEgg(i);
		}
		const bool bLidAlive = PoseLid(i, T);
		bRunning = bRunning || bLidAlive;
	}
	return bRunning;
}

void ATN_ProcStartStructure::HatchEgg(int32 Index)
{
	using namespace TNProcStartDetail;
	// Las paredes invisibles de los huevos sueltan en cuanto se rompe el primero.
	if (BarrierMesh && Style == ETNMatchStartStyle::Eggs)
	{
		BarrierMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	UWorld* World = GetWorld();
	if (!bLaunchOnHatch || !World)
	{
		return;
	}

	const FTransform& Xf = GetActorTransform();
	const FVector Spot = Xf.TransformPosition(SpotLocal(ETNMatchStartStyle::Eggs, Index));
	const FVector HopVelocity = Xf.TransformVectorNoScale(EggLaunchDirLocal(Index)) * EggLaunchSpeedXY + FVector(0.0, 0.0, EggLaunchSpeedZ);
	// Solo mueve a la tortuga quien la controla, a la vez: el servidor (a todas) y cada cliente (a la suya, al recibir
	// bOpen). En el cliente, el iterador solo tiene sus controladores locales.
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		ACharacter* Turtle = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
		if (!Turtle || !(HasAuthority() || Turtle->IsLocallyControlled()))
		{
			continue;
		}
		const FVector Where = Turtle->GetActorLocation();
		if (FVector::Dist2D(Where, Spot) > EggLaunchRadius || Where.Z < Spot.Z - 60.0 || Where.Z > Spot.Z + 450.0)
		{
			continue;
		}
		Turtle->LaunchCharacter(HopVelocity, true, true);
	}
}

bool ATN_ProcStartStructure::PoseLid(int32 Index, double T)
{
	using namespace TNProcStartDetail;
	UStaticMeshComponent* Lid = EggLids.IsValidIndex(Index) ? EggLids[Index].Get() : nullptr;
	if (!Lid || !Lid->GetStaticMesh())
	{
		return false;
	}
	const double Flight = LidFlightSeconds(Index);
	const double Shrink = T > Flight ? 1.0 - (T - Flight) / LidShrinkSeconds : 1.0;
	if (Shrink <= 0.0)
	{
		Lid->SetVisibility(false);
		return false;
	}
	// Tiro parabólico hacia fuera dando vueltas; al posarse se queda donde cae y encoge hasta desaparecer.
	const double Tf = FMath::Min(T, Flight);
	const FVector Where = LidClosedLocation(Index) + LidDirLocal(Index) * (LidSpeedXY * Tf)
		+ FVector(0.0, 0.0, LidSpeedZ * Tf - 0.5 * LidGravity * Tf * Tf);
	const FRotator Spin(LidTumble * Tf, LidClosedYawStep * Index + LidSpinYaw * Tf, 0.0);
	Lid->SetVisibility(true);
	Lid->SetRelativeLocationAndRotation(Where, Spin);
	Lid->SetRelativeScale3D(FVector(Shrink));
	return true;
}
