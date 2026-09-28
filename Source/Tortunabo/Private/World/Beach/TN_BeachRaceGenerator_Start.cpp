// ─────────────────────────────────────────────────────────────────────────────
// ATN_BeachRaceGenerator — salida con huevos: una fila de cuatro huevos en su nido de
// arena (las bases y el nido van con la salida, en TN_BeachRaceGenerator_Scenery.cpp),
// con la tortuga de cada jugador dentro durante la preparación y la cuenta atrás. Al dar
// la salida (OpenStartEggs, del GameMode), las tapas saltan dando vueltas hacia los lados
// y cada tortuga sale lanzada hacia el mar, ya corriendo: como la salida de huevos del
// cooperativo (ATN_ProcStartStructure), pero en fila y con el salto hacia delante. En el
// sprint final de desempate el nido va a la línea del sprint (SetStartEggsAtSprint): sus
// bases y su anillo de arena se hacen allí (SprintNestMesh) y las tapas se mudan encima.
// Se replica solo si están rotos, desde cuándo y en qué línea (RoundNet); cada máquina
// anima sus tapas.
// ─────────────────────────────────────────────────────────────────────────────

#include "World/Beach/TN_BeachRaceGenerator.h"
#include "Components/StaticMeshComponent.h"
#include "Core/TN_Log.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "ProceduralMeshComponent.h"
#include "TN_BeachRaceKit.h"

namespace TNBeachEggs
{
	/** Segundos entre que se rompe un huevo y el siguiente (de fuera adentro). */
	constexpr double HatchStagger = 0.08;
	/** Tapas: salto hacia arriba y hacia el lado (cm/s), gravedad (cm/s²), giros (grados/s) y lo que tardan en esfumarse. */
	constexpr double LidSpeedZ = 620.0;
	constexpr double LidSpeedSide = 360.0;
	constexpr double LidSpeedBack = 90.0;
	constexpr double LidGravity = 1400.0;
	constexpr double LidSpinYaw = 540.0;
	constexpr double LidTumble = 320.0;
	constexpr double LidShrinkSeconds = 0.3;
	/** Altura del origen de la tapa (la costura) sobre el suelo al posarse: los dientes bajan 18 cm. */
	constexpr double LidRestHeight = 20.0;
	/** Giro de cada tapa cerrada: múltiplo de 22,5° para que sus dientes encajen entre los de la base. */
	constexpr double LidClosedYawStep = 45.0;
	/** Lo que se hunde la base del huevo en la arena. */
	constexpr double CupSink = 8.0;

	/** Origen de la tapa cerrada sobre la base Cup (local): el centro de su costura. */
	FVector LidClosed(const FVector& Cup)
	{
		return Cup + FVector(0.0, 0.0, TNCastleKit::EggSeam);
	}

	/** Hacia dónde salta la tapa de la base Cup: hacia fuera de la fila (los de -Y a -Y y los de +Y a +Y), un poco hacia atrás. */
	FVector LidDir(const FVector& Cup)
	{
		const double Side = Cup.Y >= 0.0 ? 1.0 : -1.0;
		return FVector(-LidSpeedBack, Side * LidSpeedSide, 0.0);
	}

	/** Segundos que vuela la tapa de la base Cup hasta posarse en la arena. */
	double LidFlightSeconds(const FVector& Cup)
	{
		const FVector Start = LidClosed(Cup);
		const FVector Land = Start + LidDir(Cup);
		const double Ground = TNBeachLayout::GroundZ(Land.X, Land.Y) + LidRestHeight;
		const double Drop = FMath::Max(0.0, Start.Z - Ground);
		return (LidSpeedZ + FMath::Sqrt(LidSpeedZ * LidSpeedZ + 2.0 * LidGravity * Drop)) / LidGravity;
	}

	/**
	 * Nido del huevo Index con el suelo en Spot (local): la base del huevo (la de los del lobby, medio enterrada) en Cups y
	 * el anillo de arena removida que se pisa en Ring. El mismo nido que el de la salida (TN_BeachRaceGenerator_Scenery.cpp).
	 */
	void AddNest(TNProcMesh::FTNProcMeshBuffers& Ring, TNProcMesh::FTNProcMeshBuffers& Cups, const FVector& Spot, int32 Index)
	{
		TNCastleKit::BuildEggCup(Cups, Spot - FVector(0.0, 0.0, CupSink), TNCastleKit::Col(0xFFF3DC), TNCastleKit::Col(TNCastleKit::EggAccent(Index)));
		const FLinearColor Mound = TNBeachRaceKit::Hex(0xE2C58Eu);
		auto RingAt = [&Spot](double A, double R, double Z) { return FVector(Spot.X + FMath::Cos(A) * R, Spot.Y + FMath::Sin(A) * R, Spot.Z + Z); };
		constexpr int32 Seg = 20;
		for (int32 k = 0; k < Seg; ++k)
		{
			const double A0 = TNProcMap::TwoPi * k / Seg;
			const double A1 = TNProcMap::TwoPi * (k + 1) / Seg;
			const double Bump0 = 6.0 * TNProcMesh::TNProcHashNoise(k, Index, 0xE66u);
			const double Bump1 = 6.0 * TNProcMesh::TNProcHashNoise((k + 1) % Seg, Index, 0xE66u);
			const FVector Inward(-FMath::Cos(0.5 * (A0 + A1)), -FMath::Sin(0.5 * (A0 + A1)), 0.0);
			// Lomo del anillo (de 1,3 m y 30 cm de alto a 2,8 m, enterrado) y su cara de dentro, hacia el huevo.
			Ring.AddQuad(RingAt(A0, 130.0, 30.0 + Bump0), RingAt(A1, 130.0, 30.0 + Bump1), RingAt(A1, 280.0, -18.0), RingAt(A0, 280.0, -18.0), FVector::UpVector, Mound);
			Ring.AddQuad(RingAt(A0, 130.0, -20.0), RingAt(A1, 130.0, -20.0), RingAt(A1, 130.0, 30.0 + Bump1), RingAt(A0, 130.0, 30.0 + Bump0), Inward, Mound * 0.9f);
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Línea de los huevos: la salida o la del sprint final
// ─────────────────────────────────────────────────────────────────────────────

FVector ATN_BeachRaceGenerator::StartEggCup(int32 Index) const
{
	if (!bEggsAtSprintLocal)
	{
		return TNBeachLayout::StartSpot(Index) - FVector(0.0, 0.0, TNBeachEggs::CupSink);
	}
	// En la línea del sprint, sobre el suelo con los asientos de la ronda (el mismo que GetSprintStartTransform).
	const FVector Spot = TNBeachLayout::SprintSpot(Index);
	const double Ground = TNBeachLayout::StampedZ(Layout.Stamps, Spot.X, Spot.Y, TNBeachLayout::SurfaceZ(Spot.X, Spot.Y));
	return FVector(Spot.X, Spot.Y, Ground - TNBeachEggs::CupSink);
}

bool ATN_BeachRaceGenerator::IsStartEggLineStale() const
{
	return RoundNet.bSprintEggs != bEggsAtSprintLocal || (bEggsAtSprintLocal && (SprintNestRound != AppliedRound || !IsValid(SprintNestMesh)));
}

void ATN_BeachRaceGenerator::ApplyStartEggLine()
{
	bEggsAtSprintLocal = RoundNet.bSprintEggs;
	SprintNestRound = AppliedRound;
	if (bEggsAtSprintLocal)
	{
		BuildSprintNest();
	}
	else if (IsValid(SprintNestMesh))
	{
		// De vuelta en la salida: sin nido (ni su colisión) en la línea del sprint.
		SprintNestMesh->ClearAllMeshSections();
	}
}

void ATN_BeachRaceGenerator::BuildSprintNest()
{
	if (!BeachRoot) { return; }
	if (!IsValid(SprintNestMesh))
	{
		UProceduralMeshComponent* Nest = NewObject<UProceduralMeshComponent>(this, NAME_None, RF_Transient | RF_DuplicateTransient);
		Nest->ComponentTags.Add(TNBeachRaceKit::GeneratedTag());
		// Colisión cocinada al momento: las finalistas aparecen dentro enseguida.
		Nest->bUseAsyncCooking = false;
		Nest->SetCanEverAffectNavigation(false);
		Nest->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Nest->SetupAttachment(BeachRoot);
		Nest->RegisterComponent();
		SprintNestMesh = Nest;
	}
	SprintNestMesh->ClearAllMeshSections();
	TNProcMesh::FTNProcMeshBuffers Ring;
	TNProcMesh::FTNProcMeshBuffers Cups;
	for (int32 i = 0; i < TNBeachLayout::NumStartSpots; ++i)
	{
		TNBeachEggs::AddNest(Ring, Cups, StartEggCup(i) + FVector(0.0, 0.0, TNBeachEggs::CupSink), i);
	}
	// El anillo se pisa (con el material del terreno); las bases, con el de los huevos del lobby y sin colisión.
	TNBeachRaceKit::Upload(SprintNestMesh, 0, Ring, TNBeachRaceKit::TerrainMaterial(), true);
	TNBeachRaceKit::Upload(SprintNestMesh, 1, Cups, TNCastleKit::VertexColorMaterial(), false);
}

void ATN_BeachRaceGenerator::SetStartEggsAtSprint(bool bAtSprint)
{
	if (!HasAuthority() || RoundNet.bSprintEggs == bAtSprint) { return; }
	RoundNet.bSprintEggs = bAtSprint;
	// En la línea nueva, cerrados: las tortugas aparecen dentro.
	RoundNet.bStartOpen = false;
	RoundNet.StartOpenTime = 0.f;
	ApplyStartEggs(false);
	ForceNetUpdate();
	UE_LOG(LogTortunabo, Log, TEXT("[Playa] ronda %d: los huevos de la salida van %s."), RoundNet.Round,
		bAtSprint ? *FString::Printf(TEXT("a la línea del sprint final (%.0f m)"), TNBeachLayout::SprintLineX() / 100.0) : TEXT("a la salida"));
}

// ─────────────────────────────────────────────────────────────────────────────
// Tapas
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachRaceGenerator::BuildStartEggs()
{
	if (!BeachRoot) { return; }
	// Tapas nuevas (y, si lo había, el nido del sprint lo ha quitado ClearGenerated): ApplyStartEggs las lleva a su línea.
	bEggsAtSprintLocal = false;
	SprintNestRound = -1;
	if (!IsValid(SprintNestMesh)) { SprintNestMesh = nullptr; }
	UMaterialInterface* Mat = TNCastleKit::VertexColorMaterial();
	for (int32 i = 0; i < TNBeachLayout::NumStartSpots; ++i)
	{
		// La misma tapa que los huevos del lobby y de la salida del cooperativo, con el color de cada huevo.
		TNProcMesh::FTNProcMeshBuffers LidBuffers;
		TNCastleKit::BuildEggLid(LidBuffers, TNCastleKit::Pal(0xFFF3DC), TNCastleKit::Pal(TNCastleKit::EggAccent(i)));
		UStaticMesh* LidMesh = TNProcRuntimeMesh::MakeStaticMesh(this, LidBuffers, Mat);
		if (!LidMesh) { continue; }
		StartEggLidMeshes.Add(LidMesh);
		UStaticMeshComponent* Lid = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient | RF_DuplicateTransient);
		Lid->ComponentTags.Add(TNBeachRaceKit::GeneratedTag());
		Lid->SetStaticMesh(LidMesh);
		Lid->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Lid->SetCanEverAffectNavigation(false);
		Lid->SetMobility(EComponentMobility::Movable);
		Lid->SetupAttachment(BeachRoot);
		Lid->RegisterComponent();
		Lid->SetRelativeLocationAndRotation(TNBeachEggs::LidClosed(StartEggCup(i)), FRotator(0.0, TNBeachEggs::LidClosedYawStep * i, 0.0));
		StartEggLids.Add(Lid);
	}
}

void ATN_BeachRaceGenerator::OpenStartEggs()
{
	if (!HasAuthority() || RoundNet.bStartOpen) { return; }
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	RoundNet.bStartOpen = true;
	const double ServerNow = GS ? GS->GetServerWorldTimeSeconds() : (World ? static_cast<double>(World->GetTimeSeconds()) : 0.0);
	RoundNet.StartOpenTime = static_cast<float>(ServerNow);
	ForceNetUpdate();
	ApplyStartEggs(true);
	UE_LOG(LogTortunabo, Log, TEXT("[Playa] ronda %d: se rompen los huevos %s."), RoundNet.Round, RoundNet.bSprintEggs ? TEXT("del sprint final") : TEXT("de la salida"));
}

void ATN_BeachRaceGenerator::ApplyStartEggs(bool bLive)
{
	// Primero, en su línea (la salida o la del sprint final).
	if (IsStartEggLineStale()) { ApplyStartEggLine(); }
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	bEggsOpenLocal = RoundNet.bStartOpen;
	EggsHatchedMask = 0;
	bEggsLaunch = bLive && bEggsOpenLocal;
	if (!bEggsOpenLocal)
	{
		// Cerrados: cada tapa sobre su base.
		bEggsAnimating = false;
		for (int32 i = 0; i < StartEggLids.Num(); ++i)
		{
			UStaticMeshComponent* Lid = StartEggLids[i];
			if (!Lid) { continue; }
			Lid->SetRelativeLocationAndRotation(TNBeachEggs::LidClosed(StartEggCup(i)), FRotator(0.0, TNBeachEggs::LidClosedYawStep * i, 0.0));
			Lid->SetRelativeScale3D(FVector::OneVector);
			Lid->SetVisibility(true);
		}
		return;
	}
	// Rotos: al vivirlo, desde ahora (con el salto de las tortugas); si no, ya del todo.
	EggsOpenedAt = bLive ? Now : Now - 1000.0;
	if (bEggsLaunch) { LaunchTurtlesFromEggs(); }
	bEggsAnimating = UpdateStartEggs();
}

bool ATN_BeachRaceGenerator::UpdateStartEggs()
{
	using namespace TNBeachEggs;
	const UWorld* World = GetWorld();
	const double Elapsed = (World ? World->GetTimeSeconds() : EggsOpenedAt) - EggsOpenedAt;
	bool bRunning = false;
	const int32 Num = StartEggLids.Num();
	for (int32 i = 0; i < Num; ++i)
	{
		UStaticMeshComponent* Lid = StartEggLids[i];
		if (!Lid) { continue; }
		// De fuera adentro: primero los de las puntas de la fila.
		const int32 Order = FMath::Min(i, Num - 1 - i);
		const double T = Elapsed - HatchStagger * Order;
		if (T < 0.0)
		{
			bRunning = true;
			continue;
		}
		EggsHatchedMask |= 1 << i;
		const FVector Cup = StartEggCup(i);
		const double Flight = LidFlightSeconds(Cup);
		const double Shrink = T > Flight ? 1.0 - (T - Flight) / LidShrinkSeconds : 1.0;
		if (Shrink <= 0.0)
		{
			Lid->SetVisibility(false);
			continue;
		}
		// Tiro parabólico hacia fuera dando vueltas; al posarse se queda donde cae y encoge hasta desaparecer.
		const double Tf = FMath::Min(T, Flight);
		const FVector Where = LidClosed(Cup) + LidDir(Cup) * Tf + FVector(0.0, 0.0, LidSpeedZ * Tf - 0.5 * LidGravity * Tf * Tf);
		Lid->SetVisibility(true);
		Lid->SetRelativeLocationAndRotation(Where, FRotator(LidTumble * Tf, LidClosedYawStep * i + LidSpinYaw * Tf, 0.0));
		Lid->SetRelativeScale3D(FVector(Shrink));
		bRunning = true;
	}
	return bRunning;
}

void ATN_BeachRaceGenerator::LaunchTurtlesFromEggs()
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld()) { return; }
	const FTransform& Xf = GetActorTransform();
	const FVector Launch = Xf.TransformVectorNoScale(FVector(TNBeachLayout::EggLaunchForward, 0.0, 0.0)) + FVector(0.0, 0.0, TNBeachLayout::EggLaunchUp);
	// La franja de la línea de los huevos (la salida o la del sprint), con las filas de detrás: la de la salida, desde el
	// muro de detrás hasta EggLaunchReachX; la del sprint, la misma franja movida a su línea.
	const double LineShift = bEggsAtSprintLocal ? TNBeachLayout::SprintLineX() - TNBeachLayout::StartSpotX : 0.0;
	const double MinX = TNBeachLayout::BackWallX + LineShift;
	const double MaxX = TNBeachLayout::EggLaunchReachX + LineShift;
	// Solo mueve a cada tortuga quien la controla, a la vez: el servidor (a todas) y cada cliente (a la suya, al recibir
	// los huevos rotos). En un cliente, el iterador solo tiene sus controladores locales.
	int32 Launched = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		ACharacter* Turtle = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
		if (!Turtle || !(HasAuthority() || Turtle->IsLocallyControlled())) { continue; }
		const FVector Local = Xf.InverseTransformPosition(Turtle->GetActorLocation());
		if (Local.X < MinX || Local.X > MaxX || FMath::Abs(Local.Y) > TNBeachLayout::HalfWidth) { continue; }
		// Recién soltada de la espera puede seguir sin modo de movimiento: el lanzamiento necesita uno.
		if (UCharacterMovementComponent* Move = Turtle->GetCharacterMovement())
		{
			if (Move->MovementMode == MOVE_None) { Move->SetMovementMode(MOVE_Falling); }
		}
		Turtle->LaunchCharacter(Launch, true, true);
		++Launched;
	}
	UE_LOG(LogTortunabo, Verbose, TEXT("[Playa] huevos: %d tortugas lanzadas hacia el mar."), Launched);
}
