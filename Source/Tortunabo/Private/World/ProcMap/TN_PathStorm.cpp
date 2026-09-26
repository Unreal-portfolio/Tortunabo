#include "World/ProcMap/TN_PathStorm.h"
#include "World/ProcMap/TN_ProcMapGenerator.h"
#include "World/ProcMap/TN_ProcMapActorUtils.h"
#include "Core/TN_CoopPlayerState.h"
#include "Core/TN_Log.h"
#include "Player/TortugaCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

ATN_PathStorm::ATN_PathStorm()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(4.f);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	FrontWall = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrontWall"));
	FrontWall->SetupAttachment(Root);
	FrontWall->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FrontWall->SetCastShadow(false);
	// Muro alto y ancho perpendicular al camino (el camino va por +X local).
	FrontWall->SetRelativeScale3D(FVector(1.f, 180.f, 90.f));
	FrontWall->SetRelativeLocation(FVector(0.f, 0.f, 3000.f));
	FrontWall->SetHiddenInGame(true);
	if (Cube.Succeeded()) { FrontWall->SetStaticMesh(Cube.Object); }

	InsidePostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("InsidePostProcess"));
	InsidePostProcess->SetupAttachment(Root);
	InsidePostProcess->bUnbound = true;
	InsidePostProcess->bEnabled = false;
}

void ATN_PathStorm::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_PathStorm, FrontProgress);
	DOREPLIFETIME(ATN_PathStorm, Speed);
	DOREPLIFETIME(ATN_PathStorm, bActive);
	DOREPLIFETIME(ATN_PathStorm, Generator);
}

void ATN_PathStorm::BeginPlay()
{
	Super::BeginPlay();
	TNProcActors::Tint(FrontWall, FLinearColor(0.12f, 0.1f, 0.18f));
}

void ATN_PathStorm::StartStorm(ATN_ProcMapGenerator* InGenerator, float InSpeed, float GraceSeconds)
{
	if (!HasAuthority()) { return; }
	Generator = InGenerator;
	Speed = InSpeed;
	GraceRemaining = GraceSeconds;
	// Arranca un poco por detrás de la salida para que el claro inicial quede libre.
	FrontProgress = -3000.f;
	bActive = InGenerator != nullptr && InSpeed > 0.f;
	InsideTime.Reset();
	UE_LOG(LogTortunabo, Log, TEXT("[PathStorm] Tormenta %s · %.0f cm/s · gracia %.0fs"),
		bActive ? TEXT("activa") : TEXT("inactiva"), Speed, GraceSeconds);
}

void ATN_PathStorm::StopStorm()
{
	if (!HasAuthority()) { return; }
	bActive = false;
	FrontProgress = -3000.f;
	for (auto& Pair : InsideTime)
	{
		if (APlayerController* PC = Pair.Key.Get())
		{
			if (ATN_CoopPlayerState* PS = PC->GetPlayerState<ATN_CoopPlayerState>()) { PS->DeathZoneTimeRemaining = -1.f; }
		}
	}
	InsideTime.Reset();
}

void ATN_PathStorm::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bActive)
	{
		if (HasAuthority())
		{
			if (GraceRemaining > 0.f) { GraceRemaining -= DeltaTime; }
			else { FrontProgress += Speed * DeltaTime; }

			CheckAccumulator += DeltaTime;
			if (CheckAccumulator >= 0.2f)
			{
				ServerCheckPlayers(CheckAccumulator);
				CheckAccumulator = 0.f;
			}
		}
		else if (Speed > 0.f)
		{
			// Extrapolación suave entre actualizaciones de red.
			FrontProgress += Speed * DeltaTime * 0.9f;
		}
	}
	UpdateVisual();
}

void ATN_PathStorm::UpdateVisual()
{
	if (!Generator || !Generator->IsMapReady() || !bActive)
	{
		FrontWall->SetHiddenInGame(true);
		InsidePostProcess->bEnabled = false;
		return;
	}

	FVector Dir = FVector::ForwardVector;
	const FVector FrontLoc = Generator->GetPathLocationAtProgress(FMath::Max(0.f, FrontProgress), Dir);
	SetActorLocationAndRotation(FrontLoc, FRotator(0.f, Dir.Rotation().Yaw, 0.f));
	FrontWall->SetHiddenInGame(FrontProgress <= 0.f);

	// Post-proceso solo para el jugador local que está dentro.
	bool bLocalInside = false;
	if (const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (const APawn* Pawn = PC->GetPawn())
		{
			bLocalInside = Generator->GetPathProgress(Pawn->GetActorLocation()) < FrontProgress - InsideMargin;
		}
	}
	InsidePostProcess->bEnabled = bLocalInside;
}

void ATN_PathStorm::ServerCheckPlayers(float Interval)
{
	if (!Generator || !Generator->IsMapReady()) { return; }

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) { continue; }
		ATN_CoopPlayerState* PS = PC->GetPlayerState<ATN_CoopPlayerState>();
		ATortugaCharacter* Turtle = Cast<ATortugaCharacter>(PC->GetPawn());
		if (!PS || !Turtle || !PS->bIsAlive || PS->bHasFinishedRun || Turtle->IsDead())
		{
			InsideTime.Remove(PC);
			continue;
		}

		const float Progress = Generator->GetPathProgress(Turtle->GetActorLocation());
		if (Progress < FrontProgress - InsideMargin)
		{
			float& T = InsideTime.FindOrAdd(PC);
			T += Interval;
			PS->DeathZoneTimeRemaining = FMath::Max(0.f, SecondsInsideToDie - T);
			if (T >= SecondsInsideToDie)
			{
				InsideTime.Remove(PC);
				PS->DeathZoneTimeRemaining = -1.f;
				Turtle->RequestKill(this);
			}
		}
		else if (InsideTime.Remove(PC) > 0)
		{
			PS->DeathZoneTimeRemaining = -1.f;
		}
	}
}

void ATN_PathStorm::ForceCheckPlayer(APlayerController* PC)
{
	if (PC) { InsideTime.Remove(PC); }
}
