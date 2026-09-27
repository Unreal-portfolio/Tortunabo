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

	/** Dónde cae la cagada o dónde da el picado (fijado al final del aviso). */
	UPROPERTY()
	FVector_NetQuantize Aim = FVector_NetQuantize(0.0, 0.0, 0.0);

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
 * Zona de gaviotas y pelícanos (ETNBeachElement::GullZone): las gaviotas de siempre (ATN_EnemySeagull, la caca de
 * ATN_SeagullDroppingActor) con aspecto nuevo: las aves de la fauna a escala (gaviotas de 25 m de envergadura y un
 * pelícano de 40 m) dando vueltas a 55-80 m de altura sobre la zona, con sus sombras en la arena.
 *
 * Cuando hay tortugas debajo, cada 3,5-7 s una baja a por una de ellas:
 *  - Cagada: vuela sobre ella y suelta la cagada; en la arena, una sombra que se encoge marca dónde cae (como la caca de
 *    siempre). Quien esté dentro al caer (y no a cubierto) queda aturdido y con un pegote blanco; en la arena queda la
 *    mancha un rato.
 *  - Picado como un halcón: sube, se lanza y su sombra crece y se acerca rápido. Se esquiva apartándose o con el panzazo
 *    en el último momento. Si la sombra la alcanza, la coge con el pico, la sube volando hacia la salida y la suelta desde
 *    arriba: cae en bola aturdida unos segundos (en carrera no se muere).
 *
 * El servidor decide a quién, cuándo y si acierta; replica un único ataque (FTNBeachGullAttack) y los efectos puntuales
 * van por multicast no fiable. Mientras la lleva, el servidor guía la caja física del caparazón (ATN_ShellBody).
 */
UCLASS()
class TORTUNABO_API ATN_BeachGullZone : public ATN_BeachEnemy
{
	GENERATED_BODY()

public:
	ATN_BeachGullZone();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Servidor (pruebas): ataca ya a la tortuga más cercana (1 cagada, 2 picado, 0 al azar). */
	void DebugAttackNow(int32 InKind);

protected:
	virtual void ApplySpec() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void ServerTick(float DeltaSeconds) override;
	virtual void VisualTick(float DeltaSeconds) override;
	virtual float GetVisualRange() const override { return 40000.f; }

	/** Todas las máquinas: la cagada ha caído en Where y ha manchado a Hit. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSplat(FVector_NetQuantize Where, const TArray<ATortugaCharacter*>& Hit);

private:
	UPROPERTY(ReplicatedUsing = OnRep_Attack)
	FTNBeachGullAttack Attack;

	UFUNCTION()
	void OnRep_Attack();

	/** Un pájaro de la zona (solo visual; sus piezas van en BirdParts desde FirstPart). */
	struct FBird
	{
		bool bPelican = false;
		float Radius = 3500.f;
		float Height = 6000.f;
		float Phase = 0.f;
		float AngSpeed = 0.2f;
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
	};

	TArray<FBird> Birds;

	/** Raíz de cada pájaro (absoluta: se coloca en el mundo). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> BirdRoots;

	/** Piezas de todos los pájaros (cuerpo, cabeza, alas, patas). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> BirdParts;

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
	FVector CarryStart = FVector::ZeroVector;
	bool bReleased = false;

	// Visual.
	float Clock = 0.f;
	uint8 SeenSerial = 0;
	uint8 SeenResult = 0;
	bool bSwoopPlayed = false;
	FVector ReleasePos = FVector::ZeroVector;
	bool bHasReleasePos = false;
	TNAmbientFX::FEmitter Droplets;
	TNAmbientFX::FEmitter Feathers;

	void BuildBirds();
	/** Todas las máquinas: posición del pájaro en su vuelta (sin ataque) en el instante Now. */
	FVector CirclePos(const FBird& Bird, double Now) const;
	/** Todas las máquinas: posición del pájaro que ataca (según el ataque replicado). */
	FVector AttackPos(const FBird& Bird, double Now);
	/** Suelo bajo la zona cerca de Where (traza; si no hay, la altura del actor). */
	float GroundAt(const FVector& Where) const;
	/** Servidor: empieza un ataque (Kind 1 cagada, 2 picado) contra Victim con el pájaro BirdIndex. */
	void StartAttack(ATortugaCharacter* Victim, uint8 InKind, int32 BirdIndex);
	void ServerPoop(float Tau);
	void ServerDive(float Tau, float DeltaSeconds);
	void EndAttack(double Now);
	/** Servidor: guía la caja física del caparazón de la tortuga que lleva hacia Target. */
	void DriveCarried(const FVector& Target, const FVector& TargetVel);
	/** Servidor: suelta la caja del caparazón (se deja caer con un empujoncito hacia la salida). */
	void ReleaseCarried();
	/** Todas las máquinas: ha cambiado el ataque replicado (graznidos, plumas). */
	void OnAttackChanged();
	void PoseBird(int32 Index, float DeltaSeconds, bool bAttacking, float Tau);
	/** Mancha en la arena (InParent nulo) o pegote en un caparazón (enganchado a InParent). */
	void SpawnSplat(const FVector& Where, USceneComponent* InParent, float InScale, float Life);
};
