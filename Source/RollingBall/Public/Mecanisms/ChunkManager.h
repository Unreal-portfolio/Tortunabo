#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChunkManager.generated.h"

UCLASS()
class ROLLINGBALL2_API AChunkManager : public AActor
{
	GENERATED_BODY()

public:
	AChunkManager();

	// Spawnea un chunk aleatorio alineado por InSocket (offset fijo)
	void SpawnNextChunk();

	// Spawnea el chunk final (BP distinto) alineado por InSocket (offset fijo)
	void SpawnFinalChunk();

	// Mantiene el buffer (destruye el más antiguo si sobran)
	void CleanupChunks();

	// Obtén el checkpoint actual (por si lo quieres leer desde BP)
	UFUNCTION(BlueprintCallable, Category="Player|Respawn")
	FTransform GetCurrentRespawnTransform() const { return CurrentRespawnTransform; }

	// Callback del EndTrigger
	UFUNCTION()
	void OnChunkEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

protected:
	virtual void BeginPlay() override;

private:

	/* =========================
	   Configuración de chunks
	   ========================= */

	// Clases de chunks (BP_Chunk_1, BP_Chunk_2, ...)
	UPROPERTY(EditAnywhere, Category="Chunks")
	TArray<TSubclassOf<AActor>> ChunkClasses;

	// Buffer: cuántos quieres por delante y por detrás
	UPROPERTY(EditAnywhere, Category="Chunks|Buffer")
	int32 KeepAhead = 5;

	UPROPERTY(EditAnywhere, Category="Chunks|Buffer")
	int32 KeepBehind = 1;

	// Chunk final (otro BP)
	UPROPERTY(EditAnywhere, Category="Chunks|Final")
	bool bUseFinalChunk = true;

	// Número de triggers/chunks que hay que pasar antes de spawnear el final (editable para pruebas)
	UPROPERTY(EditAnywhere, Category="Chunks|Final", meta=(ClampMin="1"))
	int32 ChunksUntilFinal = 10;

	// Clase del chunk final (BP_FinalChunk)
	UPROPERTY(EditAnywhere, Category="Chunks|Final")
	TSubclassOf<AActor> FinalChunkClass;

	// Punto inicial del primer chunk (root)
	UPROPERTY(VisibleAnywhere, Category="Chunks")
	USceneComponent* StartAnchor;

	/* =========================
	   Runtime
	   ========================= */

	// Chunks activos
	UPROPERTY()
	TArray<AActor*> ActiveChunks;

	// Transform donde debe caer el InSocket del siguiente chunk
	FTransform NextSpawnTransform;

	// Evitar repetir el mismo chunk seguido
	int32 LastIndex = -1;

	// Contador de chunks pasados
	int32 PassedChunkCount = 0;

	// Para no spawnear el final dos veces
	bool bFinalSpawned = false;

	// Checkpoint actual (donde respawn)
	UPROPERTY(VisibleAnywhere, Category="Player|Respawn")
	FTransform CurrentRespawnTransform;

	// Evitar doble disparo por chunk (guardamos el actor dueño del EndTrigger)
	UPROPERTY()
	TSet<TWeakObjectPtr<AActor>> EndTriggeredChunks;

	// Helper: total vivo
	int32 GetKeepAliveCount() const { return KeepAhead + KeepBehind; }

	// Helper: busca un SceneComponent por nombre en una instancia
	USceneComponent* FindSceneComponentByName(AActor* Actor, const FName& Name) const;

	// Helper: busca un BoxComponent por nombre en una instancia
	class UBoxComponent* FindBoxComponentByName(AActor* Actor, const FName& Name) const;
};
