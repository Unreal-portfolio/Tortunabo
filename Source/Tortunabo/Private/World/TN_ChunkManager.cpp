#include "World/TN_ChunkManager.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

ATN_ChunkManager::ATN_ChunkManager()
{
	PrimaryActorTick.bCanEverTick = false;

	// Este actor no necesita replicarse — sólo existe en el servidor.
	// Los chunks que spawnea sí replican (configurado en cada chunk BP).
	bReplicates = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginPlay
// ─────────────────────────────────────────────────────────────────────────────

void ATN_ChunkManager::BeginPlay()
{
	Super::BeginPlay();

	// El ChunkManager sólo funciona en el servidor.
	// En clientes no hace nada — los actores spawneados llegan por replicación.
	if (!HasAuthority())
	{
		return;
	}

	// Validar que hay al menos un pool configurado
	if (EasyChunkClasses.Num() == 0 && MediumChunkClasses.Num() == 0 && HardChunkClasses.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[ChunkManager] No hay chunks configurados en ningún pool. "
			"Asigna BPs en EasyChunkClasses / MediumChunkClasses / HardChunkClasses."));
		return;
	}

	// El punto de spawn del primer chunk = la posición del ChunkManager en el mapa.
	NextSpawnTransform = GetActorTransform();

	// Pre-llenar el buffer inicial: spawnear (KeepAhead + KeepBehind - 1) chunks
	// al inicio para que el jugador ya tenga camino desde el primer frame.
	const int32 InitialCount = FMath::Max(1, GetKeepAliveCount() - 1);
	for (int32 i = 0; i < InitialCount; ++i)
	{
		SpawnNextChunk();
	}

	UE_LOG(LogTemp, Log, TEXT("[ChunkManager] Inicializado. Buffer inicial: %d chunks."), InitialCount);
}

// ─────────────────────────────────────────────────────────────────────────────
// Dificultad actual
// ─────────────────────────────────────────────────────────────────────────────

ETNChunkDifficulty ATN_ChunkManager::GetCurrentDifficulty() const
{
	if (PassedChunkCount < EasyToMediumThreshold)
	{
		return ETNChunkDifficulty::Easy;
	}
	if (PassedChunkCount < MediumToHardThreshold)
	{
		return ETNChunkDifficulty::Medium;
	}
	return ETNChunkDifficulty::Hard;
}

// ─────────────────────────────────────────────────────────────────────────────
// Selección aleatoria del pool
// ─────────────────────────────────────────────────────────────────────────────

TSubclassOf<AActor> ATN_ChunkManager::SelectRandomFromPool(
	const TArray<TSubclassOf<AActor>>& Pool,
	int32& OutSelectedIndex) const
{
	if (Pool.Num() == 0)
	{
		return nullptr;
	}

	if (Pool.Num() == 1)
	{
		OutSelectedIndex = 0;
		return Pool[0];
	}

	// Evitar repetir el mismo índice consecutivamente
	int32 NewIndex = LastSelectedIndex;
	int32 Attempts = 0;
	while (NewIndex == LastSelectedIndex && Attempts < 10)
	{
		NewIndex = FMath::RandRange(0, Pool.Num() - 1);
		++Attempts;
	}
	OutSelectedIndex = NewIndex;
	return Pool[NewIndex];
}

// ─────────────────────────────────────────────────────────────────────────────
// SpawnNextChunk
// ─────────────────────────────────────────────────────────────────────────────

void ATN_ChunkManager::SpawnNextChunk()
{
	if (bFinalSpawned)
	{
		return;
	}

	// Determinar pool según dificultad actual, con fallback a otros pools
	ETNChunkDifficulty Difficulty = GetCurrentDifficulty();

	// Intentar el pool primario, luego los otros como fallback
	const TArray<TSubclassOf<AActor>>* PrimaryPool   = nullptr;
	const TArray<TSubclassOf<AActor>>* Fallback1Pool = nullptr;
	const TArray<TSubclassOf<AActor>>* Fallback2Pool = nullptr;

	switch (Difficulty)
	{
		case ETNChunkDifficulty::Easy:
			PrimaryPool   = &EasyChunkClasses;
			Fallback1Pool = &MediumChunkClasses;
			Fallback2Pool = &HardChunkClasses;
			break;
		case ETNChunkDifficulty::Medium:
			PrimaryPool   = &MediumChunkClasses;
			Fallback1Pool = &EasyChunkClasses;
			Fallback2Pool = &HardChunkClasses;
			break;
		case ETNChunkDifficulty::Hard:
			PrimaryPool   = &HardChunkClasses;
			Fallback1Pool = &MediumChunkClasses;
			Fallback2Pool = &EasyChunkClasses;
			break;
	}

	int32 SelectedIndex = LastSelectedIndex;
	TSubclassOf<AActor> ChunkClass = SelectRandomFromPool(*PrimaryPool, SelectedIndex);

	if (!ChunkClass && Fallback1Pool)
	{
		ChunkClass = SelectRandomFromPool(*Fallback1Pool, SelectedIndex);
		UE_LOG(LogTemp, Warning, TEXT("[ChunkManager] Pool primario vacío para dificultad %d — usando fallback 1."),
			(int32)Difficulty);
	}
	if (!ChunkClass && Fallback2Pool)
	{
		ChunkClass = SelectRandomFromPool(*Fallback2Pool, SelectedIndex);
		UE_LOG(LogTemp, Warning, TEXT("[ChunkManager] Fallback 1 vacío — usando fallback 2."));
	}

	if (!ChunkClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[ChunkManager] SpawnNextChunk: Todos los pools están vacíos."));
		return;
	}

	AActor* SpawnedChunk = SpawnAlignedChunk(ChunkClass, NextSpawnTransform);
	if (!SpawnedChunk)
	{
		return;
	}

	LastSelectedIndex = SelectedIndex;

	// Registrar el EndTrigger de este chunk para detectar el paso del jugador
	if (UBoxComponent* EndTrigger = FindBoxComponentByName(SpawnedChunk, TEXT("EndTrigger")))
	{
		EndTrigger->OnComponentBeginOverlap.AddDynamic(this, &ATN_ChunkManager::OnChunkEndOverlap);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ChunkManager] Chunk '%s' no tiene BoxComponent 'EndTrigger'. "
			"El jugador no podrá avanzar."), *GetNameSafe(SpawnedChunk));
	}

	// Actualizar NextSpawnTransform al OutSocket de este chunk
	if (USceneComponent* OutSocket = FindSceneComponentByName(SpawnedChunk, TEXT("OutSocket")))
	{
		NextSpawnTransform = OutSocket->GetComponentTransform();

		if (bDebugDrawSockets)
		{
			DrawDebugSphere(GetWorld(), NextSpawnTransform.GetLocation(), 30.f, 8,
				FColor::Green, false, 10.f);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ChunkManager] Chunk '%s' no tiene SceneComponent 'OutSocket'. "
			"Los chunks siguientes se superpondrán."), *GetNameSafe(SpawnedChunk));
	}

	ActiveChunks.Add(SpawnedChunk);
	CleanupChunks();

	UE_LOG(LogTemp, Log, TEXT("[ChunkManager] Spawneado chunk %s (dificultad=%d, total pasados=%d)."),
		*GetNameSafe(SpawnedChunk), (int32)Difficulty, PassedChunkCount);
}

// ─────────────────────────────────────────────────────────────────────────────
// SpawnFinalChunk
// ─────────────────────────────────────────────────────────────────────────────

void ATN_ChunkManager::SpawnFinalChunk()
{
	if (bFinalSpawned)
	{
		return;
	}
	bFinalSpawned = true;

	if (!FinalChunkClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ChunkManager] FinalChunkClass no asignado. "
			"La carrera no tendrá meta. Asigna un BP con ATN_FinishLineVolume en BP_TN_ChunkManager."));
		return;
	}

	AActor* FinalChunk = SpawnAlignedChunk(FinalChunkClass, NextSpawnTransform);
	if (!FinalChunk)
	{
		return;
	}

	ActiveChunks.Add(FinalChunk);
	// El chunk final no necesita EndTrigger — la FinishLineVolume dentro de él
	// ya se encarga de notificar el fin de carrera al RunGameMode.

	UE_LOG(LogTemp, Log, TEXT("[ChunkManager] Chunk FINAL spawneado: '%s'."), *GetNameSafe(FinalChunk));
}

// ─────────────────────────────────────────────────────────────────────────────
// SpawnAlignedChunk — núcleo de alineación InSocket→Target
// ─────────────────────────────────────────────────────────────────────────────

AActor* ATN_ChunkManager::SpawnAlignedChunk(TSubclassOf<AActor> ChunkClass, const FTransform& TargetTransform)
{
	UWorld* World = GetWorld();
	if (!World || !ChunkClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Paso 1: Spawnear temporalmente en la identidad para leer el offset del InSocket
	AActor* TempActor = World->SpawnActor<AActor>(ChunkClass, FTransform::Identity, SpawnParams);
	if (!TempActor)
	{
		UE_LOG(LogTemp, Error, TEXT("[ChunkManager] No se pudo spawnear '%s'."),
			*ChunkClass->GetName());
		return nullptr;
	}

	// Paso 2: Localizar el InSocket y obtener su transform relativo al actor
	USceneComponent* InSocket = FindSceneComponentByName(TempActor, TEXT("InSocket"));
	FTransform InSocketRelative = FTransform::Identity;

	if (InSocket)
	{
		// GetRelativeTransform devuelve el offset relativo al actor (no al mundo)
		InSocketRelative = InSocket->GetComponentTransform().GetRelativeTransform(
			TempActor->GetActorTransform());

		if (bDebugDrawSockets)
		{
			DrawDebugSphere(GetWorld(), InSocket->GetComponentLocation(), 25.f, 8,
				FColor::Red, false, 10.f);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ChunkManager] '%s' no tiene SceneComponent 'InSocket'. "
			"Se usará la posición del actor como punto de entrada."), *ChunkClass->GetName());
	}

	// Paso 3: Calcular el transform final del actor para que InSocket quede en TargetTransform
	// FinalActorTransform = TargetTransform * InSocketRelative.Inverse()
	FTransform FinalTransform = InSocketRelative.Inverse() * TargetTransform;

	// Paso 4: Mover el actor a la posición correcta
	TempActor->SetActorTransform(FinalTransform);

	return TempActor;
}

// ─────────────────────────────────────────────────────────────────────────────
// CleanupChunks
// ─────────────────────────────────────────────────────────────────────────────

void ATN_ChunkManager::CleanupChunks()
{
	const int32 MaxChunks = GetKeepAliveCount();

	// Limpiar entradas inválidas (chunks que ya fueron destruidos)
	ActiveChunks.RemoveAll([](const TWeakObjectPtr<AActor>& Ptr)
	{
		return !Ptr.IsValid();
	});

	// Destruir los más antiguos mientras se supere el máximo
	while (ActiveChunks.Num() > MaxChunks)
	{
		TWeakObjectPtr<AActor> OldestChunk = ActiveChunks[0];
		ActiveChunks.RemoveAt(0);

		if (OldestChunk.IsValid())
		{
			EndTriggeredChunks.Remove(OldestChunk);
			OldestChunk->Destroy();
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// OnChunkEndOverlap — el jugador cruzó el EndTrigger de un chunk
// ─────────────────────────────────────────────────────────────────────────────

void ATN_ChunkManager::OnChunkEndOverlap(
	UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	// Solo en servidor
	if (!HasAuthority())
	{
		return;
	}

	// Solo contar Pawns (no proyectiles, objetos físicos, etc.)
	if (!OtherActor || !OtherActor->IsA<APawn>())
	{
		return;
	}

	// El chunk que contiene este EndTrigger
	AActor* OwnerChunk = OverlappedComp ? OverlappedComp->GetOwner() : nullptr;
	if (!OwnerChunk)
	{
		return;
	}

	// Evitar doble-trigger: en multijugador varios jugadores pueden pasar por el mismo chunk
	TWeakObjectPtr<AActor> WeakChunk(OwnerChunk);
	if (EndTriggeredChunks.Contains(WeakChunk))
	{
		return;
	}
	EndTriggeredChunks.Add(WeakChunk);

	// Avanzar el contador de progreso
	++PassedChunkCount;

	UE_LOG(LogTemp, Log, TEXT("[ChunkManager] Jugador '%s' cruzó EndTrigger del chunk '%s'. "
		"Chunks pasados: %d / %d."),
		*GetNameSafe(OtherActor), *GetNameSafe(OwnerChunk),
		PassedChunkCount, TotalChunksBeforeFinal);

	// ¿Ya hemos pasado suficientes chunks? → Spawn del chunk final
	if (PassedChunkCount >= TotalChunksBeforeFinal)
	{
		SpawnFinalChunk();
	}
	else
	{
		SpawnNextChunk();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers de búsqueda de componentes
// ─────────────────────────────────────────────────────────────────────────────

USceneComponent* ATN_ChunkManager::FindSceneComponentByName(AActor* Actor, FName Name)
{
	if (!Actor)
	{
		return nullptr;
	}

	for (UActorComponent* Comp : Actor->GetComponents())
	{
		if (Comp && Comp->GetFName() == Name)
		{
			return Cast<USceneComponent>(Comp);
		}
	}

	// Fallback: comparación case-insensitive por si el BP tiene sufijo "_0" etc.
	const FString NameStr = Name.ToString();
	for (UActorComponent* Comp : Actor->GetComponents())
	{
		if (Comp)
		{
			const FString CompName = Comp->GetName();
			if (CompName.Equals(NameStr, ESearchCase::IgnoreCase))
			{
				return Cast<USceneComponent>(Comp);
			}
		}
	}

	return nullptr;
}

UBoxComponent* ATN_ChunkManager::FindBoxComponentByName(AActor* Actor, FName Name)
{
	USceneComponent* Found = FindSceneComponentByName(Actor, Name);
	return Found ? Cast<UBoxComponent>(Found) : nullptr;
}
