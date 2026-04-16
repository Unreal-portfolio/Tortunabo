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

	// El ChunkManager en sí no se replica — sólo existe en el servidor.
	// Los chunks que spawnea tienen SetReplicates(true) activado en SpawnAlignedChunk,
	// por lo que UE los envía automáticamente a todos los clientes.
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

	// Validar secuencia personalizada si está activa
	if (!bUseRandomGeneration && CustomChunkSequence.Num() < TotalChunksBeforeFinal)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[ChunkManager] CustomChunkSequence tiene %d entradas pero TotalChunksBeforeFinal=%d. "
			     "Los chunks restantes usarán Hard como fallback."),
			CustomChunkSequence.Num(), TotalChunksBeforeFinal);
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

	// Determinar dificultad: modo aleatorio (umbrales) o secuencia personalizada
	ETNChunkDifficulty Difficulty;
	if (!bUseRandomGeneration)
	{
		if (CustomChunkSequence.IsValidIndex(PassedChunkCount))
		{
			Difficulty = CustomChunkSequence[PassedChunkCount];
		}
		else
		{
			// Fuera del rango de la secuencia → Hard como fallback
			Difficulty = ETNChunkDifficulty::Hard;
			UE_LOG(LogTemp, Warning,
				TEXT("[ChunkManager] SpawnNextChunk: PassedChunkCount=%d supera CustomChunkSequence.Num()=%d — usando Hard."),
				PassedChunkCount, CustomChunkSequence.Num());
		}
	}
	else
	{
		Difficulty = GetCurrentDifficulty();
	}

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

	// Desconectar el trigger anterior y conectar solo el del nuevo chunk.
	// Tener un único trigger activo garantiza que un solo SpawnNextChunk se llame
	// por chunk cruzado, independientemente de cuántos jugadores haya.
	if (ActiveEndTrigger.IsValid())
	{
		ActiveEndTrigger->OnComponentBeginOverlap.RemoveDynamic(this, &ATN_ChunkManager::OnChunkEndOverlap);
	}

	if (UBoxComponent* EndTrigger = FindBoxComponentByName(SpawnedChunk, TEXT("EndTrigger")))
	{
		EndTrigger->OnComponentBeginOverlap.AddDynamic(this, &ATN_ChunkManager::OnChunkEndOverlap);
		ActiveEndTrigger = EndTrigger;
	}
	else
	{
		ActiveEndTrigger = nullptr;
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
// GetOrComputeInSocketTransform — caché de InSocket por clase
// ─────────────────────────────────────────────────────────────────────────────

FTransform ATN_ChunkManager::GetOrComputeInSocketTransform(TSubclassOf<AActor> ChunkClass)
{
	// Buscar en caché
	if (FTransform* Cached = InSocketCache.Find(ChunkClass.Get()))
	{
		return *Cached;
	}

	// No está en caché → spawnar un temporal lejos del área de juego para leer InSocket.
	// Se spawnea en una posición remota para que los Child Actors no puedan
	// causar overlaps con jugadores en (0,0,0). Se destruye inmediatamente.
	FTransform InSocketTransform = FTransform::Identity;

	UWorld* World = GetWorld();
	if (!World)
	{
		InSocketCache.Add(ChunkClass.Get(), InSocketTransform);
		return InSocketTransform;
	}

	// Spawn far away from gameplay — prevents false overlaps with pawns at/near origin
	const FTransform TempSpawnTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, -50000.f));

	FActorSpawnParameters TempParams;
	TempParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	TempParams.bNoFail = true;

	AActor* Temp = World->SpawnActor<AActor>(ChunkClass, TempSpawnTransform, TempParams);
	if (Temp)
	{
		if (USceneComponent* InSocket = FindSceneComponentByName(Temp, TEXT("InSocket")))
		{
			// InSocket's world transform at TempSpawnTransform minus the spawn location
			// gives us the relative offset (same as spawning at Identity).
			// Since we only care about the relative transform from actor root to InSocket,
			// compute it explicitly.
			InSocketTransform = InSocket->GetComponentTransform().GetRelativeTransform(Temp->GetActorTransform());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[ChunkManager] Chunk '%s' no tiene 'InSocket'. "
				"Se usará la posición del actor como entrada."), *ChunkClass->GetName());
		}
		Temp->Destroy();
	}

	InSocketCache.Add(ChunkClass.Get(), InSocketTransform);
	return InSocketTransform;
}

// ─────────────────────────────────────────────────────────────────────────────
// SpawnAlignedChunk — núcleo de alineación InSocket→Target con replicación
// ─────────────────────────────────────────────────────────────────────────────

AActor* ATN_ChunkManager::SpawnAlignedChunk(TSubclassOf<AActor> ChunkClass, const FTransform& TargetTransform)
{
	UWorld* World = GetWorld();
	if (!World || !ChunkClass)
	{
		return nullptr;
	}

	// ── Paso 1: Calcular la posición final ANTES de spawnar ─────────────────
	// El enfoque anterior (spawn en Identity + teleport) rompía los Child Actors:
	// BeginPlay de los hijos (SeagullActor, ButtonInteractable, ItemSpawnZone,
	// DeathZone, etc.) corría en (0,0,0) y cacheaba posiciones erróneas.
	//
	// Solución: obtener el InSocket offset (cacheado por clase) y calcular la
	// posición final. Spawnar el chunk real directamente en esa posición.
	// BeginPlay de TODOS los actores y Child Actors corre en la posición correcta.

	const FTransform InSocketTransform = GetOrComputeInSocketTransform(ChunkClass);
	const FTransform FinalTransform = InSocketTransform.Inverse() * TargetTransform;

	if (bDebugDrawSockets)
	{
		DrawDebugSphere(World, TargetTransform.GetLocation(), 25.f, 8,
			FColor::Red, false, 10.f);
	}

	// ── Paso 2: Spawnar el chunk REAL en la posición final ───────────────────
	// BeginPlay de todos los componentes y Child Actors corre en la posición
	// world correcta → InitialLocation, Waypoints, SpawnZone, todo bien.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* Chunk = World->SpawnActor<AActor>(ChunkClass, FinalTransform, Params);
	if (!Chunk)
	{
		UE_LOG(LogTemp, Error, TEXT("[ChunkManager] SpawnActor falló para '%s'."),
			*ChunkClass->GetName());
		return nullptr;
	}

	// ── Paso 3: Activar replicación ──────────────────────────────────────────
	// La red envía el actor ya en la posición final al primer tick de red.
	Chunk->SetReplicates(true);
	Chunk->SetReplicateMovement(false); // chunks son estáticos

	return Chunk;
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
	// (no hace falta dedup: solo hay un trigger activo a la vez)
	AActor* OwnerChunk = OverlappedComp ? OverlappedComp->GetOwner() : nullptr;
	if (!OwnerChunk)
	{
		return;
	}

	// Desconectar inmediatamente para que ningún otro jugador vuelva a dispararlo.
	if (ActiveEndTrigger.IsValid())
	{
		ActiveEndTrigger->OnComponentBeginOverlap.RemoveDynamic(this, &ATN_ChunkManager::OnChunkEndOverlap);
		ActiveEndTrigger = nullptr;
	}

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

// ─────────────────────────────────────────────────────────────────────────────
// GetSafeReviveLocation
// ─────────────────────────────────────────────────────────────────────────────

FVector ATN_ChunkManager::GetSafeReviveLocation() const
{
	// NextSpawnTransform es el OutSocket del último chunk spawneado.
	// Siempre apunta a un área activa (el próximo chunk se spawneará aquí).
	// Elevamos 100 cm para evitar que el pawn aparezca dentro del suelo.
	return NextSpawnTransform.GetLocation() + FVector(0.f, 0.f, 100.f);
}
