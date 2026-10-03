// Nube de tormenta del modo carrera (el rayo de Mario Kart): una nube negra sobre cada otra tortuga en carrera y, tras un
// aviso, un rayo que las marea. Servidor: elige a las víctimas, marca el rayo UNA vez y termina. Todas las máquinas: dibujan las
// nubes, el aviso y el rayo con el reloj del servidor (ATN_RaceItemActor::GetAge).

#include "World/Beach/TN_RaceStormCloud.h"
#include "TN_BeachEnemyKit.h"
#include "../TN_LootGlowKit.h"
#include "World/Beach/TN_BeachEnemy.h"
#include "World/Beach/TN_BeachStun.h"
#include "World/Beach/TN_RaceItems.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/TN_Log.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Player/TortugaCharacter.h"

// Con nombre (no anónimo): en la compilación por bloques (unity) los nombres de un espacio anónimo se ven en el resto del
// bloque.
namespace TNRaceStormCloudDetail
{
	using FStormBuffers = TNProcMesh::FTNProcMeshBuffers;

	/** Nubes a la vez como mucho en todo el mundo. */
	constexpr int32 MaxClouds = 2;

	// ── Servidor ──────────────────────────────────────────────────────────────────────────────────────────────────────

	/** Empujón hacia arriba de la bola con que sale despedida la víctima (cm/s). */
	constexpr float LaunchUpCm = 300.f;
	/** Segundos entre el rayo y el final del actor (la nube sube y se desvanece), y margen de destrucción tras acabar. */
	constexpr float LingerSeconds = 1.6f;
	constexpr float FinishDelaySeconds = 0.5f;

	// ── La nube ───────────────────────────────────────────────────────────────────────────────────────────────────────

	/** Altura del centro de la nube sobre la tortuga (cm) y lo que tarda en alcanzarla cuando ella se mueve (s). */
	constexpr float CloudHeightCm = 2000.f;
	constexpr double CloudFollowSeconds = 0.25;
	/** Crece en GrowSeconds; tras el rayo espera LeaveDelaySeconds y en LeaveSeconds sube LeaveRiseCm encogiéndose. */
	constexpr float GrowSeconds = 0.9f;
	constexpr float LeaveDelaySeconds = 0.3f;
	constexpr float LeaveSeconds = 1.3f;
	constexpr float LeaveRiseCm = 1800.f;
	/** Si la víctima desaparece, su nube se disuelve en VanishSeconds subiendo VanishRiseCm. */
	constexpr float VanishSeconds = 0.5f;
	constexpr float VanishRiseCm = 700.f;
	/** Por debajo de esta escala la nube se esconde. */
	constexpr float MinVisibleScale = 0.02f;
	/** Cuánto tiembla la nube justo antes del rayo (cm), y cuánto respira su tamaño. */
	constexpr float ShiverCm = 24.f;
	constexpr float BreathAmount = 0.05f;
	/** Sombra en la arena bajo la nube: radio final (cm) y opacidad. */
	constexpr float ShadowRadiusCm = 760.f;
	constexpr float ShadowOpacity = 0.6f;
	/** Cuánto quedan los pies bajo el centro de una tortuga (cm). */
	constexpr float TurtleFeetBelowCm = 88.f;

	/** Chispas eléctricas y lluvia que suelta cada nube por segundo (con la nube entera), y su tope de un fotograma. */
	constexpr float SparkRate = 16.f;
	constexpr float RainRate = 48.f;
	constexpr float MaxSpawnPerFrame = 4.f;

	/** Una bola de la nube: sitio y diámetros (cm) alrededor del centro y cuál de las tres mallas de bola usa. */
	struct FBlobSpec
	{
		float X;
		float Y;
		float Z;
		float SizeX;
		float SizeY;
		float SizeZ;
		int32 Variant;
	};

	/** Siete bolas que hacen ~13 m de ancho por ~6 m de alto. */
	constexpr int32 BlobCount = 7;
	constexpr FBlobSpec CloudBlobs[BlobCount] =
	{
		{ 0.f, 0.f, 0.f, 760.f, 700.f, 460.f, 0 },
		{ -330.f, 120.f, -40.f, 560.f, 520.f, 380.f, 1 },
		{ 350.f, -90.f, -30.f, 600.f, 540.f, 400.f, 2 },
		{ -120.f, -300.f, 30.f, 520.f, 500.f, 360.f, 1 },
		{ 160.f, 300.f, 60.f, 540.f, 500.f, 380.f, 0 },
		{ 20.f, 20.f, 230.f, 560.f, 520.f, 360.f, 2 },
		{ -400.f, -140.f, 120.f, 380.f, 360.f, 300.f, 1 }
	};

	// ── Sonido ────────────────────────────────────────────────────────────────────────────────────────────────────────

	/** Radio pleno y alcance (cm) de la voz de cada nube. */
	constexpr float VoiceInnerCm = 2200.f;
	constexpr float VoiceFalloffCm = 16000.f;
	/** Trueno lejano de la nube: cuándo (s) y con qué volumen. */
	constexpr float RumbleAtSeconds = 0.15f;
	constexpr float RumbleVolume = 0.7f;
	/** Pitidos de aviso sobre la víctima: cuándo (s) suena cada uno y su tono. */
	constexpr int32 BeepCount = 3;
	constexpr float BeepTimes[BeepCount] = { 0.05f, 0.6f, 0.88f };
	constexpr float BeepPitches[BeepCount] = { 1.f, 1.12f, 1.3f };

	// ── Remolino de quien la lanza ────────────────────────────────────────────────────────────────────────────────────

	constexpr float SwirlSeconds = 0.4f;
	constexpr int32 SwirlBlobCount = 5;
	constexpr float SwirlHeightCm = 190.f;
	constexpr float SwirlRadiusCm = 60.f;
	constexpr float SwirlTurnsPerSecond = 2.5f;

	// ── Rayo ──────────────────────────────────────────────────────────────────────────────────────────────────────────

	/** Duración del rayo en pantalla (s) y cuándo suena el trueno tras él. */
	constexpr float BoltSeconds = 0.34f;
	constexpr float ThunderDelaySeconds = 0.22f;
	/** Longitud de las mallas del rayo (cm), variantes distintas, y por debajo de la nube y por encima del centro de la tortuga donde arranca y acaba. */
	constexpr float BoltMeshLengthCm = 2000.f;
	constexpr int32 BoltVariants = 4;
	constexpr int32 BoltSteps = 14;
	constexpr float BoltStartBelowCloudCm = 160.f;
	constexpr float BoltHitAboveCm = 20.f;
	/** Con el protector solar el rayo cae a este lado de la tortuga (cm). */
	constexpr float DeflectSideCm = 320.f;

	/** Destello: lúmenes, radio de luz, cuánto se acerca a la cámara (cm) y distancia a la que ya es débil. */
	constexpr float FlashLumens = 60000.f;
	constexpr float FlashRadiusCm = 4500.f;
	constexpr float FlashPullCm = 1300.f;
	constexpr float FlashFarCm = 9000.f;
	constexpr float FlashMinStrength = 0.12f;

	inline float StormSmooth(float X)
	{
		const float Clamped = FMath::Clamp(X, 0.f, 1.f);
		return Clamped * Clamped * (3.f - 2.f * Clamped);
	}

	/** M_ProcFXHard (translúcido sin luz y sin fundido por profundidad; lo crea Scripts/create_poop_decal.py) o null si aún no existe. */
	inline UMaterialInterface* StormHardMaterial()
	{
		static TWeakObjectPtr<UMaterialInterface> Cached;
		if (!Cached.IsValid())
		{
			Cached = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ProcMap/Materials/M_ProcFXHard.M_ProcFXHard"), nullptr, LOAD_NoWarn);
		}
		return Cached.Get();
	}

	/** Material dinámico de un efecto translúcido de Comp: el duro si existe, el suave si no. */
	inline UMaterialInstanceDynamic* StormSetupMid(UStaticMeshComponent* Comp)
	{
		if (!Comp)
		{
			return nullptr;
		}
		if (UMaterialInterface* Hard = StormHardMaterial())
		{
			Comp->CreateDynamicMaterialInstance(0, Hard);
		}
		return TNBeachKit::SoftMID(Comp);
	}

	/** Pieza de malla colocada en el mundo (no sigue al actor), sin sombra ni colisión. Null si no hay malla. */
	inline UStaticMeshComponent* StormAddWorldPart(AActor* Host, UStaticMesh* Mesh)
	{
		if (!Host || !Mesh)
		{
			return nullptr;
		}
		UStaticMeshComponent* Comp = TNBeachKit::AddPart(Host, Host->GetRootComponent(), Mesh, FVector::ZeroVector, false);
		if (Comp)
		{
			Comp->SetAbsolute(true, true, true);
			Comp->SetTranslucentSortPriority(3);
		}
		return Comp;
	}

	/** Gris azulado muy oscuro de cada una de las tres mallas de bola (sRGB aproximado: el constructor de mallas lo pasa a lineal). */
	inline FLinearColor CloudTone(int32 Variant)
	{
		switch (Variant)
		{
			case 1:
				return FLinearColor(0.2f, 0.2f, 0.25f);
			case 2:
				return FLinearColor(0.1f, 0.1f, 0.14f);
			default:
				return FLinearColor(0.15f, 0.15f, 0.19f);
		}
	}

	/** Bola irregular de caras planas, de 100 cm de diámetro y centrada en el origen (se escala a elipsoide al usarla). */
	inline UStaticMesh* CloudBlobMesh(int32 Variant)
	{
		const int32 Safe = FMath::Clamp(Variant, 0, 2);
		const FString Key = FString::Printf(TEXT("Race.Storm.Blob.%d"), Safe);
		return TNBeachKit::CachedMesh(Key, [Safe](FStormBuffers& M)
		{
			TArray<double> Heights;
			TArray<double> Radii;
			for (int32 Ring = 0; Ring <= 6; ++Ring)
			{
				const double Angle = -HALF_PI + PI * Ring / 6.0;
				Heights.Add(50.0 * (1.0 + FMath::Sin(Angle)));
				Radii.Add(FMath::Max(0.5, 50.0 * FMath::Cos(Angle)));
			}
			TNProcMesh::TNProcAddLathe(M, FVector(0.0, 0.0, -50.0), Heights, Radii, 0.16, 400u + static_cast<uint32>(Safe) * 31u, CloudTone(Safe), 10);
		});
	}

	/**
	 * Camino del rayo en su espacio local: arranca en el origen y baja por -Z hasta -BoltMeshLengthCm haciendo zigzag (cada punto
	 * a un lado, con más recorrido en el medio que en las puntas) y dos ramas cortas que salen del cuerpo. Sale de una semilla
	 * (la variante): siempre el mismo dibujo, en todas las máquinas.
	 */
	inline void BuildBoltPath(int32 Variant, TArray<FVector>& OutMain, TArray<TArray<FVector>>& OutForks)
	{
		OutMain.Reset();
		OutForks.Reset();
		const uint32 Salt = 1013u + static_cast<uint32>(Variant) * 7919u;
		// Todo el zigzag va sobre un eje horizontal propio de la variante, con un poco de desvío hacia el otro lado.
		const double Azimuth = UE_DOUBLE_TWO_PI * TNBeachKit::Hash01(Salt);
		const FVector ZigAxis(FMath::Cos(Azimuth), FMath::Sin(Azimuth), 0.0);
		const FVector CrossAxis(-ZigAxis.Y, ZigAxis.X, 0.0);
		for (int32 Step = 0; Step <= BoltSteps; ++Step)
		{
			const double Along = static_cast<double>(Step) / BoltSteps;
			const double Taper = (Step == 0 || Step == BoltSteps) ? 0.0 : FMath::Pow(FMath::Sin(UE_DOUBLE_PI * Along), 0.45);
			const double Side = (Step & 1) ? 1.0 : -1.0;
			const double Reach = 175.0 * Taper * (0.45 + 0.55 * TNBeachKit::Hash01(Salt + 11u + static_cast<uint32>(Step) * 3u));
			const double Drift = 70.0 * Taper * (TNBeachKit::Hash01(Salt + 97u + static_cast<uint32>(Step) * 5u) - 0.5);
			OutMain.Add(ZigAxis * (Side * Reach) + CrossAxis * Drift + FVector(0.0, 0.0, -static_cast<double>(BoltMeshLengthCm) * Along));
		}
		// Dos ramas: de la mitad de arriba y de más abajo, cuatro tramos cortos hacia un lado.
		const int32 ForkFrom[2] = { 4, 9 };
		for (int32 Fork = 0; Fork < 2; ++Fork)
		{
			TArray<FVector>& Branch = OutForks.AddDefaulted_GetRef();
			FVector Cursor = OutMain[ForkFrom[Fork]];
			Branch.Add(Cursor);
			const double Direction = ((Variant + Fork) & 1) ? 1.0 : -1.0;
			for (int32 Piece = 0; Piece < 4; ++Piece)
			{
				const double Out = 110.0 + 60.0 * TNBeachKit::Hash01(Salt + 211u + static_cast<uint32>(Fork * 8 + Piece));
				const double Wiggle = 90.0 * (TNBeachKit::Hash01(Salt + 313u + static_cast<uint32>(Fork * 8 + Piece)) - 0.5);
				Cursor += ZigAxis * (Direction * Out) + CrossAxis * Wiggle + FVector(0.0, 0.0, -190.0);
				Branch.Add(Cursor);
			}
		}
	}

	/** Tramo del rayo como viga de sección cuadrada con el alfa de sus vértices; se alarga un pelo por los extremos para tapar las juntas. */
	inline void AddBoltSegment(FStormBuffers& M, const FVector& From, const FVector& To, double Half, const FLinearColor& Color, float Alpha)
	{
		const FVector Direction = (To - From).GetSafeNormal();
		const int32 Before = M.Verts.Num();
		M.AddBeam(From - Direction * (Half * 0.8), To + Direction * (Half * 0.8), Half, Color);
		for (int32 Index = Before; Index < M.Verts.Num(); ++Index)
		{
			M.Colors[Index].A = Alpha;
		}
	}

	inline int32 BoltVariantOf(int32 Variant)
	{
		return ((Variant % BoltVariants) + BoltVariants) % BoltVariants;
	}

	/** Funda del rayo: viga ancha y tenue azulada con un núcleo blanco (translúcida, el alfa va en el vértice). */
	inline UStaticMesh* BoltSheathMesh(int32 Variant)
	{
		const int32 Safe = BoltVariantOf(Variant);
		return TNBeachKit::CachedMesh(FString::Printf(TEXT("Race.Storm.BoltSheath.%d"), Safe), [Safe](FStormBuffers& M)
		{
			TArray<FVector> MainPath;
			TArray<TArray<FVector>> Forks;
			BuildBoltPath(Safe, MainPath, Forks);
			const FLinearColor SheathColor(0.55f, 0.72f, 1.f);
			const FLinearColor CoreColor(1.f, 1.f, 1.f);
			for (int32 Point = 0; Point + 1 < MainPath.Num(); ++Point)
			{
				AddBoltSegment(M, MainPath[Point], MainPath[Point + 1], 46.0, SheathColor, 0.3f);
				AddBoltSegment(M, MainPath[Point], MainPath[Point + 1], 14.0, CoreColor, 1.f);
			}
			for (const TArray<FVector>& Branch : Forks)
			{
				for (int32 Point = 0; Point + 1 < Branch.Num(); ++Point)
				{
					AddBoltSegment(M, Branch[Point], Branch[Point + 1], 26.0, SheathColor, 0.24f);
					AddBoltSegment(M, Branch[Point], Branch[Point + 1], 7.0, CoreColor, 0.9f);
				}
			}
		}, TNBeachKit::EBeachMeshMat::SoftVertexAlpha);
	}

	/**
	 * Núcleo del rayo con el material emisivo de los brillos del mapa (M_ProcGlow, opaco, emisivo = color x 2: florece): el mismo
	 * camino, más fino. Null si el material no está.
	 */
	inline UStaticMesh* BoltGlowMesh(int32 Variant)
	{
		static TMap<int32, TWeakObjectPtr<UStaticMesh>> Cache;
		const int32 Safe = BoltVariantOf(Variant);
		if (const TWeakObjectPtr<UStaticMesh>* Found = Cache.Find(Safe))
		{
			if (UStaticMesh* Existing = Found->Get())
			{
				return Existing;
			}
		}
		UMaterialInterface* GlowMat = TNLootGlow::GlowMaterial();
		if (!GlowMat)
		{
			return nullptr;
		}
		TArray<FVector> MainPath;
		TArray<TArray<FVector>> Forks;
		BuildBoltPath(Safe, MainPath, Forks);
		FStormBuffers M;
		const FLinearColor CoreColor(0.92f, 0.96f, 1.f);
		for (int32 Point = 0; Point + 1 < MainPath.Num(); ++Point)
		{
			AddBoltSegment(M, MainPath[Point], MainPath[Point + 1], 9.0, CoreColor, 0.f);
		}
		for (const TArray<FVector>& Branch : Forks)
		{
			for (int32 Point = 0; Point + 1 < Branch.Num(); ++Point)
			{
				AddBoltSegment(M, Branch[Point], Branch[Point + 1], 5.0, CoreColor, 0.f);
			}
		}
		UStaticMesh* Mesh = TNProcRuntimeMesh::MakeStaticMesh(GetTransientPackage(), M, GlowMat, false, 0.f, 1.f, 0.f);
		Cache.Add(Safe, Mesh);
		return Mesh;
	}

	/** Cuánto brilla el rayo (0 a 1) a los T segundos de empezar: dos fogonazos y un parpadeo que se apaga. */
	inline float StormBoltIntensity(float T)
	{
		constexpr int32 Points = 9;
		constexpr float Times[Points] = { 0.f, 0.05f, 0.065f, 0.1f, 0.19f, 0.21f, 0.25f, 0.3f, 0.34f };
		constexpr float Levels[Points] = { 1.f, 1.f, 0.25f, 0.95f, 0.9f, 0.2f, 0.6f, 0.35f, 0.f };
		if (T <= 0.f)
		{
			return Levels[0];
		}
		for (int32 Index = 1; Index < Points; ++Index)
		{
			if (T <= Times[Index])
			{
				const float Blend = (T - Times[Index - 1]) / (Times[Index] - Times[Index - 1]);
				return FMath::Lerp(Levels[Index - 1], Levels[Index], Blend);
			}
		}
		return 0.f;
	}
}

ATN_RaceStormCloud::ATN_RaceStormCloud()
{
	// Nada más que ajustar: la base ya lo deja siempre relevante y sin movimiento replicado. Sin Track (UsesTrack es false): cada
	// máquina coloca las nubes sobre las tortugas que ve.
}

void ATN_RaceStormCloud::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_RaceStormCloud, Victims);
	DOREPLIFETIME(ATN_RaceStormCloud, bStruck);
}

// ─────────────────────────────────────────────────────────────────────────────
// Servidor
// ─────────────────────────────────────────────────────────────────────────────

bool ATN_RaceStormCloud::ServerCast(ATortugaCharacter* Caster)
{
	using namespace TNRaceStormCloudDetail;
	UWorld* World = IsValid(Caster) ? Caster->GetWorld() : nullptr;
	if (!World || !Caster->HasAuthority())
	{
		return false;
	}
	// Con dos nubes en el mundo ya hay tormenta de sobra.
	if (CountOf(World, ATN_RaceStormCloud::StaticClass()) >= MaxClouds)
	{
		return false;
	}

	// Las víctimas: las demás tortugas en carrera que se puedan golpear ahora.
	TArray<ATortugaCharacter*> Racers;
	TNRaceItems::GatherRacers(Caster, Racers);
	TArray<ATortugaCharacter*> Targets;
	for (ATortugaCharacter* Racer : Racers)
	{
		if (Racer && Racer != Caster && TNRaceItems::CanBeHurt(Racer) && ATN_BeachEnemy::CanBeHit(Racer) && !ATN_BeachEnemy::IsTurtleHeld(Racer))
		{
			Targets.Add(Racer);
		}
	}
	if (Targets.Num() == 0)
	{
		return false;
	}

	const FTransform SpawnAt(FRotator::ZeroRotator, Caster->GetActorLocation());
	ATN_RaceStormCloud* Storm = World->SpawnActorDeferred<ATN_RaceStormCloud>(ATN_RaceStormCloud::StaticClass(), SpawnAt, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Storm)
	{
		return false;
	}
	Storm->SetOwnerTurtle(Caster);
	for (ATortugaCharacter* Target : Targets)
	{
		Storm->Victims.Add(Target);
	}
	Storm->FinishSpawning(SpawnAt);
	UE_LOG(LogTortunabo, Log, TEXT("[Carrera] %s lanza la nube de tormenta sobre %d tortugas."), *GetNameSafe(Caster), Targets.Num());
	return true;
}

void ATN_RaceStormCloud::ServerTick(float DeltaSeconds)
{
	using namespace TNRaceStormCloudDetail;
	const double Age = GetAge();
	if (!bStruck)
	{
		if (Age >= static_cast<double>(TNRaceItems::StormTelegraphSeconds))
		{
			ServerStrike();
			return;
		}
		// Si todas las víctimas dejan la carrera (llegan a la meta, se van) antes del rayo, no queda nada que hacer.
		TArray<ATortugaCharacter*> Racers;
		TNRaceItems::GatherRacers(this, Racers);
		bool bAnyLeft = false;
		for (int32 Index = 0; Index < Victims.Num(); ++Index)
		{
			ATortugaCharacter* Candidate = Victims[Index].Get();
			if (Candidate && Racers.Contains(Candidate))
			{
				bAnyLeft = true;
				break;
			}
		}
		if (!bAnyLeft)
		{
			ServerFinish(FinishDelaySeconds);
		}
	}
	else if (Age >= static_cast<double>(TNRaceItems::StormTelegraphSeconds + LingerSeconds))
	{
		ServerFinish(FinishDelaySeconds);
	}
}

void ATN_RaceStormCloud::ServerStrike()
{
	using namespace TNRaceStormCloudDetail;
	bStruck = true;
	// Decisión (#72): una nube ya anunciada cae igualmente, como el picotazo de la gaviota (ATN_RaceGullStrike::ServerImpact),
	// pero con la carrera parada («¡TIEMPO!», recuento, título del sprint, podio) no marea a nadie.
	const bool bLive = ATN_BeachEnemy::IsRaceLive(this);
	TArray<ATortugaCharacter*> Racers;
	TNRaceItems::GatherRacers(this, Racers);
	int32 Stunned = 0;
	for (int32 Index = 0; Index < Victims.Num(); ++Index)
	{
		ATortugaCharacter* Victim = Victims[Index].Get();
		// Solo a las que siguen en carrera y siguen pudiendo ser golpeadas (el protector solar o el pelícano las salvan).
		if (!bLive || !Victim || !Racers.Contains(Victim) || !TNRaceItems::CanBeHurt(Victim) || !ATN_BeachEnemy::CanBeHit(Victim) || ATN_BeachEnemy::IsTurtleHeld(Victim))
		{
			continue;
		}
		TNBeach::StunTurtle(Victim, TNRaceItems::StormStunSeconds, FVector(0.0, 0.0, LaunchUpCm));
		++Stunned;
	}
	ForceNetUpdate();
	// El servidor no recibe OnRep: lo aplica aquí.
	OnRep_Struck();
	UE_LOG(LogTortunabo, Log, TEXT("[Carrera] La nube de tormenta de %s marea a %d de %d tortugas%s."), *GetNameSafe(GetOwnerTurtle()), Stunned, Victims.Num(),
		bLive ? TEXT("") : TEXT(" (con la carrera parada no marea a nadie)"));
}

void ATN_RaceStormCloud::OnRep_Struck()
{
	// El servidor ya ha marcado el rayo: cae aunque el reloj de esta máquina vaya un pelo por detrás del suyo.
	if (bStruck)
	{
		bStrikeAnnounced = true;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Visual
// ─────────────────────────────────────────────────────────────────────────────

void ATN_RaceStormCloud::BuildVisuals()
{
	using namespace TNRaceStormCloudDetail;
	const uint32 Seed = static_cast<uint32>(GetTypeHash(GetActorLocation())) | 1u;
	Clouds.SetNum(Victims.Num());

	// Chispas eléctricas azuladas (sueltas bajo cada nube y en el impacto).
	TNAmbientFX::FEmitterDesc SparkDesc = TNBeachKit::MakeDesc(TNAmbientFX::EShape::Ember, FLinearColor(0.72f, 0.86f, 1.f), true, 1.f, 90, 0.f,
		520.f, -500.f, 0.22f, 0.5f, 24.f, 3.f);
	SparkDesc.SpawnRadius = 20.f;
	SparkDesc.Spread = 1.6f;
	TNBeachKit::InitEmitter(SparkFX, this, SparkDesc, Seed);

	// Lluvia que cae de la nube.
	TNAmbientFX::FEmitterDesc RainDesc = TNBeachKit::MakeDesc(TNAmbientFX::EShape::Drop, FLinearColor(0.62f, 0.72f, 0.85f), true, 0.55f, 80, 0.f,
		1500.f, -600.f, 0.85f, 1.05f, 36.f, 36.f);
	RainDesc.SpawnRadius = 500.f;
	RainDesc.Spread = 0.12f;
	RainDesc.Drag = 0.1f;
	TNBeachKit::InitEmitter(RainFX, this, RainDesc, Seed + 3u);

	// Polvo gris que levanta el impacto.
	TNAmbientFX::FEmitterDesc DustDesc = TNBeachKit::MakeDesc(TNAmbientFX::EShape::Puff, FLinearColor(0.62f, 0.56f, 0.46f), true, 0.7f, 20, 0.f,
		380.f, -60.f, 0.6f, 1.f, 70.f, 230.f);
	DustDesc.SpawnRadius = 60.f;
	DustDesc.Spread = 1.8f;
	TNBeachKit::InitEmitter(DustFX, this, DustDesc, Seed + 5u);
	bEmittersReady = true;

	// Remolino oscuro sobre quien la lanza: unas bolas pequeñas que giran, escondidas hasta que toque.
	USceneComponent* Whirl = NewObject<USceneComponent>(this, NAME_None, RF_Transient);
	Whirl->SetMobility(EComponentMobility::Movable);
	Whirl->SetupAttachment(GetRootComponent());
	Whirl->RegisterComponent();
	Whirl->SetAbsolute(true, true, true);
	SwirlRoot = Whirl;
	for (int32 Piece = 0; Piece < SwirlBlobCount; ++Piece)
	{
		SwirlBlobs.Add(TNBeachKit::AddPart(this, Whirl, CloudBlobMesh(1 + Piece % 2), FVector::ZeroVector, false));
	}
	Whirl->SetVisibility(false, true);
	bSwirlShown = false;

	// Destello blanco azulado que se coloca cerca de la cámara cuando cae un rayo.
	UPointLightComponent* Flash = NewObject<UPointLightComponent>(this, NAME_None, RF_Transient);
	Flash->SetMobility(EComponentMobility::Movable);
	Flash->SetupAttachment(GetRootComponent());
	Flash->SetCastShadows(false);
	Flash->SetIntensityUnits(ELightUnits::Lumens);
	Flash->SetIntensity(0.f);
	Flash->SetAttenuationRadius(FlashRadiusCm);
	Flash->SetLightColor(FLinearColor(0.82f, 0.9f, 1.f));
	Flash->RegisterComponent();
	Flash->SetVisibility(false);
	FlashLight = Flash;
	bFlashLit = false;
}

void ATN_RaceStormCloud::BuildCloud(int32 Index, const FVector& VictimAt)
{
	using namespace TNRaceStormCloudDetail;
	if (!Clouds.IsValidIndex(Index))
	{
		return;
	}
	FCloudView& Cloud = Clouds[Index];
	Cloud.bBuilt = true;
	Cloud.LastVictimAt = VictimAt;

	// La nube: un punto de anclaje en el mundo con las bolas colgando de él.
	USceneComponent* CloudRootComp = NewObject<USceneComponent>(this, NAME_None, RF_Transient);
	CloudRootComp->SetMobility(EComponentMobility::Movable);
	CloudRootComp->SetupAttachment(GetRootComponent());
	CloudRootComp->RegisterComponent();
	CloudRootComp->SetAbsolute(true, true, true);
	Cloud.CloudRoot = CloudRootComp;
	for (int32 BlobIndex = 0; BlobIndex < BlobCount; ++BlobIndex)
	{
		Cloud.Blobs.Add(TNBeachKit::AddPart(this, CloudRootComp, CloudBlobMesh(CloudBlobs[BlobIndex].Variant), FVector::ZeroVector, false));
	}
	CloudRootComp->SetVisibility(false, true);
	Cloud.bShown = false;

	// Su sombra en la arena (blanda, se oscurece al crecer la nube).
	UStaticMeshComponent* ShadowComp = TNBeachKit::AddPart(this, GetRootComponent(), TNBeachKit::ShadowDiscEdge(0.4f), FVector::ZeroVector, false);
	if (ShadowComp)
	{
		ShadowComp->SetAbsolute(true, true, true);
		ShadowComp->SetTranslucentSortPriority(2);
		TNBeachKit::PlaceShadow(ShadowComp, FVector::ZeroVector, 0.f);
	}
	Cloud.Shadow = ShadowComp;

	// Su voz (trueno y pitidos en el sitio de la víctima); null sin audio.
	Cloud.Voice = UTN_RaceItemSynthComponent::AttachTo(this, VictimAt + FVector(0.0, 0.0, 250.0), VoiceInnerCm, VoiceFalloffCm);
}

void ATN_RaceStormCloud::HideCloud(FCloudView& Cloud)
{
	if (USceneComponent* CloudRootComp = Cloud.CloudRoot.Get())
	{
		CloudRootComp->SetVisibility(false, true);
	}
	TNBeachKit::PlaceShadow(Cloud.Shadow.Get(), FVector::ZeroVector, 0.f);
	Cloud.bShown = false;
}

void ATN_RaceStormCloud::VisualTick(float DeltaSeconds)
{
	using namespace TNRaceStormCloudDetail;
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const double Age = GetAge();
	FVector View = GetActorLocation();
	const bool bHasView = TNBeachKit::LocalCamera(World, View);

	TArray<ATortugaCharacter*> Racers;
	TNRaceItems::GatherRacers(this, Racers);
	if (Clouds.Num() != Victims.Num())
	{
		Clouds.SetNum(Victims.Num());
	}

	FlashLevel = 0.f;
	for (int32 Index = 0; Index < Clouds.Num(); ++Index)
	{
		TickCloud(Index, DeltaSeconds, Age, Racers);
	}
	TickSwirl(Age);
	TickFlash(View, bHasView);

	if (bEmittersReady)
	{
		TNBeachKit::TickEmitterIfBusy(SparkFX, DeltaSeconds, View);
		TNBeachKit::TickEmitterIfBusy(RainFX, DeltaSeconds, View);
		TNBeachKit::TickEmitterIfBusy(DustFX, DeltaSeconds, View);
	}
}

void ATN_RaceStormCloud::TickCloud(int32 Index, float DeltaSeconds, double Age, const TArray<ATortugaCharacter*>& Racers)
{
	using namespace TNRaceStormCloudDetail;
	UWorld* World = GetWorld();
	if (!World || !Clouds.IsValidIndex(Index))
	{
		return;
	}
	FCloudView& Cloud = Clouds[Index];
	ATortugaCharacter* Victim = Victims.IsValidIndex(Index) ? Victims[Index].Get() : nullptr;
	const bool bRacing = Victim != nullptr && Racers.Contains(Victim);
	const float TelegraphEnd = TNRaceItems::StormTelegraphSeconds;

	if (!Cloud.bBuilt)
	{
		// Aún no ha llegado la víctima por la red (o ya no corre), o llegamos tarde a ver esta nube: nada que dibujar.
		if (!bRacing || Age > static_cast<double>(TelegraphEnd + LingerSeconds))
		{
			return;
		}
		BuildCloud(Index, Victim->GetActorLocation());
	}

	// Si la víctima desaparece o llega a la meta, la nube se disuelve.
	if (!bRacing && !Cloud.bVanishing)
	{
		Cloud.bVanishing = true;
		Cloud.VanishStartAge = Age;
	}
	if (bRacing)
	{
		Cloud.LastVictimAt = Victim->GetActorLocation();
	}
	const FVector VictimAt = Cloud.LastVictimAt;

	// Tamaño: crece al formarse, se encoge al irse tras el rayo y al disolverse.
	const float AgeF = static_cast<float>(Age);
	const float Grow = 0.05f + 0.95f * StormSmooth(AgeF / GrowSeconds);
	const float Leave = StormSmooth((AgeF - TelegraphEnd - LeaveDelaySeconds) / LeaveSeconds);
	const float Gone = Cloud.bVanishing ? StormSmooth(static_cast<float>(Age - Cloud.VanishStartAge) / VanishSeconds) : 0.f;
	const float CloudScale = Grow * (1.f - Leave) * (1.f - Gone);

	// Sitio: sobre la víctima, con suavizado (aparece justo encima y luego la sigue).
	const FVector Target = VictimAt + FVector(0.0, 0.0, CloudHeightCm);
	if (!Cloud.bPlaced)
	{
		Cloud.CloudAt = Target;
		Cloud.bPlaced = true;
	}
	else
	{
		const double Follow = 1.0 - FMath::Exp(-static_cast<double>(DeltaSeconds) / CloudFollowSeconds);
		Cloud.CloudAt += (Target - Cloud.CloudAt) * Follow;
	}
	const FVector RootAt = Cloud.CloudAt + FVector(0.0, 0.0, static_cast<double>(Leave * LeaveRiseCm + Gone * VanishRiseCm));

	// Voz de la nube: sigue a la víctima; trueno lejano y pitidos de aviso.
	if (UTN_RaceItemSynthComponent* Voice = Cloud.Voice.Get())
	{
		Voice->SetWorldLocation(VictimAt + FVector(0.0, 0.0, 250.0));
		if (!Cloud.bVanishing)
		{
			if (!Cloud.bRumbled && AgeF >= RumbleAtSeconds && AgeF < TelegraphEnd)
			{
				Voice->Play(ETNRaceSound::Rumble, 1.f, RumbleVolume);
				Cloud.bRumbled = true;
			}
			while (Cloud.BeepsPlayed < BeepCount && AgeF >= BeepTimes[Cloud.BeepsPlayed])
			{
				if (AgeF < TelegraphEnd)
				{
					Voice->Play(ETNRaceSound::Beep, BeepPitches[Cloud.BeepsPlayed], 0.9f);
				}
				++Cloud.BeepsPlayed;
			}
		}
	}

	// El rayo, a la hora del servidor (o en cuanto avisa de que ya lo ha marcado).
	const bool bStrikeTime = bStrikeAnnounced || AgeF >= TelegraphEnd;
	if (bStrikeTime && !Cloud.bBoltStarted && !Cloud.bVanishing)
	{
		if (AgeF > TelegraphEnd + BoltSeconds + 0.4f)
		{
			// Llegamos tarde: el rayo ya pasó.
			Cloud.bBoltStarted = true;
			Cloud.bBoltDone = true;
		}
		else
		{
			StartBolt(Index, Cloud, VictimAt);
		}
	}
	if (Cloud.bBoltStarted && !Cloud.bBoltDone)
	{
		TickBolt(Index, Cloud, World->GetTimeSeconds());
	}

	// Demasiado pequeña: escondida.
	if (CloudScale < MinVisibleScale)
	{
		if (Cloud.bShown)
		{
			HideCloud(Cloud);
		}
		return;
	}
	if (!Cloud.bShown)
	{
		if (USceneComponent* ShownRoot = Cloud.CloudRoot.Get())
		{
			ShownRoot->SetVisibility(true, true);
		}
		Cloud.bShown = true;
	}
	if (USceneComponent* CloudRootComp = Cloud.CloudRoot.Get())
	{
		CloudRootComp->SetWorldLocation(RootAt);
		CloudRootComp->SetWorldScale3D(FVector(static_cast<double>(CloudScale)));
	}

	// Las bolas giran despacio, respiran y tiemblan más según se acerca el rayo.
	const float Shiver = ShiverCm * StormSmooth(AgeF / TelegraphEnd) * StormSmooth(AgeF / TelegraphEnd) * (1.f - Leave);
	for (int32 BlobIndex = 0; BlobIndex < Cloud.Blobs.Num() && BlobIndex < BlobCount; ++BlobIndex)
	{
		UStaticMeshComponent* Blob = Cloud.Blobs[BlobIndex].Get();
		if (!Blob)
		{
			continue;
		}
		const FBlobSpec& Spec = CloudBlobs[BlobIndex];
		const float Phase = AgeF * 1.9f + static_cast<float>(BlobIndex) * 1.37f;
		const FVector Wobble(Shiver * FMath::Sin(Phase * 7.f), Shiver * FMath::Cos(Phase * 6.3f), 26.f * FMath::Sin(Phase));
		const float Breath = 1.f + BreathAmount * FMath::Sin(Phase * 1.3f + 1.f);
		const FVector Diameter(Spec.SizeX * Breath, Spec.SizeY * Breath, Spec.SizeZ * Breath);
		const float Turn = AgeF * 6.f * ((BlobIndex % 2) == 0 ? 1.f : -1.f);
		Blob->SetRelativeTransform(FTransform(FRotator(0.f, Turn, 0.f), FVector(Spec.X, Spec.Y, Spec.Z) + Wobble, Diameter / 100.0));
	}

	// Sombra en la arena bajo la nube.
	const float GroundZ = GroundHeightAt(Cloud.CloudAt, static_cast<float>(VictimAt.Z) - TurtleFeetBelowCm);
	TNBeachKit::PlaceShadow(Cloud.Shadow.Get(), FVector(Cloud.CloudAt.X, Cloud.CloudAt.Y, GroundZ), ShadowRadiusCm * CloudScale);
	TNBeachKit::SetOpacity(TNBeachKit::SoftMID(Cloud.Shadow.Get()), ShadowOpacity * CloudScale);

	// Chispas y lluvia mientras la nube está entera y aún no se va.
	if (bEmittersReady && CloudScale > 0.35f && Leave < 0.6f)
	{
		Cloud.SparkAccum = FMath::Min(Cloud.SparkAccum + SparkRate * CloudScale * DeltaSeconds, MaxSpawnPerFrame);
		while (Cloud.SparkAccum >= 1.f)
		{
			Cloud.SparkAccum -= 1.f;
			const FVector SparkAt = RootAt + FVector(FMath::FRandRange(-1.f, 1.f) * 420.f * CloudScale, FMath::FRandRange(-1.f, 1.f) * 420.f * CloudScale, -220.f * CloudScale);
			TNBeachKit::BurstAt(SparkFX, SparkAt, FVector::DownVector, 1);
		}
		Cloud.RainAccum = FMath::Min(Cloud.RainAccum + RainRate * CloudScale * DeltaSeconds, MaxSpawnPerFrame);
		while (Cloud.RainAccum >= 1.f)
		{
			Cloud.RainAccum -= 1.f;
			TNBeachKit::BurstAt(RainFX, RootAt + FVector(0.0, 0.0, -200.0 * CloudScale), FVector::DownVector, 1);
		}
	}
}

void ATN_RaceStormCloud::StartBolt(int32 Index, FCloudView& Cloud, const FVector& VictimAt)
{
	using namespace TNRaceStormCloudDetail;
	Cloud.bBoltStarted = true;
	UWorld* World = GetWorld();
	if (!World)
	{
		Cloud.bBoltDone = true;
		return;
	}
	Cloud.BoltStartTime = World->GetTimeSeconds();

	// Con el protector solar (o el pelícano) el rayo cae a su lado y no le hace nada.
	const ATortugaCharacter* Victim = Victims.IsValidIndex(Index) ? Victims[Index].Get() : nullptr;
	Cloud.bDeflected = Victim != nullptr && TNRaceItems::IsInvulnerable(Victim);

	Cloud.BoltTop = Cloud.CloudAt + FVector(0.0, 0.0, -BoltStartBelowCloudCm);
	FVector Bottom = VictimAt + FVector(0.0, 0.0, BoltHitAboveCm);
	if (Cloud.bDeflected)
	{
		const double Around = UE_DOUBLE_TWO_PI * TNBeachKit::Hash01(static_cast<uint32>(Index) * 40503u + 17u);
		const FVector Beside = VictimAt + FVector(FMath::Cos(Around), FMath::Sin(Around), 0.0) * static_cast<double>(DeflectSideCm);
		Bottom = FVector(Beside.X, Beside.Y, GroundHeightAt(Beside, static_cast<float>(VictimAt.Z) - TurtleFeetBelowCm));
	}
	Cloud.BoltBottom = Bottom;

	// El dibujo del rayo mide BoltMeshLengthCm hacia -Z desde su origen: se gira hacia el blanco y se estira a lo que haga falta.
	const FVector Along = Cloud.BoltBottom - Cloud.BoltTop;
	const double Length = FMath::Max(Along.Size(), 300.0);
	const FVector Direction = Along.IsNearlyZero() ? FVector(0.0, 0.0, -1.0) : Along.GetSafeNormal();
	Cloud.BoltRotation = FQuat::FindBetweenNormals(FVector(0.0, 0.0, -1.0), Direction);
	Cloud.BoltLengthScale = Length / static_cast<double>(BoltMeshLengthCm);
	const FTransform BoltAt(Cloud.BoltRotation, Cloud.BoltTop, FVector(1.0, 1.0, Cloud.BoltLengthScale));

	UStaticMeshComponent* BoltSheathPart = StormAddWorldPart(this, BoltSheathMesh(Index));
	if (BoltSheathPart)
	{
		StormSetupMid(BoltSheathPart);
		BoltSheathPart->SetWorldTransform(BoltAt);
	}
	Cloud.BoltSheath = BoltSheathPart;
	UStaticMeshComponent* BoltCorePart = StormAddWorldPart(this, BoltGlowMesh(Index));
	if (BoltCorePart)
	{
		BoltCorePart->SetWorldTransform(BoltAt);
	}
	Cloud.BoltCore = BoltCorePart;

	// Chispas y polvo en el impacto, y el chasquido.
	if (bEmittersReady)
	{
		TNBeachKit::BurstAt(SparkFX, Cloud.BoltBottom, FVector::UpVector, 18);
		TNBeachKit::BurstAt(DustFX, Cloud.BoltBottom, FVector::UpVector, 7);
	}
	if (UTN_RaceItemSynthComponent* Voice = Cloud.Voice.Get())
	{
		Voice->SetWorldLocation(Cloud.BoltBottom + FVector(0.0, 0.0, 200.0));
		Voice->Play(ETNRaceSound::Zap, 1.f, 1.f);
	}
}

void ATN_RaceStormCloud::TickBolt(int32 Index, FCloudView& Cloud, double LocalNow)
{
	using namespace TNRaceStormCloudDetail;
	UStaticMeshComponent* BoltSheathPart = Cloud.BoltSheath.Get();
	UStaticMeshComponent* BoltCorePart = Cloud.BoltCore.Get();
	const float Elapsed = static_cast<float>(LocalNow - Cloud.BoltStartTime);
	if (Elapsed >= BoltSeconds)
	{
		if (BoltSheathPart)
		{
			BoltSheathPart->SetVisibility(false);
		}
		if (BoltCorePart)
		{
			BoltCorePart->SetVisibility(false);
		}
		Cloud.bBoltDone = true;
		return;
	}
	const float Level = StormBoltIntensity(Elapsed);
	if (BoltSheathPart)
	{
		const bool bShowSheath = Level > 0.02f;
		if (BoltSheathPart->IsVisible() != bShowSheath)
		{
			BoltSheathPart->SetVisibility(bShowSheath);
		}
		TNBeachKit::SetOpacity(TNBeachKit::SoftMID(BoltSheathPart), Level);
	}
	if (BoltCorePart)
	{
		// El núcleo es opaco: parpadea y se afina cuando el rayo flaquea.
		const bool bShowCore = Level > 0.5f;
		if (BoltCorePart->IsVisible() != bShowCore)
		{
			BoltCorePart->SetVisibility(bShowCore);
		}
		const double Width = 0.55 + 0.6 * static_cast<double>(Level);
		BoltCorePart->SetWorldTransform(FTransform(Cloud.BoltRotation, Cloud.BoltTop, FVector(Width, Width, Cloud.BoltLengthScale)));
	}

	// El trueno llega un momento después del chasquido.
	if (!Cloud.bThunderDone && Elapsed >= ThunderDelaySeconds)
	{
		Cloud.bThunderDone = true;
		if (UTN_RaceItemSynthComponent* Voice = Cloud.Voice.Get())
		{
			Voice->Play(ETNRaceSound::Rumble, 0.85f, 1.f);
		}
	}

	// El destello de este rayo (más débil cuanto más lejos de la cámara local, y menos si cae al lado).
	FVector CameraAt = Cloud.BoltBottom;
	const bool bCamera = TNBeachKit::LocalCamera(GetWorld(), CameraAt);
	const float Distance = bCamera ? static_cast<float>(FVector::Dist(CameraAt, Cloud.BoltBottom)) : 0.f;
	const float Strength = FMath::Clamp(1.15f - Distance / FlashFarCm, FlashMinStrength, 1.f) * (Cloud.bDeflected ? 0.7f : 1.f);
	const float Lit = Level * Strength;
	if (Lit > FlashLevel)
	{
		FlashLevel = Lit;
		FlashPoint = Cloud.BoltBottom;
	}
}

void ATN_RaceStormCloud::TickSwirl(double Age)
{
	using namespace TNRaceStormCloudDetail;
	USceneComponent* Whirl = SwirlRoot.Get();
	if (!Whirl)
	{
		return;
	}
	ATortugaCharacter* Caster = GetOwnerTurtle();
	const bool bWithinSwirl = Age < static_cast<double>(SwirlSeconds);
	if (!bSwirlSounded && bWithinSwirl)
	{
		// Un trueno sobre la cabeza de quien la lanza (el actor nace en su sitio).
		bSwirlSounded = true;
		PlaySfx(ETNRaceSound::Rumble, 1.1f, 0.8f);
	}
	if (!bWithinSwirl || !Caster)
	{
		if (bSwirlShown)
		{
			Whirl->SetVisibility(false, true);
			bSwirlShown = false;
		}
		return;
	}
	if (!bSwirlShown)
	{
		Whirl->SetVisibility(true, true);
		bSwirlShown = true;
	}
	const float Phase = static_cast<float>(Age / static_cast<double>(SwirlSeconds));
	const float Bulge = FMath::Sin(Phase * PI);
	Whirl->SetWorldLocation(Caster->GetActorLocation() + FVector(0.0, 0.0, SwirlHeightCm));
	const int32 Count = SwirlBlobs.Num();
	for (int32 Piece = 0; Piece < Count; ++Piece)
	{
		UStaticMeshComponent* Blob = SwirlBlobs[Piece].Get();
		if (!Blob)
		{
			continue;
		}
		const float Angle = static_cast<float>(Age) * SwirlTurnsPerSecond * 2.f * PI + static_cast<float>(Piece) * 2.f * PI / static_cast<float>(Count);
		const float Radius = SwirlRadiusCm * (0.6f + 0.4f * Bulge) * (1.f + 0.25f * static_cast<float>(Piece % 2));
		const float Diameter = (38.f + 12.f * static_cast<float>(Piece % 3)) * Bulge;
		Blob->SetRelativeTransform(FTransform(FRotator(0.f, FMath::RadiansToDegrees(Angle), 0.f),
			FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 14.f * static_cast<float>(Piece)), FVector(static_cast<double>(Diameter) / 100.0)));
	}
}

void ATN_RaceStormCloud::TickFlash(const FVector& View, bool bHasView)
{
	using namespace TNRaceStormCloudDetail;
	UPointLightComponent* Flash = FlashLight.Get();
	if (!Flash)
	{
		return;
	}
	if (FlashLevel > 0.01f)
	{
		// Cerca de la cámara, en dirección al rayo (sin cámara, sobre el punto de impacto).
		const FVector Toward = FlashPoint - View;
		const FVector At = bHasView ? View + Toward.GetClampedToMaxSize(static_cast<double>(FlashPullCm)) + FVector(0.0, 0.0, 250.0)
			: FlashPoint + FVector(0.0, 0.0, 600.0);
		Flash->SetWorldLocation(At);
		Flash->SetIntensity(FlashLumens * FlashLevel);
		if (!bFlashLit)
		{
			Flash->SetVisibility(true);
			bFlashLit = true;
		}
	}
	else if (bFlashLit)
	{
		Flash->SetIntensity(0.f);
		Flash->SetVisibility(false);
		bFlashLit = false;
	}
}

void ATN_RaceStormCloud::OnFinished()
{
	// Se acabó: todo lo que se ve (nubes, rayos, partículas, luz) desaparece con el actor un instante después.
	if (!bHasScreen)
	{
		return;
	}
	SetActorHiddenInGame(true);
	if (UPointLightComponent* Flash = FlashLight.Get())
	{
		Flash->SetVisibility(false);
	}
	bFlashLit = false;
}
