#include "World/TN_ItemSpawnZone.h"
#include "Components/BoxComponent.h"
#include "Core/TN_InventoryTypes.h"
#include "World/TN_PickupInteractableBase.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "TimerManager.h"

ATN_ItemSpawnZone::ATN_ItemSpawnZone()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false; // Solo el servidor spawnea; los pickups replican por sí mismos.

	SpawnBox = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnBox"));
	SetRootComponent(SpawnBox);
	SpawnBox->SetBoxExtent(FVector(500.f, 500.f, 200.f));
	SpawnBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpawnBox->SetHiddenInGame(true);

#if WITH_EDITORONLY_DATA
	SpawnBox->SetLineThickness(2.f);
	SpawnBox->ShapeColor = FColor::Cyan;
#endif
}

void ATN_ItemSpawnZone::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	if (!ItemDataTable || ItemRowNames.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ItemSpawnZone] '%s' — Sin DataTable o ItemRowNames. No se spawnean ítems."), *GetName());
		return;
	}

	// Diferir el spawn de ítems un tick.
	// Cuando este actor es un ChildActor dentro de un chunk, necesitamos que
	// SpawnBox->GetComponentTransform() refleje la posición final del chunk.
	//
	// IMPORTANTE: Usar FTimerDelegate bound a UObject (no lambda) para que EndPlay→
	// ClearAllTimersForObject(this) cancele el timer si el actor es destruido
	// antes del tick (ej. chunk temporal del ChunkManager::GetOrComputeInSocketTransform
	// que spawnea un chunk en Identity solo para leer el InSocket y lo destruye enseguida).
	// Con la lambda anterior, el timer NO se cancelaba → SpawnItems() disparaba en el
	// tick siguiente sobre un actor pendiente de GC en posición Identity → ítems en (0,0,0).
	GetWorldTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &ATN_ItemSpawnZone::SpawnItems));
}

void ATN_ItemSpawnZone::SpawnItems()
{
	TArray<FVector> SpawnedLocations;
	int32 SuccessCount = 0;

	for (int32 i = 0; i < SpawnCount; ++i)
	{
		FVector SpawnLocation;
		bool bFound = false;

		for (int32 Attempt = 0; Attempt < MaxRetries; ++Attempt)
		{
			if (FindValidSpawnPoint(SpawnLocation, SpawnedLocations))
			{
				bFound = true;
				break;
			}
		}

		if (!bFound)
		{
			UE_LOG(LogTemp, Warning, TEXT("[ItemSpawnZone] '%s' — No se encontró posición válida para ítem %d/%d tras %d intentos."),
				*GetName(), i + 1, SpawnCount, MaxRetries);
			continue;
		}

		// Elegir fila aleatoria
		const FName& RowName = ItemRowNames[FMath::RandRange(0, ItemRowNames.Num() - 1)];
		const FTN_InventoryItem* Row = ItemDataTable->FindRow<FTN_InventoryItem>(RowName, TEXT("TN_ItemSpawnZone::BeginPlay"));
		if (!Row || !Row->PickupActorClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("[ItemSpawnZone] Fila '%s' inválida o sin PickupActorClass."), *RowName.ToString());
			continue;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		ATN_PickupInteractableBase* Pickup = GetWorld()->SpawnActor<ATN_PickupInteractableBase>(
			Row->PickupActorClass, SpawnLocation, FRotator::ZeroRotator, Params);

		if (Pickup)
		{
			Pickup->InitializeFromInventoryItem(*Row);
			SpawnedLocations.Add(SpawnLocation);
			++SuccessCount;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[ItemSpawnZone] '%s' — Spawneados %d/%d ítems."), *GetName(), SuccessCount, SpawnCount);
}

bool ATN_ItemSpawnZone::FindValidSpawnPoint(FVector& OutLocation, const TArray<FVector>& ExistingLocations) const
{
	if (!GetWorld() || !SpawnBox)
	{
		return false;
	}

	// ── Generar punto aleatorio en espacio LOCAL del box, luego transformar ──
	// Usar GetUnscaledBoxExtent() + GetComponentTransform() es correcto para
	// cualquier combinación de rotación y escala del actor.
	// Con GetScaledBoxExtent() + GetComponentLocation() (código anterior), si
	// el actor tenía escala muy pequeña la extensión era ~0 → todos los
	// offsets eran (0,0,0) → todos los ítems aparecían en el centro.
	const FVector UnscaledExtent = SpawnBox->GetUnscaledBoxExtent();

	// Validación: si el box tiene extensión casi nula, loguear y salir
	if (UnscaledExtent.X < 10.f || UnscaledExtent.Y < 10.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ItemSpawnZone] '%s' — BoxExtent muy pequeño (%.1f, %.1f). "
			"Revisa el SpawnBox en el Editor o la escala del actor."),
			*GetName(), UnscaledExtent.X, UnscaledExtent.Y);
		return false;
	}

	// Punto en espacio local del componente (Z=0 → la capa media del box)
	const FVector LocalPoint(
		FMath::FRandRange(-UnscaledExtent.X, UnscaledExtent.X),
		FMath::FRandRange(-UnscaledExtent.Y, UnscaledExtent.Y),
		0.f
	);

	// Transformar a espacio mundo (incluye scale + rotation + translation)
	const FVector TestLocation = SpawnBox->GetComponentTransform().TransformPosition(LocalPoint);

	// Extensión en escala mundo para los rangos del line trace
	const FVector ScaledExtent = SpawnBox->GetScaledBoxExtent();
	const float TraceHalfZ = FMath::Max(ScaledExtent.Z, 50.f); // mínimo 50cm

	// Verificar distancia mínima con ítems ya spawneados
	for (const FVector& Existing : ExistingLocations)
	{
		if (FVector::DistSquared(TestLocation, Existing) < MinSpacing * MinSpacing)
		{
			return false;
		}
	}

	// ── Line trace al suelo ───────────────────────────────────────────────────
	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TN_SpawnZoneFloor), false);
	const FVector TraceStart = TestLocation + FVector(0.f, 0.f, TraceHalfZ + 50.f);
	const FVector TraceEnd   = TestLocation - FVector(0.f, 0.f, TraceHalfZ + 500.f);

	// Primero WorldStatic, luego Visibility como fallback
	bool bHitFloor = GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams);
	if (!bHitFloor)
	{
		bHitFloor = GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
	}

	if (!bHitFloor)
	{
		return false; // No hay suelo bajo este punto
	}

	// Centro del sweep: ENCIMA del radio de la esfera para no intersectar el suelo.
	constexpr float SphereRadius  = 30.f;
	constexpr float SphereZOffset = SphereRadius + 10.f; // 40 cm sobre el suelo
	const FVector GroundPoint = Hit.ImpactPoint + FVector(0.f, 0.f, SphereZOffset);

	// Comprobar obstáculos DINÁMICOS en esa posición (otros pickups, pawns).
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams SweepParams(SCENE_QUERY_STAT(TN_SpawnZoneObstacle), false);
	const FCollisionShape SweepShape = FCollisionShape::MakeSphere(SphereRadius);

	if (GetWorld()->OverlapAnyTestByObjectType(GroundPoint, FQuat::Identity, ObjParams, SweepShape, SweepParams))
	{
		return false; // Obstáculo dinámico presente (otro pickup, personaje, etc.)
	}

	// Posición final: sobre el punto de impacto del suelo + pequeño offset para el mesh
	OutLocation = Hit.ImpactPoint + FVector(0.f, 0.f, 5.f);
	return true;
}

