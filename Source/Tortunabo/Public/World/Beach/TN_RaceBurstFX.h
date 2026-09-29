#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/ProcMap/TN_ProcMapAmbientFX.h"
#include "TN_RaceBurstFX.generated.h"

class UStaticMeshComponent;

/** Efectos puntuales de los objetos de carrera (ATN_RaceBurstFX). */
UENUM()
enum class ETNRaceBurst : uint8
{
	/** Onda del silbato del sargento: anillos crema y una cúpula tenue que se abren desde el suelo. Size = radio final (cm). */
	WhistleWave,
	/** Nubecilla blanca-arena de humo con unas estrellitas (una caja de objetos que se abre, un objeto que aparece o se gasta). */
	Poof,
	/** Estallido dorado de chispas con una estrella de cuatro puntas que se expande y se apaga. */
	StarPop,
	/** Destellos dorados cortos. */
	Sparkle,
	Count
};

/**
 * Efectos puntuales de los objetos de carrera (Docs/Modo_Carrera.md, «Objetos de carrera»): la onda del silbato, la nubecilla
 * de humo, el estallido de estrellas y los destellos dorados. Son LOCALES: no se replican. El servidor avisa con un multicast
 * (p. ej. UTN_RaceItemComponent::MulticastWhistle) y cada máquina con pantalla crea el suyo con SpawnLocal; el actor se
 * anima con su propio Tick (mallas suaves en ejecución con el alfa en el vértice y emisores de partículas de TNAmbientFX
 * dentro del propio actor) y se destruye solo al acabar. Nada de esto existe en un servidor dedicado.
 */
UCLASS(NotBlueprintable)
class TORTUNABO_API ATN_RaceBurstFX : public AActor
{
	GENERATED_BODY()

public:
	ATN_RaceBurstFX();

	/**
	 * Crea el efecto Kind en Where solo en esta máquina (no hace nada en un servidor dedicado ni fuera de un mundo de
	 * juego). Size: en WhistleWave es el RADIO final de la onda en cm (5500 = 55 m); en los demás, un factor de escala
	 * (1 = normal). Los efectos pequeños no se crean si la cámara local está a más de 150 m.
	 */
	static void SpawnLocal(UWorld* World, ETNRaceBurst Kind, const FVector& Where, float Size = 1.f);

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	void BuildWhistle();
	void BuildPoof();
	void BuildStarPop();
	void BuildSparkle();

	/** Coloca la onda del silbato para el tiempo BurstAge. */
	void TickWhistle();

	/** Coloca la estrella de cuatro puntas (mira a la cámara, gira, crece y se apaga) para el tiempo BurstAge. */
	void TickStar(const FVector& View);

	/** true si a ningún emisor le queda nada vivo. */
	bool AreEmittersIdle() const;

	/** Cuál de los efectos es y su tamaño (los pone SpawnLocal antes de que el actor empiece a jugar). */
	ETNRaceBurst BurstKind = ETNRaceBurst::Poof;
	float BurstSize = 1.f;

	/** Segundos desde que nació. */
	float BurstAge = 0.f;

	/** Onda del silbato: primer anillo, segundo anillo (más lento y más frío) y cúpula. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> WaveRingA;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> WaveRingB;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> WaveDome;

	/** Estrella de cuatro puntas (estallido de estrellas y destellos). */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> StarFlash;

	/** Emisores propios del actor: el principal (humo o chispas doradas) y el secundario (estrellitas o chispas claras). */
	TNAmbientFX::FEmitter FXMain;
	TNAmbientFX::FEmitter FXExtra;
};
