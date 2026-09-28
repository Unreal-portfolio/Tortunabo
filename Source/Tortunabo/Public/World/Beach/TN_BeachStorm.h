#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/Beach/TN_BeachEnemy.h"
#include "World/Beach/TN_BeachTrapCommon.h"
#include "World/ProcMap/TN_ProcMapAmbientFX.h"
#include "TN_BeachStorm.generated.h"

class UExponentialHeightFogComponent;
class UMaterialInstanceDynamic;
class UPostProcessComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTN_BeachEnemySynthComponent;
class ATN_BeachRaceGenerator;
class ATortugaCharacter;

/**
 * Tormenta de bañistas de la carrera en la playa: la tormenta del camino del cooperativo (ATN_PathStorm) llevada a la
 * playa. No es un elemento del reparto: la crea y la arranca el GameMode de la carrera en cada ronda.
 *
 * Un frente recto a lo ancho de la playa que sale de detrás de la salida y avanza hacia el mar, pegado a la arena (la
 * altura sale del terreno del generador, sin trazas). Son los bañistas que llegan: una cortina de arena y polvo (el velo
 * de ATN_PathStorm, color arena) con piernas gigantes que pisan justo en su borde, y sombrillas, cubos, sillas de playa,
 * toallas, flotadores, palas, chanclas y pelotas volando a escala por el borde de verdad del frente (ni por delante ni
 * por detrás). Nada aparece ni desaparece de golpe: el velo, los trastos y los bañistas se funden (opacidad y escala).
 *
 * Es justa: arranca tarde (15 s de gracia y 6 s cogiendo velocidad) y va bastante más despacio que la media de la carrera
 * (1,8 m/s; la tortuga anda a 4,5 y la media con obstáculos es ~4). Solo acelera al final: pasados 4 minutos, cuando la
 * primera tortuga ha hecho el 80 % del recorrido o si la última se ha quedado muy atrás (para que siempre se note). Antes
 * de alcanzarte avisa (temblor, viento, arena y «¡QUE VIENE LA TORMENTA!»). Nadie se puede quedar detrás del frente: a
 * quien se queda detrás (también si una gaviota la suelta ahí) un bañista le da una patada que la mete en su caparazón y
 * la lanza en bola hasta KickAhead (20 m) por delante del frente. Dentro, la imagen se cierra (niebla y tinte de arena) y
 * la tortuga tose (UTN_StormCoughComponent).
 *
 * Marco: el del propio actor. Su X local es la dirección de la carrera (hacia el mar), su Y local el ancho (centro en
 * Y = 0) y su Z, el suelo de referencia. El frente es la recta X local = GetFrontDistance().
 *
 * Interfaz para el GameMode (servidor):
 *  - Crearla detrás de la salida, en el centro de la playa, girada hacia el mar, y llamar a StartStorm() (sin
 *    parámetros, por nombre: el frente sale del propio actor a DefaultSpeed tras DefaultGrace) o, desde C++, a
 *    StartStormAt(Desplazamiento, Velocidad, Gracia).
 *  - StopStorm() (sin parámetros): la para; se queda quieta y a la vista (recuento). Destruirla la quita.
 *  - IsLocationInside, GetFrontDistance, GetFrontLocation, GetFrontSpeed, IsStormActive, FindStorm.
 *
 * Red: replica solo el tramo de marcha en curso (desplazamiento, velocidad, aceleración, velocidad a la que va, hora del
 * servidor, gracia); cada máquina calcula el frente con el reloj del servidor. Al cambiar de velocidad el servidor
 * empieza un tramo nuevo desde donde está. El servidor decide a quién patea (la bola va por la física replicada del
 * caparazón); la pierna que patea y los avisos van por multicast o son locales.
 * Consola: TN.Beach.Storm.Start [metros por detrás] [cm/s], TN.Beach.Storm.Stop, TN.Beach.Storm.Info y
 * TN.Beach.Storm.Here (TN_BeachEnemyDebug.cpp).
 */
UCLASS()
class TORTUNABO_API ATN_BeachStorm : public AActor
{
	GENERATED_BODY()

public:
	ATN_BeachStorm();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Servidor: arranca desde el propio actor a DefaultSpeed tras DefaultGrace (cogiendo velocidad poco a poco). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Storm")
	void StartStorm();

	/** Servidor: el frente sale InStartOffset cm por delante del actor (negativo: detrás) y va a Speed cm/s tras GraceSeconds. */
	void StartStormAt(float InStartOffset, float Speed, float GraceSeconds = 0.f);

	/** Servidor: para la tormenta donde esté (se queda a la vista). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Storm")
	void StopStorm();

	/**
	 * Servidor (pruebas, TN.Beach.Storm.Here): pone el frente en InFront (cm a lo largo de la carrera desde el actor) sin
	 * cambiar de velocidad (parada, a la normal) y en marcha.
	 */
	void DebugSetFront(float InFront);

	/** Distancia del frente al actor a lo largo de la carrera (cm; cualquier máquina, con el reloj del servidor). */
	UFUNCTION(BlueprintPure, Category = "Storm")
	float GetFrontDistance() const;

	/** Velocidad del frente ahora mismo (cm/s). */
	UFUNCTION(BlueprintPure, Category = "Storm")
	float GetFrontSpeed() const;

	/** Punto del frente en el centro de la playa (a la altura del actor). */
	UFUNCTION(BlueprintPure, Category = "Storm")
	FVector GetFrontLocation() const;

	UFUNCTION(BlueprintPure, Category = "Storm")
	bool IsStormActive() const { return bActive; }

	/** Si una posición está dentro: por detrás del frente más que InsideMargin y dentro del ancho. */
	bool IsLocationInside(const FVector& WorldLocation) const;

	/** La tormenta de este mundo (la primera), o null. */
	static ATN_BeachStorm* FindStorm(const UObject* WorldContext);

	/** Resumen para la consola (TN.Beach.Storm.Info): frente, velocidad, objetivo y distancia a la última tortuga. */
	FString DescribeState() const;

	/**
	 * Velocidad normal del frente (cm/s; la tortuga anda a 450, esprinta a 800 y la media de la carrera es ~400): 1,8 m/s,
	 * para que quien avanza con normalidad le saque ventaja clara.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0"))
	float DefaultSpeed = 180.f;

	/** Segundos quieta antes de echar a andar con StartStorm(). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0"))
	float DefaultGrace = 15.f;

	/** Aceleración al arrancar (cm/s²: de 0 a 1,8 m/s en 6 s) y al cambiar de velocidad después. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "1.0"))
	float StartAccel = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "1.0"))
	float SpeedChangeAccel = 15.f;

	/** Ronda larga: pasados estos segundos de marcha acelera SpeedRampPerMinute (cm/s por minuto) hasta MaxSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0"))
	float LateStartSeconds = 240.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0"))
	float SpeedRampPerMinute = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0"))
	float MaxSpeed = 300.f;

	/** Final de la ronda: con la primera tortuga pasado este tanto del recorrido, al menos EndRushSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EndRushProgress = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0"))
	float EndRushSpeed = 250.f;

	/** Si la última tortuga le saca más de CatchUpGap (cm), va a CatchUpSpeed hasta quedarse a CatchUpRelease. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0"))
	float CatchUpGap = 18000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0"))
	float CatchUpRelease = 12000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0"))
	float CatchUpSpeed = 280.f;

	/** Semiancho del frente (cm): la playa (140 m) y las palmeras de los lados. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "1000.0"))
	float HalfWidth = 22000.f;

	/** Margen por detrás del frente antes de contar como dentro para lo que se ve y se oye (niebla, tos) (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0"))
	float InsideMargin = 600.f;

	/** Aviso a la tortuga local con el frente a menos de esto por detrás (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0"))
	float WarnDistance = 2500.f;

	/** Patada: la que se queda detrás del frente sale en bola hasta aquí por delante de él (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0"))
	float KickAhead = 2000.f;

	/** Detrás del frente más que KickSlack (cm) durante KickDelay (s): patada. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0"))
	float KickSlack = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0"))
	float KickDelay = 0.25f;

	/** Vuelo de la bola (s, más largo cuanto más lejos tiene que llegar) y mareo que sigue al aterrizar (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.3"))
	float KickMinFlight = 1.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.3"))
	float KickMaxFlight = 2.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0"))
	float KickStunExtra = 0.8f;

protected:
	/** Todas las máquinas: un bañista ha dado una patada a Victim (la pierna que patea, arena, sonido y aviso). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastKick(ATortugaCharacter* Victim, FVector_NetQuantize10 Launch);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storm")
	TObjectPtr<USceneComponent> Root;

	/** Post-proceso de dentro (tinte de arena y viñeta) para el jugador local. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storm")
	TObjectPtr<UPostProcessComponent> InsidePostProcess;

private:
	/** Tramo de marcha en curso: sale de StartOffset a FrontSpeed, acelera FrontAccel (con signo) hasta TargetSpeed. */
	UPROPERTY(Replicated)
	float StartOffset = 0.f;

	UPROPERTY(Replicated)
	float FrontSpeed = 0.f;

	UPROPERTY(Replicated)
	float FrontAccel = 0.f;

	UPROPERTY(Replicated)
	float TargetSpeed = 0.f;

	UPROPERTY(Replicated)
	float StartServerTime = 0.f;

	UPROPERTY(Replicated)
	float Grace = 0.f;

	/** Frente parado (StopStorm): se queda aquí y a la vista. */
	UPROPERTY(Replicated)
	float FrozenFront = 0.f;

	UPROPERTY(Replicated)
	bool bActive = false;

	/** Se ve (arrancada alguna vez, aunque esté parada). */
	UPROPERTY(Replicated)
	bool bShown = false;

	// Servidor.
	/** Cuánto lleva cada tortuga detrás del frente (s) y hasta cuándo no se la vuelve a patear (vuela). */
	TMap<TWeakObjectPtr<ATortugaCharacter>, float> BehindFor;
	TMap<TWeakObjectPtr<ATortugaCharacter>, double> KickedUntil;
	float CheckTimer = 0.f;
	float SpeedTimer = 0.f;
	/** Velocidad normal de esta tormenta, hora del servidor en que echó a andar y si va alcanzando a la última. */
	float BaseSpeed = 180.f;
	double MarchStartTime = 0.0;
	bool bCatchingUp = false;
	FRandomStream Rng;

	/** Generador de la playa (suelo sin trazas y progreso); se busca una vez. */
	TWeakObjectPtr<ATN_BeachRaceGenerator> Generator;
	bool bGeneratorLooked = false;

	// ── Efectos (solo con pantalla) ──

	/** Un trasto que vuela por el borde del frente (posición y velocidad en el mundo), con su fundido y la arena debajo. */
	struct FDebris
	{
		int32 Item = 0;
		FVector Pos = FVector::ZeroVector;
		FVector Vel = FVector::ZeroVector;
		FQuat Rot = FQuat::Identity;
		FVector SpinAxis = FVector::UpVector;
		float SpinRate = 0.f;
		float Scale = 1.f;
		float Lift = 0.f;
		float Ground = 0.f;
		float GroundTimer = 0.f;
		/** Dónde se queda respecto al frente (cm, a lo largo de la carrera): lo va buscando con un muelle. */
		float EdgeOffset = 0.f;
		/** Fundido (0-1), edad y vida (s); al morir se funde en vez de desaparecer. */
		float Fade = 0.f;
		float Age = 0.f;
		float Life = 5.f;
		bool bDying = false;
		bool bLive = false;
	};

	/** Un bañista que pisa en el borde del frente (Y local, profundidad tras el frente, paso), su fundido y la arena. */
	struct FBather
	{
		float SlotY = 0.f;
		float Depth = 600.f;
		float Rate = 0.5f;
		float Phase = 0.f;
		float Scale = 1.f;
		float LastStep = 0.f;
		float Ground = 0.f;
		float GroundTimer = 0.f;
		float Fade = 0.f;
	};

	/** La pierna de un bañista que da una patada (se ve un momento, se funde). */
	struct FKickLeg
	{
		FVector Hip = FVector::ZeroVector;
		float Yaw = 0.f;
		float Age = 10.f;
	};

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Veil;

	/** Material dinámico del velo: su «Opacity» sigue a FrontBlend (se funde al acercarse o alejarse el frente). */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> VeilMid;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> DebrisComps;

	/** Por trasto: su malla opaca, su copia translúcida (para fundirse) y el material de la copia. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> DebrisSolid;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> DebrisSoft;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DebrisMids;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> BatherRoots;

	/** Por bañista: caderas, pierna izquierda y pierna derecha (con sus mallas opaca y translúcida y el material). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> BatherParts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> BatherSolid;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> BatherSoft;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> BatherMids;

	/** Piernas de las patadas (dos a la vez como mucho). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> KickComps;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> KickMids;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> KickSolid;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> KickSoft;

	UPROPERTY(Transient)
	TObjectPtr<UTN_BeachEnemySynthComponent> Voice;

	TArray<FDebris> Debris;
	TArray<FBather> Bathers;
	TArray<FKickLeg> KickLegs;
	int32 NextKick = 0;
	bool bFXReady = false;
	float FXTime = 0.f;
	float FrontBlend = 0.f;
	float InsideBlend = 0.f;
	float WarnBlend = 0.f;
	float FrontGroundZ = 0.f;
	bool bFrontGroundValid = false;
	float GroundTimer = 0.f;
	float CoughTimer = 0.f;
	uint32 FxRng = 0x51A7B00Du;
	TMap<TWeakObjectPtr<ATortugaCharacter>, float> CoughInside;
	TNAmbientFX::FEmitter FrontSand;
	TNAmbientFX::FEmitter FrontDust;
	TNAmbientFX::FEmitter FrontClouds;
	TNAmbientFX::FEmitter ViewSand;
	TNAmbientFX::FEmitter ViewDust;

	/** Avisos a la tortuga local («¡QUE VIENE LA TORMENTA!», «¡CORRE!») y el de la patada. */
	FTNTrapPopText WarnPop;
	FTNTrapPopText HitPop;
	bool bWarned = false;
	bool bWarnedInside = false;

	/** Niebla del nivel (se cierra dentro y vuelve a su estado al salir). */
	TWeakObjectPtr<UExponentialHeightFogComponent> Fog;
	bool bFogCached = false;
	bool bFogApplied = false;
	float FogDensity0 = 0.f;
	float FogFalloff0 = 0.f;
	float FogStart0 = 0.f;
	float FogOpacity0 = 1.f;
	FLinearColor FogColor0 = FLinearColor::White;

	/** Distancia recorrida y velocidad del tramo en curso a los T segundos de echar a andar. */
	void SegmentAt(double T, float& OutDistance, float& OutSpeed) const;
	/** Servidor: empieza un tramo nuevo desde donde está hacia NewTarget (cm/s). */
	void ChangeSpeed(float NewTarget);
	/** Servidor: decide a qué velocidad debe ir (base, ronda larga, final de la ronda, alcanzar a la última). */
	void ServerUpdateSpeed();
	/** Servidor: patea a quien se ha quedado detrás del frente. */
	void ServerCheck();
	ATN_BeachRaceGenerator* FindGenerator();
	/** Arena bajo Where: la del generador (sin trazas) o una traza que no se deja engañar por los muros invisibles. */
	float GroundAt(const FVector& Where);
	void SetupFX();
	void TickFX(float DeltaSeconds);
	void TickDebris(float DeltaSeconds, float Front, const FVector& ViewLocal);
	void SpawnDebris(FDebris& D, float Front, const FVector& ViewLocal);
	void TickBathers(float DeltaSeconds, float Front, const FVector& ViewLocal);
	void TickKicks(float DeltaSeconds);
	void TickCough(float DeltaSeconds);
	void ApplyInsideLook();
	void RestoreFog();
	float Rand01();

	/**
	 * Fundido de una pieza: con Fade 1, su malla opaca de siempre (con su luz); por debajo, la copia translúcida
	 * (M_ProcFXSoft) con esa opacidad; con 0, oculta. Solo cambia de malla o de material al cruzar los extremos.
	 */
	void ApplyFade(UStaticMeshComponent* Comp, UStaticMesh* Solid, UStaticMesh* Soft, TObjectPtr<UMaterialInstanceDynamic>& Mid, float Fade);
};
