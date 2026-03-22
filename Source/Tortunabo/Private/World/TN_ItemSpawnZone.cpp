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

	const FVector BoxOrigin = SpawnBox->GetComponentLocation();
	const FVector BoxExtent = SpawnBox->GetScaledBoxExtent();

	// Posición random dentro del box (solo XY; Z se calcula desde el trace al suelo)
	const FVector RandomOffset(
		FMath::FRandRange(-BoxExtent.X, BoxExtent.X),
		FMath::FRandRange(-BoxExtent.Y, BoxExtent.Y),
		0.f
	);
	const FVector TestLocation = BoxOrigin + RandomOffset;

	// Verificar distancia mínima con ítems ya spawneados
	for (const FVector& Existing : ExistingLocations)
	{
		if (FVector::DistSquared(TestLocation, Existing) < MinSpacing * MinSpacing)
		{
			return false;
		}
	}

	// Line trace al suelo (desde la cima del box hacia abajo)
	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TN_SpawnZoneFloor), false);
	const FVector TraceStart = TestLocation + FVector(0.f, 0.f, BoxExtent.Z);
	const FVector TraceEnd   = TestLocation - FVector(0.f, 0.f, BoxExtent.Z + 500.f);

	if (!GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
	{
		return false; // No hay suelo bajo este punto
	}

	// Centro del sweep: ENCIMA del radio de la esfera para no intersectar el suelo.
	// Radio 30 cm → offset mínimo 30 cm. Usamos 40 cm para un margen seguro.
	constexpr float SphereRadius  = 30.f;
	constexpr float SphereZOffset = SphereRadius + 10.f; // 40 cm sobre el suelo
	const FVector GroundPoint = Hit.ImpactPoint + FVector(0.f, 0.f, SphereZOffset);

	// Comprobar obstáculos DINÁMICOS en esa posición (otros pickups, pawns).
	// Usamos OverlapAnyTestByObjectType (BY OBJECT TYPE) en lugar de
	// OverlapBlockingTestByChannel (BY TRACE CHANNEL) para que la geometría
	// estática del suelo (WorldStatic) no sea detectada como obstáculo.
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

