#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_TutorialCourse.generated.h"

class APawn;
class APlayerController;
class ATortugaCharacter;
class ATN_JellyfishTrampoline;
class ATN_ProcWaterVolume;
class ATN_TutorialCatapult;
class ATN_TutorialDummy;
class ATN_TutorialFauna;
class ATN_TutorialSearchSpot;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;
class UProceduralMeshComponent;
class USceneComponent;
class UStaticMesh;
class UTextRenderComponent;

/**
 * @brief Pasillo del tutorial de la primera partida, flotando 180 m por encima del lobby (Docs/Tutorial.md).
 *
 * Dos islas alargadas con el aspecto del mapa del cooperativo (terreno con el material, los colores de bioma y el camino
 * de ATN_ProcMapGenerator, su vegetación, sus objetos sueltos y su fauna) por las que se recorren 19 estaciones, una por
 * cosa que hay que saber: moverse, correr, saltar, panzazo, objetos, caparazón, trampolín, catapulta, nadar, cargar y
 * liberarse, bailes y frases, voz y menú de pausa. Al final, una cascada cae hacia el castillo: se desvanece a mitad de la
 * caída y la tortuga aterriza en la plaza, sin daño ni bola. El recorrido es siempre el mismo (TN_TutorialLayout.h).
 *
 * Quién lo hace: cada jugador la primera vez (su guardado local, UTN_TutorialSaveGame). Su UTN_TutorialPlayerComponent
 * lo pide al servidor; un cliente que se une por primera vez lo dice además en la URL (?TNTut=1) y aparece ya aquí. Varios
 * a la vez; los demás siguen en el lobby. Al acabar (o al saltarlo) se apunta en su guardado.
 *
 * Red: el actor se replica sin propiedades que cambien (la cascada sobre el sitio de aterrizaje va en su transformada).
 * La malla, la vegetación, el agua y la fauna se construyen en cada máquina cuando hace falta (EnsureBuilt: el servidor con
 * el primer participante, un cliente cuando su jugador va a entrar) y son iguales en todas. Lo que tiene juego (la barrita,
 * el montículo, el cangrejo de prácticas, la medusa, la catapulta y las tortugas de práctica) lo crea el servidor y se
 * replica solo a quien está cerca (no a los del lobby). En cada máquina el recorrido se ve solo con la cámara arriba (quien
 * lo hace o cae por la cascada): desde el lobby no se ve.
 */
UCLASS()
class TORTUNABO_API ATN_TutorialCourse : public AActor
{
	GENERATED_BODY()

public:
	ATN_TutorialCourse();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** El recorrido de este mundo (null fuera del lobby). */
	static ATN_TutorialCourse* Find(const UObject* WorldContext);

	/**
	 * Servidor: lo crea sobre el lobby con la cascada encima de Landing (el suelo de la plaza donde se cae), a Dims::Height
	 * de altura; las islas se alargan hacia Yaw + 180° desde ahí.
	 */
	static ATN_TutorialCourse* SpawnAbove(UWorld* World, const FVector& Landing, float Yaw);

	// ── Servidor ─────────────────────────────────────────────────────────────

	/** Mete al jugador en el tutorial (desde el principio) si no está ya. */
	void StartFor(APlayerController* PC);

	/** Lo saca: saltado (vuelve al lobby con el menú de pausa o la consola) o terminado por la cascada. */
	void FinishFor(APlayerController* PC, bool bSkipped);

	/** Lleva al jugador a la estación N (lo mete en el tutorial si no estaba). Consola: TN.Tutorial.Station. */
	void GoToStation(APlayerController* PC, int32 StationIndex);

	bool IsParticipant(const APlayerController* PC) const;
	int32 NumParticipants() const { return Participants.Num(); }

	/** El cangrejo de prácticas ha recibido un golpe de algo que lanzó Thrower. */
	void NotifyDummyHit(APawn* Thrower);

	// ── Todas las máquinas ───────────────────────────────────────────────────

	/** Construye (una vez) la malla, la vegetación, el agua, la cascada, los carteles y la fauna. */
	void EnsureBuilt();
	bool IsBuilt() const { return bBuilt; }

	/** Espacio local del recorrido (TN_TutorialLayout.h) ⇄ mundo. */
	FVector LocalToWorld(const FVector& Local) const;
	FVector WorldToLocal(const FVector& WorldPoint) const;
	/** Giro (grados) del +X local en el mundo. */
	float LocalYawToWorld(float LocalYaw) const;

	/** Estación del tramo en el que cae un punto del mundo. */
	int32 StationAt(const FVector& WorldPoint) const;

	/** true si el punto (mundo) está sobre las islas o a menos de 60 m por debajo de ellas (la zona del tutorial). */
	bool IsInCourseSpace(const FVector& WorldPoint) const;

	/** Suelo del lobby donde se aterriza tras la cascada (debajo del borde). */
	FVector GetLandingSpot() const;

private:
	UPROPERTY(VisibleAnywhere, Category = "Tutorial")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Terreno de las dos islas (sección 0 con colisión: pasillo, taludes y meseta; sección 1 sin ella: la roca de debajo). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> IslandMeshes;

	/** Piezas con colisión (tronco del cañón, montículo, peana del cangrejo, carteles) y adornos sin ella. */
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> DecorMesh;

	/** Agua de la laguna y del arroyo. */
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> WaterMesh;

	/** La cascada (M_ProcCascade): se desvanece a mitad de la caída. */
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> CascadeMesh;

	/** Nubes alrededor y debajo de las islas y la bruma donde se deshace la cascada. */
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> CloudMesh;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> FloraComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> RuntimeMeshes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextRenderComponent>> SignTexts;

	/** Agua nadable de la laguna (local en cada máquina: el nado lo predicen servidor y cliente). */
	UPROPERTY(Transient)
	TObjectPtr<ATN_ProcWaterVolume> PoolVolume;

	/** Fauna ambiental (local). */
	UPROPERTY(Transient)
	TObjectPtr<ATN_TutorialFauna> Fauna;

	bool bBuilt = false;
	bool bLocalVisible = true;
	float VisibilityTimer = 0.f;

	// ── Servidor: participantes ──────────────────────────────────────────────

	struct FParticipant
	{
		TWeakObjectPtr<APlayerController> PC;
		/** Punto de control más avanzado pisado (TNTutorial::CheckpointX). */
		int32 Checkpoint = 0;
		/** Hueco de aparición (0-7). */
		int32 Slot = 0;
	};

	TArray<FParticipant> Participants;
	float ServerTimer = 0.f;
	/** Segundos sin nadie en el tutorial (las piezas de práctica se quitan a los 20 s). */
	float EmptySeconds = 0.f;

	// ── Servidor: piezas de práctica ─────────────────────────────────────────

	TWeakObjectPtr<AActor> BarPickup;
	double BarRespawnAt = -1.0;
	TWeakObjectPtr<ATN_TutorialSearchSpot> Mound;
	TWeakObjectPtr<ATN_TutorialDummy> Dummy;
	TWeakObjectPtr<ATN_JellyfishTrampoline> Jelly;
	TWeakObjectPtr<ATN_TutorialCatapult> Catapult;
	TWeakObjectPtr<ATortugaCharacter> Rodolfo;
	TWeakObjectPtr<ATortugaCharacter> Berta;
	/** Rodolfo: segundos de pie seguidos (se vuelve a meter en su caparazón para que lo cojan otra vez). */
	float RodolfoStandSeconds = 0.f;
	/** Berta: segundos llevando a alguien y hasta cuándo descansa antes de volver a coger. */
	float BertaCarrySeconds = 0.f;
	double BertaRestUntil = 0.0;

	// ── Construcción (TN_TutorialCourse_Build.cpp) ───────────────────────────

	UProceduralMeshComponent* NewMeshComponent(const TCHAR* DebugName, bool bCollision);
	void BuildIsland(int32 Part);
	void BuildDecor();
	void BuildWater();
	void BuildCascade();
	void BuildClouds();
	void BuildFlora();
	void BuildSigns();
	void SpawnLocalActors();

	// ── Visibilidad (cada máquina) ───────────────────────────────────────────

	void UpdateLocalVisibility();
	void SetLocalVisible(bool bVisible);

	// ── Servidor ─────────────────────────────────────────────────────────────

	void ServerTick(float DeltaSeconds);
	void TickParticipants();
	void EnsurePractice();
	void DespawnPractice();
	void TickPractice(float DeltaSeconds);
	void TickRodolfo(float DeltaSeconds);
	void TickBerta(float DeltaSeconds);
	AActor* SpawnItemPickup(FName RowName, const FVector& WorldFeet);
	ATortugaCharacter* SpawnPracticeTurtle(const FVector& LocalFeet, float LocalYaw, const TCHAR* Label);
	FParticipant* FindParticipant(const APlayerController* PC);
	int32 FreeSlot() const;

	/** Coloca a la tortuga en un sitio del recorrido (sin llevar ni ser llevada, fuera del caparazón y sin caída que la rompa). */
	void PlacePawn(APawn* Pawn, const FVector& WorldFeet, float WorldYaw, APlayerController* PC);
};
