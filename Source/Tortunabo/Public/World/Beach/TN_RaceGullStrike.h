#pragma once

#include "CoreMinimal.h"
#include "World/Beach/TN_RaceItemActor.h"
#include "World/ProcMap/TN_ProcMapAmbientFX.h"
#include "TN_RaceGullStrike.generated.h"

class ATortugaCharacter;
class USceneComponent;
class UStaticMeshComponent;

/**
 * Gaviota justiciera (Docs/Modo_Carrera.md, «Objetos de carrera»): la «concha azul» de la playa. Una gaviota gigante (la misma
 * de las zonas de gaviotas: 25 m de envergadura) va a por la tortuga que lidera la carrera y le suelta una cagada.
 *
 *  - Objetivo (al lanzarla, ServerLaunch): la tortuga que va la primera de todas, solo si va por delante de quien la lanza
 *    (TNRaceItems::FindTurtleTarget); si no hay (quien la lanza va la primera o sola), el enemigo más cercano por delante
 *    (a menos de 120 m), que se marea 4 s; si no hay nada, ServerLaunch devuelve false. Como mucho hay 3 a la vez.
 *  - Línea de tiempo (segundos desde que nace, del reloj del servidor): 0–3,2 la gaviota nace a 90 m por detrás del objetivo y
 *    a 70 m de altura, y vuela hasta situarse sobre él a 30 m, con un graznido al nacer; a los 3,2 s suelta la cagada, que
 *    cae 1,7 s (silbido de caída y, si es una tortuga, un pitido de aviso en ella); al llegar abajo, el impacto (radio 3,3 m en
 *    planta, menos de 3 m de altura): derriba con ragdoll (TNBeach::KnockDownTurtle) a quien esté dentro y se pueda golpear y
 *    le deja un pegote en el caparazón unos 8 s. Después sube y se va (hasta los 9 s).
 *  - El punto de impacto (AimPoint) sigue al objetivo por la arena a 7 m/s como mucho (corriendo a 8 m/s se libra; andando no)
 *    y se congela al terminar la caída. Sobre la arena, una sombra dura y negra que nace pequeña al soltar y crece hasta el
 *    radio del impacto según cae la cagada.
 *
 * Red: el servidor decide a quién, dónde y a quién da. Replica el objetivo (Target) y el punto de impacto (AimPoint, que
 * los clientes suavizan); la gaviota no usa Track: su sitio sale de una fórmula del reloj del servidor y del sitio del
 * objetivo, igual en todas las máquinas. El impacto va por un multicast no fiable (sonido, mancha, pegote), con un seguro
 * local por si no llega.
 */
UCLASS(NotBlueprintable)
class TORTUNABO_API ATN_RaceGullStrike : public ATN_RaceItemActor
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Servidor: lanza una gaviota justiciera desde Turtle. false (sin crear nada) si no hay a quién mandarla o ya hay
	 * demasiadas; true si la ha creado (el uso gasta el objeto).
	 */
	static bool ServerLaunch(ATortugaCharacter* Turtle);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void ServerTick(float DeltaSeconds) override;
	virtual void BuildVisuals() override;
	virtual void VisualTick(float DeltaSeconds) override;
	virtual void OnFinished() override;
	virtual bool UsesTrack() const override { return false; }

	/** Todas las máquinas: la cagada ha caído en Where. Sonido, mancha en la arena y, si hay víctima, pegote en su caparazón. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSplat(FVector_NetQuantize10 Where, ATortugaCharacter* Hit);

	/** Todas las máquinas: otra tortuga cogida por el salpicón; solo el pegote en su caparazón. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastStain(ATortugaCharacter* Victim);

private:
	// ── Replicado ────────────────────────────────────────────────────────────

	/** A quién va: la tortuga que lidera la carrera o, si no hay, un enemigo. */
	UPROPERTY(Replicated)
	TObjectPtr<AActor> Target = nullptr;

	/** Dónde caerá la cagada (en la arena): sigue al objetivo a 7 m/s como mucho y se congela al acabar la caída. */
	UPROPERTY(Replicated)
	FVector_NetQuantize10 AimPoint = FVector_NetQuantize10(0.0, 0.0, 0.0);

	// ── Servidor ─────────────────────────────────────────────────────────────

	/** El punto de impacto sigue al objetivo por la arena hasta AimSpeed. */
	void ServerTrackAim(float DeltaSeconds);

	/** Suelta la cagada: pitido de aviso en la tortuga a la que va. */
	void ServerWarnVictim();

	/** Cae la cagada: derriba a quien esté dentro, marea al enemigo y avisa a todas las máquinas. */
	void ServerImpact();

	/** El punto de impacto en doble precisión (lo que se replica es AimPoint). */
	FVector AimServer = FVector::ZeroVector;
	float RacerClock = 0.f;
	bool bTargetLost = false;
	bool bDropped = false;
	bool bImpactDone = false;

	// ── Todas las máquinas ───────────────────────────────────────────────────

	/** Dónde está la gaviota en el instante AgeSeconds (fórmula del tiempo y del sitio del objetivo). */
	FVector FlightPosition(double AgeSeconds) const;

	/** Normal de la arena en Where (con dos muestras más del suelo). */
	FVector GroundNormalAt(const FVector& Where) const;

	/** El blanco que se ve en esta máquina: en el servidor, AimPoint; en los clientes, el replicado suavizado. */
	void UpdateShownAim(float DeltaSeconds, double AgeSeconds);

	// ── Visual (máquinas con pantalla) ───────────────────────────────────────

	/** Pone las piezas de la gaviota según el momento de la línea de tiempo. */
	void PoseGull(float DeltaSeconds, double AgeSeconds);
	void PoseGullPart(int32 PartIndex, const FRotator& Rotation);

	/** El impacto que se ve: sonido, gotas, mancha en la arena y pegote en el caparazón de Hit. Una sola vez. */
	void ShowSplat(const FVector& Where, ATortugaCharacter* Hit);

	void SpawnSandSplat(const FVector& Where);
	void SpawnShellSplat(ATortugaCharacter* Victim);

	/** Sonido en el suelo (sitio fijo, no sigue a la gaviota). */
	UTN_RaceItemSynthComponent* GetGroundSfx(const FVector& Where);

	/** Manchas: las de la arena y las de los caparazones se encogen y se van a su hora. */
	void TickMarks(float DeltaSeconds);

	struct FMark
	{
		TWeakObjectPtr<UStaticMeshComponent> Comp;
		float Born = 0.f;
		float Life = 8.f;
		float BaseScale = 1.f;
		/** true: plana en la arena (encoge a lo ancho); false: en un caparazón (encoge entera). */
		bool bFlat = false;
	};

	TArray<FMark> Marks;
	float MarkClock = 0.f;

	/** Raíz de la gaviota (absoluta: se coloca en el mundo) y sus piezas. */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> GullRoot;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> GullParts;

	/** Pivote de reposo de cada pieza (espacio del cuerpo o de la raíz, medidas de la fauna). */
	TArray<FVector> PartPivots;

	int32 WingLeftPart = INDEX_NONE;
	int32 WingRightPart = INDEX_NONE;
	int32 LegLeftPart = INDEX_NONE;
	int32 LegRightPart = INDEX_NONE;
	int32 HeadPart = INDEX_NONE;

	/** Mandíbula de abajo, enganchada a la cabeza. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Jaw;

	/** Sombra de la gaviota en la arena. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> GullShadow;

	/** La cagada que cae. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Dropping;

	/** Aviso en la arena: sombra dura y negra donde va a caer (nace pequeña y crece hasta el radio del impacto). */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> DropShadow;

	/** Sonido del impacto, fijo en el suelo. */
	UPROPERTY(Transient)
	TObjectPtr<UTN_RaceItemSynthComponent> GroundSfx;

	TNAmbientFX::FEmitter Droplets;
	TNAmbientFX::FEmitter Trail;

	/** Hacia dónde avanza la carrera (el mar del generador; sin él, +X): igual en todas las máquinas. */
	FVector CourseForward = FVector::ForwardVector;

	/** Dónde ve esta máquina al objetivo (el ancla del vuelo). */
	FVector LastAnchor = FVector::ZeroVector;
	bool bAnchorValid = false;

	/** Blanco que se ve. */
	FVector ShownAim = FVector::ZeroVector;
	bool bShownAimValid = false;
	bool bAimLocked = false;

	/** Estado del vuelo que se ve: sitio, velocidad suavizada y giros. */
	FVector GullPos = FVector::ZeroVector;
	FVector GullVel = FVector::ZeroVector;
	float GullYaw = 0.f;
	float GullPitch = 0.f;
	float GullBank = 0.f;
	bool bPoseValid = false;

	float AnimClock = 0.f;
	float JawOpenLeft = 0.f;
	float JawAngle = 0.f;
	float ShadowZ = 0.f;
	float ShadowTimer = 0.f;
	float TrailTimer = 0.f;
	float DropNormalTimer = 0.f;
	FVector DropNormal = FVector::UpVector;

	/** Dónde soltó la cagada (bajo la cola de la gaviota). */
	FVector DropStart = FVector::ZeroVector;
	bool bDropStartValid = false;

	bool bBirthSoundPlayed = false;
	bool bReleaseShown = false;
	bool bSplatShown = false;
};
