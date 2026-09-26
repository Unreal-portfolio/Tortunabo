#include "Lobby/TN_ChangingBooth.h"
#include "Core/TN_Log.h"
#include "Player/MP_GamePlayerController.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "../World/ProcMap/TN_ProcMapRuntimeMesh.h"

namespace TNBoothDetail
{
	/**
	 * Media botella boca abajo: la mitad del culo, cortada y puesta sobre el corte. El culo (con su hundido) hace de
	 * techo y la puerta es el tapón (una chapa de corona). La puerta mira a +X; su centro y su radio se miden sobre la
	 * pared (arco y altura).
	 */
	constexpr double WallR = 150.0;
	constexpr double WallTop = 262.0;
	constexpr double DoorR = 88.0;
	constexpr double DoorZ = 114.0;
	/** Cara de la chapa, por fuera de la pared (la falda rizada va de la cara a la pared). */
	constexpr double CapFaceR = WallR + 16.0;
	constexpr int32 Seg = 32;
	constexpr int32 CapFlutes = 21;
	constexpr float DoorOpenYaw = -108.f;
	constexpr float HopDelay = 0.4f;

	FLinearColor Pal(uint32 Hex)
	{
		return FLinearColor(TNProcRuntimeMesh::SRGBToLinear(((Hex >> 16) & 255) / 255.f), TNProcRuntimeMesh::SRGBToLinear(((Hex >> 8) & 255) / 255.f),
			TNProcRuntimeMesh::SRGBToLinear((Hex & 255) / 255.f), 1.f);
	}

	/** Punto de un cilindro de radio R: S = arco desde el centro de la puerta (+X), Z = altura. */
	FVector OnCylinder(double R, double S, double Z)
	{
		const double A = S / R;
		return FVector(R * FMath::Cos(A), R * FMath::Sin(A), Z);
	}

	FVector RadialOut(const FVector& P)
	{
		return FVector(P.X, P.Y, 0.0).GetSafeNormal();
	}

	/** Bisagra de la chapa (a la izquierda vista desde fuera, por fuera de la falda): el giro negativo la abre. */
	FVector HingePoint()
	{
		const FVector H = OnCylinder(CapFaceR, -DoorR - 10.0, 0.0);
		return FVector(H.X, H.Y, 0.0);
	}
}

ATN_ChangingBooth::ATN_ChangingBooth()
{
	using namespace TNBoothDetail;
	PrimaryActorTick.bCanEverTick = true;
	NetDormancy = DORM_Awake;
	bAlwaysRelevant = true;
	CooldownSeconds = 0.8f;
	PromptText = NSLOCTEXT("Tortunabo", "BoothPrompt", "Entrar al probador");

	// Hitbox de interacción (invisible) delante de la puerta.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Mesh && Cylinder.Succeeded())
	{
		Mesh->SetStaticMesh(Cylinder.Object);
		Mesh->SetRelativeLocation(FVector(WallR + 50.0, 0.0, 60.0));
		Mesh->SetRelativeScale3D(FVector(1.6f, 1.6f, 1.2f));
		Mesh->SetHiddenInGame(true);
	}
	if (PromptWidgetComponent)
	{
		PromptWidgetComponent->SetUsingAbsoluteScale(true);
		PromptWidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, 170.f));
	}

	Bottle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bottle"));
	Bottle->SetupAttachment(SceneRoot);
	Bottle->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	DoorHinge = CreateDefaultSubobject<USceneComponent>(TEXT("DoorHinge"));
	DoorHinge->SetupAttachment(Bottle);
	DoorHinge->SetRelativeLocation(HingePoint());

	Door = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Door"));
	Door->SetupAttachment(DoorHinge);
	Door->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Paredes: cuatro cajas giradas 45° forman un octógono de la altura de la botella (una cápsula tan ancha se
	// estrecharía abajo y dejaría meter los pies).
	for (int32 k = 0; k < 4; ++k)
	{
		UBoxComponent* Side = CreateDefaultSubobject<UBoxComponent>(*FString::Printf(TEXT("Walls%d"), k));
		Side->SetupAttachment(SceneRoot);
		Side->InitBoxExtent(FVector(WallR, WallR * 0.4142, (WallTop + 60.0) * 0.5));
		Side->SetRelativeLocation(FVector(0.0, 0.0, (WallTop + 60.0) * 0.5));
		Side->SetRelativeRotation(FRotator(0.0, 45.0 * k, 0.0));
		Side->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
		Side->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		Walls.Add(Side);
	}

	// "La cámara se aleja": vista desde fuera, de tres cuartos, con la botella entera.
	ViewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ViewCamera"));
	ViewCamera->SetupAttachment(SceneRoot);
	const FVector CamPos(720.0, -320.0, 260.0);
	ViewCamera->SetRelativeLocation(CamPos);
	ViewCamera->SetRelativeRotation((FVector(30.0, 0.0, 170.0) - CamPos).Rotation());
	ViewCamera->SetFieldOfView(62.f);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Bottle);
	Label->SetRelativeLocation(FVector(WallR + 5.0, 0.0, DoorZ + DoorR + 30.0));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(28.f);
	Label->SetTextRenderColor(FColor(255, 251, 240));
	Label->SetText(NSLOCTEXT("Tortunabo", "BoothLabel", "PROBADOR"));
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ATN_ChangingBooth::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_ChangingBooth, Occupant);
}

void ATN_ChangingBooth::BeginPlay()
{
	Super::BeginPlay();
	BuildMeshes();
	HideBlockout();
}

bool ATN_ChangingBooth::CanInteract(APawn* Interactor) const
{
	return Occupant == nullptr && Super::CanInteract(Interactor);
}

void ATN_ChangingBooth::BuildMeshes()
{
	using namespace TNBoothDetail;
	UMaterialInterface* VertexColorMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Cosmetics/Materials/M_CosmeticVertexColor.M_CosmeticVertexColor"));
	const FLinearColor Glass = Pal(0x6CCDB5);
	const FLinearColor GlassDark = Pal(0x3E9C88);
	const FLinearColor GlassLight = Pal(0xB4F0E0);
	const FLinearColor GlassEdge = Pal(0xCFF8EC);
	const FLinearColor Sand = Pal(0xF2D49B);

	TNProcMesh::FTNProcMeshBuffers B;
	// ── Media botella: perfil (radio, z) de abajo arriba. El tramo recto se subdivide para recortar el hueco de la
	// puerta; arriba, el canto redondeado del culo y su hundido (el techo).
	TArray<FVector2D> Profile = { FVector2D(WallR, 12.0) };
	for (int32 i = 1; i <= 14; ++i) { Profile.Add(FVector2D(WallR, 12.0 + (WallTop - 12.0) * i / 14.0)); }
	Profile.Append({ FVector2D(148.0, 285.0), FVector2D(140.0, 302.0), FVector2D(126.0, 314.0), FVector2D(106.0, 321.0),
		FVector2D(72.0, 323.0), FVector2D(42.0, 317.0), FVector2D(16.0, 309.0) });
	auto RingPoint = [](const FVector2D& P, int32 K) { const double A = TNProcMap::TwoPi * K / Seg; return FVector(P.X * FMath::Cos(A), P.X * FMath::Sin(A), P.Y); };
	for (int32 i = 0; i + 1 < Profile.Num(); ++i)
	{
		for (int32 k = 0; k < Seg; ++k)
		{
			const FVector A = RingPoint(Profile[i], k), Bv = RingPoint(Profile[i], k + 1);
			const FVector C = RingPoint(Profile[i + 1], k + 1), D = RingPoint(Profile[i + 1], k);
			const FVector Mid = (A + Bv + C + D) * 0.25;
			double Ang = TNProcMap::TwoPi * (k + 0.5) / Seg;
			if (Ang > PI) { Ang -= TNProcMap::TwoPi; }
			const double Arc = Ang * FVector(Mid.X, Mid.Y, 0.0).Size();
			if (FMath::Square(Arc) + FMath::Square(Mid.Z - DoorZ) < FMath::Square(DoorR - 4.0)) { continue; }
			const bool bRoof = Profile[i].Y >= WallTop - 1.0;
			FLinearColor Col = bRoof ? TNProcMesh::TNProcLerpColor(Glass, GlassLight, 0.25f) : Glass;
			if (!bRoof && (k % 8 == 2 || k % 8 == 3)) { Col = TNProcMesh::TNProcLerpColor(Col, GlassLight, 0.55f); }
			// Hacia fuera en la pared; en el techo, hacia arriba (el hundido del culo también se ve desde arriba).
			const FVector Out = bRoof ? FVector(Mid.X * 0.004, Mid.Y * 0.004, 1.0) : RadialOut(Mid);
			B.AddQuad(A, Bv, C, D, Out, Col);
			B.AddQuad(A, Bv, C, D, -Out, GlassDark * 0.8f);
		}
	}
	// Tapa del hundido del techo, canto del corte a ras de suelo (vidrio grueso, más claro) y suelo de arena.
	for (int32 k = 0; k < Seg; ++k)
	{
		B.AddTri(FVector(0.0, 0.0, 305.0), RingPoint(Profile.Last(), k), RingPoint(Profile.Last(), k + 1), FVector::UpVector, TNProcMesh::TNProcLerpColor(Glass, GlassLight, 0.4f));
		const FVector R0 = RingPoint(FVector2D(WallR + 5.0, 0.0), k), R1 = RingPoint(FVector2D(WallR + 5.0, 0.0), k + 1);
		const FVector T0 = RingPoint(FVector2D(WallR + 5.0, 14.0), k), T1 = RingPoint(FVector2D(WallR + 5.0, 14.0), k + 1);
		const FVector I0 = RingPoint(FVector2D(WallR - 1.0, 14.0), k), I1 = RingPoint(FVector2D(WallR - 1.0, 14.0), k + 1);
		B.AddQuad(R0, R1, T1, T0, RadialOut((R0 + T1) * 0.5), GlassEdge);
		B.AddQuad(T0, T1, I1, I0, FVector::UpVector, GlassEdge * 1.05f);
		B.AddTri(FVector(0.0, 0.0, 2.0), RingPoint(FVector2D(WallR - 2.0, 2.0), k), RingPoint(FVector2D(WallR - 2.0, 2.0), k + 1), FVector::UpVector, Sand);
	}
	// Boca de vidrio grueso alrededor del hueco (tapa los dientes del recorte) con canto hacia dentro y hacia fuera.
	constexpr int32 LipSeg = 40;
	const FVector Axis = OnCylinder(WallR, 0.0, DoorZ);
	for (int32 j = 0; j < LipSeg; ++j)
	{
		const double P0 = TNProcMap::TwoPi * j / LipSeg, P1 = TNProcMap::TwoPi * (j + 1) / LipSeg;
		auto LipPt = [](double R, double Phi, double Radius) { return OnCylinder(Radius, R * FMath::Cos(Phi), DoorZ + R * FMath::Sin(Phi)); };
		const FVector I0 = LipPt(DoorR - 22.0, P0, WallR + 4.0), I1 = LipPt(DoorR - 22.0, P1, WallR + 4.0);
		const FVector O0 = LipPt(DoorR + 7.0, P0, WallR + 4.0), O1 = LipPt(DoorR + 7.0, P1, WallR + 4.0);
		B.AddQuad(I0, I1, O1, O0, RadialOut((I0 + O1) * 0.5), GlassEdge);
		const FVector O0b = LipPt(DoorR + 7.0, P0, WallR), O1b = LipPt(DoorR + 7.0, P1, WallR);
		B.AddQuad(O0, O1, O1b, O0b, (O0 + O1) * 0.5 - Axis, GlassLight);
		const FVector J0 = LipPt(DoorR - 22.0, P0, WallR - 10.0), J1 = LipPt(DoorR - 22.0, P1, WallR - 10.0);
		B.AddQuad(I0, I1, J1, J0, Axis - (I0 + J1) * 0.5, GlassLight * 0.85f);
	}
	Bottle->SetStaticMesh(TNProcRuntimeMesh::MakeStaticMesh(this, B, VertexColorMat));

	// ── Puerta: el tapón, una chapa de corona roja con su estrella (en el espacio de la bisagra) ──
	TNProcMesh::FTNProcMeshBuffers D;
	const FVector Hinge = HingePoint();
	auto CapPt = [&Hinge](double Radius, double S, double Z) { return OnCylinder(Radius, S, Z) - Hinge; };
	const FLinearColor CapRed = Pal(0xD7263D);
	const FLinearColor CapRedDark = Pal(0xA3162B);
	const FLinearColor CapCream = Pal(0xFFF2D4);
	const FLinearColor CapGold = Pal(0xFFCB3D);
	const FLinearColor Liner = Pal(0xE9DCC0);
	const double Radii[5] = { 0.0, 0.34, 0.62, 0.78, 1.0 };
	constexpr int32 CapSeg = 42;
	for (int32 r = 0; r < 4; ++r)
	{
		for (int32 j = 0; j < CapSeg; ++j)
		{
			const double P0 = TNProcMap::TwoPi * j / CapSeg, P1 = TNProcMap::TwoPi * (j + 1) / CapSeg;
			const double Ra = Radii[r] * DoorR, Rb = Radii[r + 1] * DoorR;
			// La cara abomba un poco hacia fuera en el centro, como una chapa.
			const double Da = 5.0 * (1.0 - Radii[r] * Radii[r]), Db = 5.0 * (1.0 - Radii[r + 1] * Radii[r + 1]);
			const FVector A = CapPt(CapFaceR + Da, Ra * FMath::Cos(P0), DoorZ + Ra * FMath::Sin(P0));
			const FVector Bv = CapPt(CapFaceR + Da, Ra * FMath::Cos(P1), DoorZ + Ra * FMath::Sin(P1));
			const FVector C = CapPt(CapFaceR + Db, Rb * FMath::Cos(P1), DoorZ + Rb * FMath::Sin(P1));
			const FVector Dv = CapPt(CapFaceR + Db, Rb * FMath::Cos(P0), DoorZ + Rb * FMath::Sin(P0));
			const FVector Out = RadialOut((A + C) * 0.5 + Hinge);
			D.AddQuad(A, Bv, C, Dv, Out, r == 2 ? CapCream : CapRed);
			// Forro de dentro (se ve con la puerta abierta).
			const FVector Back = -Out * 14.0;
			D.AddQuad(A + Back, Bv + Back, C + Back, Dv + Back, -Out, Liner);
		}
	}
	// Estrella dorada en relieve en el centro de la chapa.
	{
		const FVector Center = CapPt(CapFaceR + 6.5, 0.0, DoorZ);
		const FVector Out = RadialOut(Center + Hinge);
		TArray<FVector> Star;
		for (int32 i = 0; i < 10; ++i)
		{
			const double A = HALF_PI + PI * i / 5.0;
			const double R = (i % 2) ? 12.0 : 30.0;
			Star.Add(CapPt(CapFaceR + 6.5, R * FMath::Cos(A), DoorZ + R * FMath::Sin(A)));
		}
		for (int32 i = 0; i < 10; ++i)
		{
			const FVector& S0 = Star[i];
			const FVector& S1 = Star[(i + 1) % 10];
			D.AddTri(Center + Out * 1.5, S0, S1, Out, CapGold);
			D.AddQuad(S0, S1, S1 - Out * 2.0, S0 - Out * 2.0, (S0 + S1) * 0.5 - Center, CapGold * 0.8f);
		}
	}
	// Falda rizada de la chapa (21 pliegues) desde la cara hasta la pared, y borde de la cara.
	constexpr int32 Crimp = CapFlutes * 2;
	for (int32 j = 0; j < Crimp; ++j)
	{
		const double P0 = TNProcMap::TwoPi * j / Crimp, P1 = TNProcMap::TwoPi * (j + 1) / Crimp;
		const double R0 = DoorR + ((j % 2) ? 9.0 : 2.0), R1 = DoorR + (((j + 1) % 2) ? 9.0 : 2.0);
		const FVector F0 = CapPt(CapFaceR, R0 * FMath::Cos(P0), DoorZ + R0 * FMath::Sin(P0));
		const FVector F1 = CapPt(CapFaceR, R1 * FMath::Cos(P1), DoorZ + R1 * FMath::Sin(P1));
		const FVector W0 = CapPt(WallR + 1.0, (R0 + 5.0) * FMath::Cos(P0), DoorZ + (R0 + 5.0) * FMath::Sin(P0));
		const FVector W1 = CapPt(WallR + 1.0, (R1 + 5.0) * FMath::Cos(P1), DoorZ + (R1 + 5.0) * FMath::Sin(P1));
		const FVector Radial = RadialOut(F0 + Hinge);
		const double PhiMid = (P0 + P1) * 0.5;
		const FVector EdgeOut = FVector(0.0, 0.0, FMath::Sin(PhiMid)) + FVector(-Radial.Y, Radial.X, 0.0) * FMath::Cos(PhiMid);
		D.AddQuad(F0, F1, W1, W0, EdgeOut, (j % 2) ? CapRedDark : CapRed * 1.08f);
		const FVector E0 = CapPt(CapFaceR, DoorR * FMath::Cos(P0), DoorZ + DoorR * FMath::Sin(P0));
		const FVector E1 = CapPt(CapFaceR, DoorR * FMath::Cos(P1), DoorZ + DoorR * FMath::Sin(P1));
		D.AddQuad(E0, E1, F1, F0, Radial, CapRed);
	}
	Door->SetStaticMesh(TNProcRuntimeMesh::MakeStaticMesh(this, D, VertexColorMat));
}

void ATN_ChangingBooth::HideBlockout()
{
	// Las piezas de la maqueta que esta botella sustituye (la botella gris y la puerta de prueba) se esconden.
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Other = *It;
		if (!Other || Other == this) { continue; }
		const FString ClassName = Other->GetClass()->GetName();
		const double Dist = FVector::Dist2D(Other->GetActorLocation(), GetActorLocation());
		const bool bBottle = ClassName.Contains(TEXT("VestidorBotella")) && Dist < 200.0;
		const bool bDoor = ClassName.Contains(TEXT("ShellDoor")) && Dist < 450.0;
		if (bBottle || bDoor)
		{
			Other->SetActorHiddenInGame(true);
			Other->SetActorEnableCollision(false);
		}
	}
}

void ATN_ChangingBooth::OnInteracted_Implementation(APawn* Interactor)
{
	Super::OnInteracted_Implementation(Interactor);
	if (!HasAuthority() || !Interactor || Occupant) { return; }
	AMP_GamePlayerController* PC = Cast<AMP_GamePlayerController>(Interactor->GetController());
	if (!PC) { return; }

	Occupant = Interactor;
	OnRep_Occupant();
	ForceNetUpdate();
	UE_LOG(LogTortunabo, Log, TEXT("[Probador] %s entra en %s."), *GetNameSafe(Interactor), *GetName());
	PC->ClientOpenBooth(this);
}

void ATN_ChangingBooth::ReleaseOccupant(APawn* Pawn)
{
	if (!HasAuthority() || !Pawn || Pawn != Occupant) { return; }
	Occupant = nullptr;
	OnRep_Occupant();
	ForceNetUpdate();
	UE_LOG(LogTortunabo, Log, TEXT("[Probador] %s sale de %s."), *GetNameSafe(Pawn), *GetName());
}

void ATN_ChangingBooth::OnRep_Occupant()
{
	using namespace TNBoothDetail;
	WobbleTime = 1.2f;

	// Solo mueve a la tortuga quien la controla: el servidor (autoridad) y su cliente (predicción), a la vez.
	const FRotator Facing(0.f, GetActorRotation().Yaw, 0.f);
	auto Controls = [this](const APawn* P) { return P && (HasAuthority() || P->IsLocallyControlled()); };

	if (Occupant && Controls(Occupant))
	{
		// Dentro: centrada y mirando a la puerta; ignora las paredes mientras esté aquí.
		Occupant->MoveIgnoreActorAdd(this);
		if (ACharacter* Char = Cast<ACharacter>(Occupant.Get())) { Char->GetCharacterMovement()->StopMovementImmediately(); }
		Occupant->SetActorLocationAndRotation(GetActorTransform().TransformPosition(FVector(0.0, 0.0, 96.0)), Facing, false, nullptr, ETeleportType::TeleportPhysics);
	}
	APawn* Leaving = (!Occupant && PreviousOccupant.IsValid()) ? PreviousOccupant.Get() : nullptr;
	PreviousOccupant = Occupant.Get();
	if (Leaving && Controls(Leaving) && GetWorld())
	{
		// Sale de un saltito por la puerta cuando ya se ha abierto un poco, y luego vuelve a chocar con la botella.
		TWeakObjectPtr<APawn> WeakPawn(Leaving);
		TWeakObjectPtr<ATN_ChangingBooth> WeakBooth(this);
		FTimerHandle HopTimer;
		GetWorldTimerManager().SetTimer(HopTimer, FTimerDelegate::CreateLambda([WeakPawn, WeakBooth, Facing]()
		{
			APawn* P = WeakPawn.Get();
			ATN_ChangingBooth* Booth = WeakBooth.Get();
			if (!P || !Booth) { return; }
			P->SetActorRotation(Facing);
			if (ACharacter* Char = Cast<ACharacter>(P)) { Char->LaunchCharacter(Facing.Vector() * 540.f + FVector(0.f, 0.f, 420.f), true, true); }
			FTimerHandle RestoreTimer;
			Booth->GetWorldTimerManager().SetTimer(RestoreTimer, FTimerDelegate::CreateLambda([WeakPawn, WeakBooth]()
			{
				if (APawn* Q = WeakPawn.Get()) { Q->MoveIgnoreActorRemove(WeakBooth.Get()); }
			}), 1.1f, false);
		}), HopDelay, false);
	}
}

void ATN_ChangingBooth::Tick(float DeltaSeconds)
{
	using namespace TNBoothDetail;
	Super::Tick(DeltaSeconds);

	// Si quien estaba dentro se ha ido (desconexión, viaje), la puerta se vuelve a abrir.
	if (HasAuthority() && Occupant && (!IsValid(Occupant) || !Occupant->GetController()))
	{
		Occupant = nullptr;
		OnRep_Occupant();
		ForceNetUpdate();
	}

	const float Target = Occupant ? 0.f : 1.f;
	DoorOpen = FMath::FInterpConstantTo(DoorOpen, Target, DeltaSeconds, 1.9f);
	DoorHinge->SetRelativeRotation(FRotator(0.f, DoorOpenYaw * FMath::SmoothStep(0.f, 1.f, DoorOpen), 0.f));

	// Meneo de la botella: al cerrar/abrir y, mientras alguien se cambia, un traqueteo de vez en cuando.
	if (Occupant)
	{
		ShuffleClock += DeltaSeconds;
		if (ShuffleClock > 2.3f) { ShuffleClock = 0.f; WobbleTime = FMath::Max(WobbleTime, 0.7f); }
	}
	float Squash = 0.f;
	if (WobbleTime > 0.f)
	{
		WobbleTime = FMath::Max(0.f, WobbleTime - DeltaSeconds);
		Squash = 0.035f * FMath::Sin(WobbleTime * 22.f) * FMath::Min(1.f, WobbleTime);
	}
	Bottle->SetRelativeScale3D(FVector(1.f + Squash, 1.f + Squash, 1.f - Squash * 0.8f));
}
