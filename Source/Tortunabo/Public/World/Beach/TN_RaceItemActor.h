#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/Beach/TN_RaceItemSynth.h"
#include "TN_RaceItemActor.generated.h"

class ATN_BeachRaceGenerator;
class ATortugaCharacter;
class UTN_RaceItemSynthComponent;

/**
 * Dónde está y hacia dónde mira un objeto de carrera que se mueve (el cangrejo, la gaviota, la mina lanzada...), compacto:
 * lo replica el servidor a unos 20 Hz y cada máquina lo suaviza (ATN_RaceItemActor::SmoothToTrack).
 */
USTRUCT()
struct FTNRaceTrack
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize10 Location = FVector_NetQuantize10(0.0, 0.0, 0.0);

	/** Giro alrededor de Z y cabeceo, comprimidos (FRotator::CompressAxisToShort). */
	UPROPERTY()
	uint16 Yaw = 0;

	UPROPERTY()
	uint16 Pitch = 0;
};

/**
 * Base de los actores de los objetos de carrera (Docs/Modo_Carrera.md, «Objetos de carrera»): cangrejo teledirigido,
 * gaviota justiciera, mina de arena, disco volador y nube de tormenta. Reúne lo que tienen en común:
 *
 *  - Servidor con autoridad: ServerTick decide dónde está, a quién da y cuándo acaba. El resto de máquinas solo ven lo
 *    replicado (Track, la hora de inicio y quién lo lanzó) y lo animan; los golpes y aturdimientos los aplica el servidor
 *    (TNBeach::StunTurtle/KnockDownTurtle, ATN_BeachEnemy::ApplyHitStun) y llegan a todos por sus vías de siempre.
 *  - Siempre relevante y sin movimiento de motor replicado: el servidor escribe Track (ServerPublishTrack) y los clientes
 *    lo suavizan (SmoothToTrack) sin saltos.
 *  - Reloj del servidor común (Age = segundos desde que nació): las animaciones que salen de una fórmula del tiempo (la
 *    nube, el disco...) coinciden en todas las máquinas.
 *  - Un sonido sintetizado propio (PlaySfx) y un final ordenado (ServerFinish).
 *
 * Las subclases sobrescriben los ganchos; nada de esto corre en un servidor dedicado salvo ServerTick.
 */
UCLASS(Abstract, NotBlueprintable)
class TORTUNABO_API ATN_RaceItemActor : public AActor
{
	GENERATED_BODY()

public:
	ATN_RaceItemActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Servidor, justo después de crearlo (antes de FinishSpawning): la tortuga que lo lanza. */
	void SetOwnerTurtle(ATortugaCharacter* Turtle);

	ATortugaCharacter* GetOwnerTurtle() const { return OwnerTurtle; }

	/** Segundos desde que nació, con el reloj del servidor (igual en todas las máquinas, con un pequeño retraso en los clientes). */
	double GetAge() const;

	/** Ya ha acabado (se esconde y desaparece enseguida). */
	bool IsFinished() const { return bFinished; }

	/** Cuántos de estos hay ahora en el mundo (para no llenar la playa: cada clase pone su tope). */
	static int32 CountOf(UWorld* World, const UClass* ItemClass);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ── Ganchos ──────────────────────────────────────────────────────────────

	/** Solo servidor, cada fotograma: mueve, busca, golpea y llama a ServerFinish al acabar. */
	virtual void ServerTick(float DeltaSeconds) {}

	/** Máquinas con pantalla, una vez en BeginPlay (mallas, luces, emisores). */
	virtual void BuildVisuals() {}

	/** Máquinas con pantalla, cada fotograma después del movimiento (animación, partículas, sombra). */
	virtual void VisualTick(float DeltaSeconds) {}

	/** Todas las máquinas: ha cambiado bFinished (esconder el modelo, hacer el efecto final). */
	virtual void OnFinished() {}

	/** true si el servidor mueve el actor y lo replica con Track (los clientes lo suavizan); false si cada máquina lo calcula. */
	virtual bool UsesTrack() const { return true; }

	/** Frecuencia de réplica de Track (Hz). */
	virtual float GetTrackFrequency() const { return 20.f; }

	// ── Utilidades ───────────────────────────────────────────────────────────

	/** Servidor: escribe Track con el sitio y el giro actuales (se envía solo si cambia). Tick lo llama tras ServerTick si UsesTrack(). */
	void ServerPublishTrack();

	/** Clientes: acerca el actor al último Track recibido (con una pizca de extrapolación). Se llama solo en Tick. */
	void SmoothToTrack(float DeltaSeconds);

	/** Servidor: el objeto ha acabado; se esconde en todas las máquinas y se destruye a los Delay segundos. */
	void ServerFinish(float Delay);

	/** Reloj del servidor en esta máquina (s). */
	double ServerNow() const;

	/** Sonido sintetizado propio (se crea la primera vez; null en servidor dedicado o sin audio). */
	UTN_RaceItemSynthComponent* GetSfx();

	/** Sonido en el sitio actual del actor (nada si no se puede oír). */
	void PlaySfx(ETNRaceSound Sound, float Pitch = 1.f, float Volume = 1.f);

	/** Cota del suelo bajo Where: el generador de la playa (sin trazas) o, sin él, una traza; Fallback si no hay nada. */
	float GroundHeightAt(const FVector& Where, float Fallback) const;

	/** El generador de la playa (se busca una vez y se guarda); null fuera de la carrera. */
	ATN_BeachRaceGenerator* GetGenerator() const;

	/** Esta máquina dibuja (no es un servidor dedicado). */
	bool bHasScreen = false;

	/** Quién lo lanzó (replicado). */
	UPROPERTY(Replicated)
	TObjectPtr<ATortugaCharacter> OwnerTurtle = nullptr;

	/** Hora del servidor a la que nació (se pone en BeginPlay del servidor). */
	UPROPERTY(Replicated)
	float StartServerTime = 0.f;

	/** Sitio y giro que manda el servidor. */
	UPROPERTY(ReplicatedUsing = OnRep_Track)
	FTNRaceTrack Track;

	UPROPERTY(ReplicatedUsing = OnRep_Finished)
	bool bFinished = false;

private:
	UFUNCTION()
	void OnRep_Track();

	UFUNCTION()
	void OnRep_Finished();

	/** Último Track recibido en esta máquina y velocidad estimada para extrapolar. */
	FVector TrackLoc = FVector::ZeroVector;
	FVector TrackVel = FVector::ZeroVector;
	FQuat TrackRot = FQuat::Identity;
	double LastTrackTime = 0.0;
	bool bHasTrack = false;

	UPROPERTY(Transient)
	TObjectPtr<UTN_RaceItemSynthComponent> Sfx;

	mutable TWeakObjectPtr<ATN_BeachRaceGenerator> CachedGenerator;

	/** Servidor: ronda del generador en que nació (-1 sin playa) y cuenta para mirar si ha cambiado (se acaba con la ronda). */
	int32 SpawnRound = -1;
	float RoundCheckClock = 0.5f;
};
