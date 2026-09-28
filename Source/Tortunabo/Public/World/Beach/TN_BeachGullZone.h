#pragma once

#include "CoreMinimal.h"
#include "World/Beach/TN_BeachEnemy.h"
#include "World/ProcMap/TN_ProcMapAmbientFX.h"
#include "TN_BeachGullZone.generated.h"

class UStaticMeshComponent;

/** Ataque en curso de una zona de gaviotas (replicado; cada máquina anima el pájaro con el reloj del servidor). */
USTRUCT()
struct FTNBeachGullAttack
{
	GENERATED_BODY()

	/** Tortuga a la que va (en el picado, la que se lleva si la coge). */
	UPROPERTY()
	TObjectPtr<ATortugaCharacter> Victim = nullptr;

	/** Dónde cae la cagada o dónde da el picado (en el suelo; fijado al final del aviso). */
	UPROPERTY()
	FVector_NetQuantize Aim = FVector_NetQuantize(0.0, 0.0, 0.0);

	/** Centro de la tortuga al cogerla: de ahí sale su camino colgada del pico (el mismo en todas las máquinas). */
	UPROPERTY()
	FVector_NetQuantize Hold = FVector_NetQuantize(0.0, 0.0, 0.0);

	/** Reloj del servidor al empezar. */
	UPROPERTY()
	float StartTime = 0.f;

	/** 0 nada, 1 cagada, 2 picado. */
	UPROPERTY()
	uint8 Kind = 0;

	/** Qué pájaro de la zona. */
	UPROPERTY()
	uint8 Bird = 0;

	/** 0 en curso, 1 acierto (mancha o la coge), 2 fallo (esquivado, a cubierto o nadie debajo). */
	UPROPERTY()
	uint8 Result = 0;

	/** 1 cuando Aim ya está fijado. */
	UPROPERTY()
	uint8 bLocked = 0;

	/** Sube con cada ataque nuevo. */
	UPROPERTY()
	uint8 Serial = 0;
};

/**
 * Zona de gaviotas y pelícanos (ETNBeachElement::GullZone): las aves de la fauna a escala (gaviotas de 25 m de
 * envergadura y, a veces, un pelícano de 40 m) dando vueltas sobre la zona, cada una en su círculo (centro desplazado,
 * radio, forma y sentido propios) y a su altura (capas separadas 8 m), con sus sombras en la arena.
 *
 * Cuando hay tortugas debajo, cada 3-6 s la más cercana baja a por una de ellas:
 *  - Cagada: vuela sobre ella y la suelta desde 30 m; cae un pegote blanco bien visible con su estela y su sombra que se
 *    encoge. Quien esté dentro al caer (y no a cubierto) cae derribada con ragdoll y mareo (TNBeach::KnockDownTurtle),
 *    con la mancha en el caparazón; en la arena queda la mancha un rato. «¡PLOF!».
 *  - Picado: sube, se lanza en picado (su sombra crece y se acerca), abre el pico en el último momento y, si la tortuga
 *    sigue debajo (se esquiva apartándose o con el panzazo), la coge por el caparazón: la tortuga queda colgando del pico
 *    pataleando (pose de pataleta, sin caparazón en bola), y el pájaro tira de ella, sube aleteando fuerte, vuela un poco
 *    hacia la salida y la suelta abriendo el pico: cae en bola aturdida (TNBeach::StunTurtle).
 *
 * Red: el servidor decide a quién, cuándo y si acierta; replica un único ataque (FTNBeachGullAttack) y los efectos
 * puntuales van por multicast. Mientras la lleva, cada máquina coloca a la tortuga en el pico con el mismo camino (del
 * reloj del servidor y de Attack.Hold), con su movimiento apagado; el servidor no corrige al dueño mientras tanto.
 */
UCLASS()
class TORTUNABO_API ATN_BeachGullZone : public ATN_BeachEnemy
{
	GENERATED_BODY()

public:
	ATN_BeachGullZone();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** Servidor (pruebas): ataca ya a la tortuga más cercana (1 cagada, 2 picado, 0 al azar). */
	void DebugAttackNow(int32 InKind);

protected:
	virtual void ApplySpec() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void ServerTick(float DeltaSeconds) override;
	virtual void VisualTick(float DeltaSeconds) override;
	virtual float GetVisualRange() const override { return 40000.f; }

	/** Todas las máquinas: la cagada ha caído en Where y ha derribado a Hit (con la mancha en su caparazón). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSplat(FVector_NetQuantize Where, const TArray<ATortugaCharacter*>& Hit);

private:
	UPROPERTY(ReplicatedUsing = OnRep_Attack)
	FTNBeachGullAttack Attack;

	UFUNCTION()
	void OnRep_Attack();

	/** Un pájaro de la zona (vuelo en todas las máquinas; sus piezas, solo con pantalla, en BirdParts desde FirstPart). */
	struct FBird
	{
		bool bPelican = false;
		/** Su círculo: centro desplazado del de la zona, radio, achatamiento, giro del óvalo, altura y velocidad angular. */
		FVector2D CenterOffset = FVector2D::ZeroVector;
		float Radius = 3500.f;
		float Ratio = 1.f;
		float OvalYaw = 0.f;
		float Height = 6000.f;
		float Phase = 0.f;
		float AngSpeed = 0.2f;
		float DriftPhase = 0.f;
		float Scale = 28.f;
		float Span = 2500.f;
		int32 FirstPart = 0;
		int32 NumParts = 0;
		int32 WingL = INDEX_NONE;
		int32 WingR = INDEX_NONE;
		int32 LegL = INDEX_NONE;
		int32 LegR = INDEX_NONE;
		int32 Head = INDEX_NONE;
		FVector Pos = FVector::ZeroVector;
		FVector Vel = FVector::ZeroVector;
		float Yaw = 0.f;
		float Pitch = 0.f;
		float Bank = 0.f;
		float ShadowZ = 0.f;
		float ShadowTimer = 0.f;
		float SquawkTimer = 3.f;
		/** Pico: abierto (grados) y tiempo que le queda abierto por un graznido. */
		float Jaw = 0.f;
		float JawOpenLeft = 0.f;
	};

	TArray<FBird> Birds;

	/** Raíz de cada pájaro (absoluta: se coloca en el mundo). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> BirdRoots;

	/** Piezas de todos los pájaros (cuerpo, cabeza, alas, patas). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> BirdParts;

	/** Mandíbula de abajo de cada pájaro (enganchada a su cabeza). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Jaws;

	/** Pivote de reposo de cada pieza (espacio del cuerpo o de la raíz, medidas de la fauna). */
	TArray<FVector> PartPivots;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Shadows;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Dropping;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> DropShadow;

	/** Manchas en la arena y pegotes en los caparazones, con su hora de nacer y su vida. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Splats;

	TArray<float> SplatBorn;
	TArray<float> SplatLife;
	TArray<float> SplatScale;

	float SizeK = 1.f;
	float AttackRadius = 3800.f;
	float CircleRadius = 3800.f;

	/** Hacia la salida de la carrera (al revés del mar del generador; sin él, -X): hacia donde se la lleva. */
	FVector CourseBack = FVector(-1.0, 0.0, 0.0);

	// Servidor.
	double NextAttackTime = 0.0;
	bool bReleased = false;

	// Tortuga colgando del pico en esta máquina (movimiento apagado mientras tanto).
	TWeakObjectPtr<ATortugaCharacter> HeldLocal;
	uint8 SavedSmoothing = 0;
	bool bSmoothingSaved = false;

	// Visual.
	float Clock = 0.f;
	float DropTrailTimer = 0.f;
	uint8 SeenSerial = 0;
	uint8 SeenResult = 0;
	bool bSwoopPlayed = false;
	bool bWhistlePlayed = false;
	bool bReleasePlayed = false;
	TNAmbientFX::FEmitter Droplets;
	TNAmbientFX::FEmitter Feathers;
	TNAmbientFX::FEmitter Trail;
	TNAmbientFX::FEmitter SandPuff;

	void BuildBirds();
	/** Todas las máquinas: posición de la raíz del pájaro en su vuelta (sin ataque) en el instante Now. */
	FVector CirclePos(const FBird& Bird, double Now) const;
	/** Todas las máquinas: posición de la raíz del pájaro que ataca fuera del agarre (subida, picado, fallo y vuelta). */
	FVector AttackPos(const FBird& Bird, double Now, float Tau) const;
	/** Arena bajo Where (la del generador, sin trazas; sin él, traza; si no hay nada, la altura del actor). */
	float GroundAt(const FVector& Where) const;

	// ── Agarre (mismas cuentas en todas las máquinas) ──

	/** Punto del pico que sujeta, desde la raíz del pájaro (sin escalar), con la cabeza girada HeadPitch. */
	FVector GripOffset(const FBird& Bird, float HeadPitch) const;
	/** Cuánto hay del centro de la tortuga al punto del caparazón por el que la sujeta (cm). */
	float GripDropFor(const ATortugaCharacter* Turtle) const;
	/** Punto del caparazón que va en el pico durante el agarre (U: segundos desde que la coge). */
	FVector GripPath(float U) const;
	/** Giro del pájaro que la lleva (U: segundos desde que la coge) y cabeceo de su cabeza. */
	FRotator CarryRotation(float U) const;
	float CarryHeadPitch(float U) const;
	/** Raíz del pájaro al final del picado: con el pico en el punto del caparazón de la tortuga. */
	FVector StrikeRoot(const FBird& Bird, double Now) const;
	/** Raíz de un pájaro con el pico (girado Rot, cabeza HeadPitch) en Grip. */
	FVector RootForGrip(const FBird& Bird, const FVector& Grip, const FRotator& Rot, float HeadPitch) const;
	/** Hacia dónde mira la tortuga colgada (la misma dirección que el pájaro: hacia la salida). */
	float HeldYaw() const;

	/** Servidor: empieza un ataque (Kind 1 cagada, 2 picado) contra Victim con el pájaro BirdIndex. */
	void StartAttack(ATortugaCharacter* Victim, uint8 InKind, int32 BirdIndex);
	void ServerPoop(float Tau);
	void ServerDive(float Tau);
	void EndAttack(double Now);
	/** Servidor: el pájaro más cercano a Where (el pelícano no caga). */
	int32 PickBird(const FVector& Where, bool bForPoop) const;

	/** Todas las máquinas: coloca a la tortuga en el pico o la suelta, según el ataque replicado. */
	void TickHold();
	void BeginHoldLocal(ATortugaCharacter* Turtle);
	void EndHoldLocal();

	/** Todas las máquinas: ha cambiado el ataque replicado (graznidos, plumas, arena). */
	void OnAttackChanged();
	void PoseBird(int32 Index, float DeltaSeconds, bool bAttacking, float Tau);
	/** Mancha en la arena (InTurtle nulo) o pegote en el caparazón de InTurtle. */
	void SpawnSplat(const FVector& Where, ATortugaCharacter* InTurtle, float InScale, float Life);
};
