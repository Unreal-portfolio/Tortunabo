#pragma once

#include "CoreMinimal.h"
#include "World/Beach/TN_BeachEnemy.h"
#include "World/ProcMap/TN_ProcMapAmbientFX.h"
#include "TN_RacePelicanTaxi.generated.h"

class ATortugaCharacter;
class USceneComponent;
class UStaticMeshComponent;
class UTN_RaceItemSynthComponent;

/**
 * Plan de vuelo del pelícano taxi (replicado una sola vez al crearlo y al acabar). El servidor lo decide al usar el objeto y
 * cada máquina saca de él, con el reloj del servidor, dónde va el pelícano y dónde cuelga la tortuga: nada de posiciones por
 * fotograma en la red.
 */
USTRUCT()
struct FTNPelicanTaxiPlan
{
	GENERATED_BODY()

	/** La tortuga que lleva (la que ha usado el objeto). */
	UPROPERTY()
	TObjectPtr<ATortugaCharacter> Victim = nullptr;

	/** Centro de su cápsula al cogerla. */
	UPROPERTY()
	FVector_NetQuantize10 Start = FVector_NetQuantize10(0.0, 0.0, 0.0);

	/** Centro de su cápsula de pie en el sitio donde la deja (arena abierta; la suelta ReleaseHeight más arriba). */
	UPROPERTY()
	FVector_NetQuantize10 End = FVector_NetQuantize10(0.0, 0.0, 0.0);

	/** Altura del centro de su cápsula en el crucero (cm, mundo). */
	UPROPERTY()
	float CruiseZ = 0.f;

	/** Giro de la carrera (hacia el mar, grados): el rumbo de reserva si la salida y el sitio coinciden en planta. */
	UPROPERTY()
	float CourseYaw = 0.f;

	/** Reloj del servidor al usar el objeto: todo el vuelo se cuenta desde aquí. */
	UPROPERTY()
	float StartTime = 0.f;

	/** Reloj del servidor en que la ha soltado (en el sitio) o la ha perdido (abortado); 0 mientras la lleva. */
	UPROPERTY()
	float EndTime = 0.f;

	/** 0 sin plan, 1 la lleva, 2 la ha dejado en el sitio, 3 abortado (se la han quitado o ya no está). */
	UPROPERTY()
	uint8 Phase = 0;

	/** Sube con cada cambio del plan. */
	UPROPERTY()
	uint8 Serial = 0;
};

/**
 * Pelícano taxi (objeto de carrera ETNRaceItem::PelicanTaxi, la «bala» de las carreras de karts). Al usarlo, un pelícano
 * gigante (el de la fauna a escala 24, 40 m de envergadura) baja en picado desde atrás, coge a la tortuga por el caparazón
 * con el pico y la lleva volando por delante de todas, hacia el mar, unos 120 m; luego baja y la suelta de pie en arena
 * abierta (nunca en el agua, en una poza, en una trinchera ni en un obstáculo, y a más de 55 m del filo: no se salta la
 * meta) y se marcha. Mientras vuela es invulnerable (UTN_RaceItemComponent::SetRiding) y no puede usar objetos.
 *
 * Deriva de ATN_BeachEnemy para reutilizar la sujeción de tortugas ya probada (BeginHoldTurtle, PlaceHeldTurtle,
 * EndHoldTurtle, las marcas de llevada y ServerReleaseHeldTurtle, que avisa con OnHoldAborted si la red de seguridad, un
 * rescate o un gusano se la quitan). No es un enemigo de verdad: no se marea, no se le puede dar con nada y no usa Mover.
 *
 * Red: el servidor decide el plan una vez (sitio de aterrizaje con TNBeach::FindOpenSandSpot, altura de crucero por encima
 * de todo el camino) y lo replica en Plan. Línea de tiempo desde Plan.StartTime: aproximación (la tortuga ya sujeta, quieta
 * y pataleando), subida, crucero, descenso, suelta y despedida. El vuelo del pelícano y el punto de la espalda de la tortuga
 * son fórmulas del tiempo iguales en todas las máquinas; cada una sujeta y coloca a la tortuga por su cuenta y la suelta al
 * llegar la hora o al recibir el cambio de fase. Lo visual (malla, sombra, partículas y sonido), solo con pantalla.
 */
UCLASS()
class TORTUNABO_API ATN_RacePelicanTaxi : public ATN_BeachEnemy
{
	GENERATED_BODY()

public:
	ATN_RacePelicanTaxi();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Servidor: llama al pelícano para Turtle (ya validado que puede usar el objeto). Decide el plan y crea el taxi; true si
	 * lo ha creado (entonces el uso gasta el objeto). false sin crear nada si ya hay un taxi con ella, si hay demasiados en
	 * el mundo o si no hay sitio por delante donde dejarla.
	 */
	static bool ServerLaunch(ATortugaCharacter* Turtle);

	/** No se marea con nada (ni con lo lanzado ni con el silbato). */
	virtual bool AcceptsHitStun() const override { return false; }

	/** No tiene cuerpo al que dar. */
	virtual bool GetHitCapsule(FVector& OutA, FVector& OutB, float& OutRadius) const override { return false; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void ServerTick(float DeltaSeconds) override;
	virtual void VisualTick(float DeltaSeconds) override;
	virtual void OnHoldAborted(ATortugaCharacter* Turtle) override;
	virtual float GetVisualRange() const override;

	/** Seguro de tiempo de la sujeción: más largo que el vuelo más largo (una sola sujeción de principio a fin). */
	virtual double GetMaxHoldSeconds() const override;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Plan)
	FTNPelicanTaxiPlan Plan;

	UFUNCTION()
	void OnRep_Plan();

	/** Tiempos y medidas del vuelo que salen del plan (se recalculan en cada máquina; los mismos en todas). */
	struct FFlightTimes
	{
		/** Rumbo del vuelo (plano, unitario: de la salida al sitio) y su giro. */
		FVector Dir = FVector::ForwardVector;
		float Yaw = 0.f;
		/** Distancia en planta de la salida al sitio (cm) y velocidad de crucero (cm/s). */
		float Length = 0.f;
		float Speed = 2200.f;
		/** Segundos desde Plan.StartTime en que empieza a subir, el crucero, el descenso y la suelta. */
		float ClimbStart = 1.f;
		float CruiseStart = 2.3f;
		float DescentStart = 2.3f;
		float Release = 3.8f;
		float CruiseSeconds = 0.f;
		/** Del centro de la cápsula a la espalda que va en el pico (cm). */
		float GripDrop = 66.f;
		/** Espalda de la tortuga al cogerla, su altura en el crucero y al soltarla. */
		FVector Spine0 = FVector::ZeroVector;
		float CruiseSpineZ = 0.f;
		FVector SpineEnd = FVector::ZeroVector;
	};

	FFlightTimes Flight;

	/** Recalcula Flight a partir de Plan. */
	void RefreshFlight();

	/** Segundos desde Plan.StartTime con el reloj del servidor. */
	float Elapsed() const;

	/** Segundos desde Plan.StartTime en que deja de llevarla (la suelta o la pierde) y empieza la despedida. */
	float FarewellStart() const;

	// ── Vuelo (mismas cuentas en todas las máquinas) ──

	/** Punto de la espalda de la tortuga (el que coloca PlaceHeldTurtle) en el instante T. */
	FVector SpineAt(float T) const;

	/** Punto del pico que la sujeta: detrás de su espalda, en el caparazón. */
	FVector BeakAt(float T) const;

	/** Cabeceo del camino de la tortuga en T (grados, recortado). */
	float PathPitchAt(float T) const;

	/** Velocidad en planta en T respecto a la de crucero (0-1): para la estela. */
	float AlongSpeedFactor(float T) const;

	/** Giro del pelícano y cabeceo de su cabeza mientras la lleva. */
	FRotator CarryRotation(float T) const;
	float CarryHeadPitch(float T) const;

	/** Punto del pico que sujeta, desde la raíz del pelícano (medidas de la fauna, sin escalar), con la cabeza en HeadPitch. */
	FVector GripOffset(float HeadPitch) const;

	/** Raíz del pelícano con el pico (girado Rot, cabeza HeadPitch) en Grip. */
	FVector RootForGrip(const FVector& Grip, const FRotator& Rot, float HeadPitch) const;

	/** Centro del cuerpo del pelícano con la raíz en Root y girado Rot. */
	FVector BodyCenter(const FVector& Root, const FRotator& Rot) const;

	/** Aproximación y agarre (hasta FarewellStart): raíz, giro y cabeza, sin suavizar. */
	void HeldPose(float T, FVector& OutRoot, FRotator& OutRot, float& OutHeadPitch) const;

	/** Todo el vuelo, con la despedida: raíz, giro y cabeza del pelícano en T, sin suavizar. */
	void PoseAt(float T, FVector& OutRoot, FRotator& OutRot, float& OutHeadPitch) const;

	/** Suelo bajo Where (el del generador o una traza). */
	float GroundBelow(const FVector& Where) const;

	// ── Sujeción ──

	/** Todas las máquinas: sujeta a la tortuga, la coloca en el pico o la suelta, según el plan y la hora. */
	void SyncHold();

	/** Pone la cápsula de la tortuga de pie ReleaseHeight por encima del sitio (justo antes de soltarla). */
	void PutVictimOverSite(ATortugaCharacter* Victim) const;

	/** Servidor: la deja en el sitio (suelta, sin caída larga, gracia de la tormenta y fin del vuelo). */
	void ServerDropOff();

	/** Servidor: deja de llevarla sin soltarla en ningún sitio y se marcha. */
	void ServerAbort(const TCHAR* Reason);

	// ── Visual (solo con pantalla) ──

	void BuildVisuals();

	/** Alas, patas, cabeza y pico; devuelve si está aleteando (para el sonido). */
	bool PosePelican(float T, float DeltaSeconds, float HeadPitch, bool bNear);

	/** Graznidos, aleteos, arena, plumas y el aterrizaje de la tortuga. */
	void TickEvents(float T, float DeltaSeconds, const FVector& Body, bool bNear, bool bFlapping);

	/** Raíz del pelícano (absoluta: se coloca en el mundo). */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> PelicanRoot;

	/** Piezas del pelícano (cuerpo, cabeza, alas, patas...). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PelicanParts;

	/** Mandíbula de abajo (enganchada a la cabeza). */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> JawPart;

	/** Sombra en la arena. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ShadowPart;

	UPROPERTY(Transient)
	TObjectPtr<UTN_RaceItemSynthComponent> Sfx;

	/** Pivote de reposo de cada pieza (medidas de la fauna). */
	TArray<FVector> PartPivots;

	int32 WingLIndex = INDEX_NONE;
	int32 WingRIndex = INDEX_NONE;
	int32 LegLIndex = INDEX_NONE;
	int32 LegRIndex = INDEX_NONE;
	int32 HeadIndex = INDEX_NONE;

	TNAmbientFX::FEmitter WindTrail;
	TNAmbientFX::FEmitter SandPuff;
	TNAmbientFX::FEmitter Feathers;

	/** Pose del fotograma (Tick, antes de la base): tiempo, raíz, giro y cabeza sin suavizar. */
	float PoseTime = 0.f;
	FVector PoseRoot = FVector::ZeroVector;
	FRotator PoseRot = FRotator::ZeroRotator;
	float PoseHead = 0.f;

	/** Giro que se ve (suavizado). */
	FRotator ShownRot = FRotator::ZeroRotator;
	bool bShownRotValid = false;

	float Clock = 0.f;
	float JawAngle = 0.f;
	float ShadowZ = 0.f;
	float ShadowTimer = 0.f;
	/** Malla de la sombra según lo nítida que va (-1 = aún la de serie). */
	int32 ShadowEdge = -1;
	float FlapSoundTimer = 0.f;
	bool bVisualsBuilt = false;
	bool bEventsPrimed = false;
	bool bGrabPlayed = false;
	bool bReleasePlayed = false;
	bool bLandPlayed = false;

	/** Esta máquina ya la ha soltado del todo (en el sitio o abortado): no la vuelve a sujetar. */
	bool bLocalHoldDone = false;

	/** Servidor: ya se ha pedido su final. */
	bool bServerEnding = false;
};
