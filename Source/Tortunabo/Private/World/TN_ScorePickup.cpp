#include "World/TN_ScorePickup.h"
#include "Core/TN_Log.h"
#include "Core/TN_CoopPlayerState.h"
#include "Player/TortugaCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "ProcMap/TN_ProcMapMeshKit.h"
#include "ProcMap/TN_ProcMapRuntimeMesh.h"
#include "ProcMap/TN_ProcMapAmbientFX.h"

namespace
{
	/**
	 * Concha de vieira de unos 70 cm, de pie en el plano XZ (se ve de frente desde ±Y): valvas abombadas por las dos
	 * caras con costillas en abanico, borde festoneado y orejetas en la charnela. Colores pensados para M_ProcGlow
	 * (emisivo = color del vértice x 2): concha dorada como las monedas de los plataformas de siempre, ámbar en la
	 * charnela y oro claro en el borde. Una sola malla en caché para todas las conchas.
	 */
	UStaticMesh* ShellCoinMesh()
	{
		static TWeakObjectPtr<UStaticMesh> CachedGold;
		if (CachedGold.IsValid()) { return CachedGold.Get(); }
		UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ProcMap/Materials/M_ProcGlow.M_ProcGlow"));
		if (!Mat) { Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ProcMap/Materials/M_ProcFoliage.M_ProcFoliage")); }
		if (!Mat) { return nullptr; }

		TNProcMesh::FTNProcMeshBuffers M;
		constexpr int32 Fan = 18;
		constexpr int32 Rings = 5;
		constexpr int32 Ribs = 9;
		const double Spread = FMath::DegreesToRadians(80.0);
		const FVector Hinge(0.0, 0.0, -26.0);
		auto EdgeR = [&](double A)
		{
			const double Rib = FMath::Abs(FMath::Sin((A / Spread + 1.0) * 0.5 * Ribs * PI));
			return 34.0 * (1.0 + 0.07 * Rib);
		};
		auto Point = [&](int32 i, int32 j, double Side)
		{
			const double A = FMath::Lerp(-Spread, Spread, static_cast<double>(i) / Fan);
			const double T = static_cast<double>(j) / Rings;
			const double R = EdgeR(A) * T;
			const double Rib = FMath::Abs(FMath::Sin((A / Spread + 1.0) * 0.5 * Ribs * PI));
			const double Bulge = (7.5 * (1.0 - T * T) + 1.6 * Rib * T) * Side;
			return Hinge + FVector(FMath::Sin(A) * R, Bulge, FMath::Cos(A) * R);
		};
		auto ColorAt = [&](int32 i, int32 j)
		{
			const float T = static_cast<float>(j) / Rings;
			FLinearColor C = FMath::Lerp(FLinearColor(0.6f, 0.27f, 0.07f), FLinearColor(0.78f, 0.55f, 0.16f), T);
			if (((i + 1) / 2) % 2 == 0) { C *= 0.82f; }
			if (j == Rings) { C = FLinearColor(0.85f, 0.72f, 0.3f); }
			return C;
		};
		for (const double Side : { 1.0, -1.0 })
		{
			const FVector Out(0.0, Side, 0.0);
			for (int32 j = 0; j < Rings; ++j)
			{
				for (int32 i = 0; i < Fan; ++i)
				{
					M.AddQuad(Point(i, j, Side), Point(i + 1, j, Side), Point(i + 1, j + 1, Side), Point(i, j + 1, Side), Out, ColorAt(i, j + 1));
				}
			}
		}
		// Canto: une las dos valvas por el borde festoneado.
		for (int32 i = 0; i < Fan; ++i)
		{
			const double Am = FMath::Lerp(-Spread, Spread, (i + 0.5) / Fan);
			const FVector Radial(FMath::Sin(Am), 0.0, FMath::Cos(Am));
			M.AddQuad(Point(i, Rings, 1.0), Point(i + 1, Rings, 1.0), Point(i + 1, Rings, -1.0), Point(i, Rings, -1.0), Radial, FLinearColor(0.85f, 0.7f, 0.25f));
		}
		// Orejetas de la charnela.
		for (const double Sx : { -1.0, 1.0 })
		{
			const FVector A = Hinge + FVector(Sx * 3.0, 0.0, 2.0);
			const FVector B = Hinge + FVector(Sx * 17.0, 0.0, 6.0);
			const FVector C = Hinge + FVector(Sx * 14.0, 0.0, -7.0);
			const FVector D = Hinge + FVector(Sx * 3.0, 0.0, -6.0);
			for (const double Side : { 1.0, -1.0 })
			{
				const FVector Off(0.0, 2.5 * Side, 0.0);
				M.AddQuad(A + Off, B + Off, C + Off, D + Off, FVector(0.0, Side, 0.0), FLinearColor(0.62f, 0.33f, 0.09f));
			}
		}
		UStaticMesh* Mesh = TNProcRuntimeMesh::MakeStaticMesh(GetTransientPackage(), M, Mat, false, 0.f, 1.f, 0.f);
		if (Mesh) { Mesh->AddToRoot(); }
		CachedGold = Mesh;
		return Mesh;
	}
}

ATN_ScorePickup::ATN_ScorePickup()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;

	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	SetRootComponent(PickupMesh);
	PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupMesh->SetIsReplicated(false);

	CollectSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollectSphere"));
	CollectSphere->SetupAttachment(PickupMesh);
	CollectSphere->SetSphereRadius(60.f);
	CollectSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollectSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollectSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	ShellMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShellMesh"));
	ShellMesh->SetupAttachment(PickupMesh);
	ShellMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShellMesh->SetCastShadow(false);
	ShellMesh->SetIsReplicated(false);
}

void ATN_ScorePickup::BeginPlay()
{
	Super::BeginPlay();

	// Aspecto: la concha que gira, salvo que arte haya puesto una malla propia en PickupMesh (la de ayuda del motor,
	// el signo de interrogación, no cuenta).
	const UStaticMesh* Current = PickupMesh ? PickupMesh->GetStaticMesh() : nullptr;
	const bool bPlaceholder = !Current || Current->GetPathName().StartsWith(TEXT("/Engine/"));
	UStaticMesh* Shell = bPlaceholder ? ShellCoinMesh() : nullptr;
	if (Shell && ShellMesh)
	{
		PickupMesh->SetVisibility(false, false);
		ShellMesh->SetStaticMesh(Shell);
		ShellMesh->SetRelativeScale3D(FVector(1.5f));
		SpinTime = FMath::FRand() * 10.f;
		// Destellos dorados alrededor.
		TNAmbientFX::FEmitterDesc Sparkle;
		Sparkle.Shape = TNAmbientFX::EShape::Flake;
		Sparkle.bSoft = true;
		Sparkle.Color = FLinearColor(1.f, 0.92f, 0.55f);
		Sparkle.Alpha = 0.9f;
		Sparkle.MaxParticles = 14;
		Sparkle.Rate = 7.f;
		Sparkle.SpawnRadius = 60.f;
		Sparkle.SpawnHeight = 90.f;
		Sparkle.Speed = 25.f;
		Sparkle.Spread = 1.f;
		Sparkle.Gravity = 0.f;
		Sparkle.Buoyancy = 20.f;
		Sparkle.LifeMin = 0.5f;
		Sparkle.LifeMax = 0.9f;
		Sparkle.SizeStart = 14.f;
		Sparkle.SizeEnd = 4.f;
		Sparkle.WakeDistance = 6000.f;
		TNAmbientFX::AddEmitter(this, Sparkle, GetActorLocation() + FVector(0.f, 0.f, 5.f));
	}
	else
	{
		SetActorTickEnabled(false);
		if (ShellMesh) { ShellMesh->SetVisibility(false); }
	}

	if (HasAuthority())
	{
		CollectSphere->OnComponentBeginOverlap.AddDynamic(this, &ATN_ScorePickup::OnSphereOverlap);
	}
}

void ATN_ScorePickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TNAmbientFX::RemoveOwner(this);
	Super::EndPlay(EndPlayReason);
}

void ATN_ScorePickup::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!ShellMesh || !ShellMesh->IsVisible()) { return; }
	// Giro de moneda y balanceo suave; los destellos, al paso.
	SpinTime += DeltaTime;
	ShellMesh->SetRelativeLocationAndRotation(FVector(0.f, 0.f, 55.f + 12.f * FMath::Sin(SpinTime * 3.f)),
		FRotator(0.f, FMath::Fmod(SpinTime * SpinTurnsPerSecond * 360.f, 360.f), 0.f));
	TNAmbientFX::TickOwner(this, DeltaTime);
}

void ATN_ScorePickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_ScorePickup, bActive);
}

// ── Recogida ───────────────────────────────────────────────────────────────────

void ATN_ScorePickup::OnSphereOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || !bActive) { return; }

	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->GetController()) { return; }

	// Solo jugadores vivos
	ATN_CoopPlayerState* PS = Pawn->GetPlayerState<ATN_CoopPlayerState>();
	if (!PS || !PS->IsAliveAndPlaying()) { return; }

	// Sumar puntos (server-auth). AddRaceScore difunde OnRaceScoreChanged también en
	// el host del listen-server, cuyo OnRep no dispara → su HUD se refresca en vivo.
	PS->AddRaceScore(ScoreValue);
	PS->ForceNetUpdate();
	UE_LOG(LogTortunabo, Log, TEXT("[ScorePickup] %s recogió %d puntos → total %d"),
		*GetNameSafe(Pawn), ScoreValue, PS->RaceScore);

	// Desactivar
	bActive = false;
	ApplyActiveState(false);
	MulticastPickedUp(Pawn);

	if (bRespawn)
	{
		FTimerDelegate RespawnDelegate;
		RespawnDelegate.BindUObject(this, &ATN_ScorePickup::Respawn);
		GetWorldTimerManager().SetTimer(RespawnTimerHandle, RespawnDelegate, RespawnSeconds, false);
	}
	else
	{
		Destroy();
	}
}

void ATN_ScorePickup::MulticastPickedUp_Implementation(APawn* Collector)
{
	OnPickedUp(Collector);
}

void ATN_ScorePickup::Respawn()
{
	bActive = true;
	ApplyActiveState(true);
}

void ATN_ScorePickup::ApplyActiveState(bool bNowActive)
{
	SetActorHiddenInGame(!bNowActive);
	SetActorEnableCollision(bNowActive);
}

void ATN_ScorePickup::OnRep_bActive()
{
	ApplyActiveState(bActive);
}
