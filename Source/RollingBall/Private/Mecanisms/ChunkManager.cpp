#include "Mecanisms/ChunkManager.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

AChunkManager::AChunkManager()
{
	PrimaryActorTick.bCanEverTick = false;

	StartAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("StartAnchor"));
	RootComponent = StartAnchor;
}

void AChunkManager::BeginPlay()
{
	Super::BeginPlay();

	ActiveChunks.Empty();
	EndTriggeredChunks.Empty();

	PassedChunkCount = 0;
	bFinalSpawned = false;

	// Donde coloques BP_ChunkManager en el editor = donde debe caer el InSocket del primer chunk
	NextSpawnTransform = GetActorTransform();

	// Respawn inicial: el inicio
	CurrentRespawnTransform = NextSpawnTransform;

	// Al inicio quieres 5 (si KeepAhead=5, KeepBehind=1 -> total 6)
	// para que al pasar el primer trigger se convierta en 6 sin borrar.
	const int32 InitialSpawnCount = FMath::Max(1, GetKeepAliveCount() - 1);

	for (int32 i = 0; i < InitialSpawnCount; i++)
	{
		SpawnNextChunk();
	}
}

USceneComponent* AChunkManager::FindSceneComponentByName(AActor* Actor, const FName& Name) const
{
	if (!Actor) return nullptr;

	TArray<USceneComponent*> Scene;
	Actor->GetComponents<USceneComponent>(Scene);

	for (USceneComponent* Comp : Scene)
	{
		if (Comp && Comp->GetFName() == Name)
		{
			return Comp;
		}
	}
	return nullptr;
}

UBoxComponent* AChunkManager::FindBoxComponentByName(AActor* Actor, const FName& Name) const
{
	if (!Actor) return nullptr;

	TArray<UBoxComponent*> Boxes;
	Actor->GetComponents<UBoxComponent>(Boxes);

	for (UBoxComponent* Comp : Boxes)
	{
		if (Comp && Comp->GetFName() == Name)
		{
			return Comp;
		}
	}
	return nullptr;
}

void AChunkManager::SpawnNextChunk()
{
	if (!GetWorld() || ChunkClasses.Num() == 0)
		return;

	// Si ya spawneaste el final, no sigas generando chunks aleatorios
	if (bUseFinalChunk && bFinalSpawned)
		return;

	// Elegir chunk aleatorio (evitar repetir seguido)
	int32 Index;
	do
	{
		Index = FMath::RandRange(0, ChunkClasses.Num() - 1);
	}
	while (Index == LastIndex && ChunkClasses.Num() > 1);
	LastIndex = Index;

	TSubclassOf<AActor> ChunkClass = ChunkClasses[Index];
	if (!*ChunkClass) return;

	// === OFFSET FIJO DEL InSocket local ===
	// Debe coincidir con el InSocket de TODOS tus chunks
	const FVector InSocketLocalLocation(-7600.f, 0.f, 150.f);
	const FRotator InSocketLocalRotation(0.f, 0.f, 0.f); // si no lo rotas en el BP, déjalo así

	// Target: donde debe caer el InSocket
	const FVector TargetLoc = NextSpawnTransform.GetLocation();
	const FQuat   TargetRot = NextSpawnTransform.GetRotation();

	// Actor rotación: igual que el target (ajustada por rotación local del InSocket si la hubiera)
	const FQuat ActorRot = TargetRot * InSocketLocalRotation.Quaternion().Inverse();

	// Actor posición: Target - (ActorRot * InSocketLocalLocation)
	const FVector ActorLoc = TargetLoc - ActorRot.RotateVector(InSocketLocalLocation);

	FTransform SpawnTransform;
	SpawnTransform.SetLocation(ActorLoc);
	SpawnTransform.SetRotation(ActorRot);
	SpawnTransform.SetScale3D(FVector::OneVector);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// ✅ Spawn directo ya alineado (BeginPlay del chunk ocurre ya en el sitio correcto)
	AActor* NewChunk = GetWorld()->SpawnActor<AActor>(ChunkClass, SpawnTransform, Params);
	if (!NewChunk) return;

	// Bind del EndTrigger
	UBoxComponent* EndTrigger = FindBoxComponentByName(NewChunk, TEXT("EndTrigger"));
	if (EndTrigger)
	{
		EndTrigger->OnComponentBeginOverlap.AddDynamic(this, &AChunkManager::OnChunkEndOverlap);
	}

	// Actualizar NextSpawnTransform usando OutSocket (world)
	USceneComponent* OutSocket = FindSceneComponentByName(NewChunk, TEXT("OutSocket"));
	if (OutSocket)
	{
		NextSpawnTransform = OutSocket->GetComponentTransform();
	}

	ActiveChunks.Add(NewChunk);
	CleanupChunks();
}

void AChunkManager::SpawnFinalChunk()
{
	if (!GetWorld() || !*FinalChunkClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnFinalChunk: FinalChunkClass no asignada"));
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Spawn temporal (identity) para poder leer el InSocket REAL del BP final
	AActor* FinalChunk = GetWorld()->SpawnActor<AActor>(FinalChunkClass, FTransform::Identity, Params);
	if (!FinalChunk) return;

	// Buscar InSocket real del final
	USceneComponent* InSocket = FindSceneComponentByName(FinalChunk, TEXT("InSocket"));
	if (!InSocket)
	{
		UE_LOG(LogTemp, Error, TEXT("FinalChunk %s: no tiene InSocket (nombre exacto?)"), *FinalChunk->GetName());
		FinalChunk->Destroy();
		return;
	}

	// Calcular transform final usando el InSocket REAL (no fijo)
	const FTransform InLocal = InSocket->GetRelativeTransform();
	const FTransform FinalActorTransform = InLocal.Inverse() * NextSpawnTransform;

	FinalChunk->SetActorTransform(FinalActorTransform, false, nullptr, ETeleportType::TeleportPhysics);

	ActiveChunks.Add(FinalChunk);
	CleanupChunks();
}


void AChunkManager::CleanupChunks()
{
	const int32 KeepAlive = GetKeepAliveCount();

	while (ActiveChunks.Num() > KeepAlive)
	{
		AActor* Oldest = ActiveChunks[0];
		ActiveChunks.RemoveAt(0);

		if (Oldest)
		{
			Oldest->Destroy();
		}
	}
}

void AChunkManager::OnChunkEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	// Solo reaccionar si es el Pawn (tu bola)
	if (!OtherActor || !OtherActor->IsA<APawn>()) return;

	AActor* ChunkActor = OverlappedComponent ? OverlappedComponent->GetOwner() : nullptr;
	if (!ChunkActor) return;

	// Evitar doble disparo del mismo chunk
	if (EndTriggeredChunks.Contains(ChunkActor))
		return;
	EndTriggeredChunks.Add(ChunkActor);

	// 1) Actualizar checkpoint al InSocket del chunk ACTUAL
	USceneComponent* InSocket = FindSceneComponentByName(ChunkActor, TEXT("InSocket"));
	if (InSocket)
	{
		CurrentRespawnTransform = InSocket->GetComponentTransform();
	}

	// 2) Contar progreso
	PassedChunkCount++;

	// 3) Si toca final, spawnearlo y parar aleatorios
	if (bUseFinalChunk && !bFinalSpawned && PassedChunkCount >= ChunksUntilFinal)
	{
		SpawnFinalChunk();
		bFinalSpawned = true;
		return;
	}

	// 4) Si no, spawnear siguiente normal
	SpawnNextChunk();
}
