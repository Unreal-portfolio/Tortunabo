#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/Beach/TN_BeachEnemy.h"
#include "World/Beach/TN_BeachTrapCommon.h"
#include "World/ProcMap/TN_ProcMapAmbientFX.h"
#include "TN_BeachStorm.generated.h"

class UExponentialHeightFogComponent;
class UPostProcessComponent;
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
 * de ATN_PathStorm, color arena) con piernas gigantes que pisan dentro, y sombrillas, cubos, sillas de playa, toallas,
 * flotadores, palas, chanclas y pelotas volando a escala, a poca altura y saliendo por delante del frente.
 *
 * Es justa: arranca tarde (15 s de gracia y 7,5 s cogiendo velocidad) y va más despacio que la media de la carrera
 * (3 m/s; la tortuga anda a 4,5 y la media con obstáculos es ~4). Solo acelera al final: pasados 4 minutos, cuando la
 * primera tortuga ha hecho el 80 % del recorrido o si la última se ha quedado muy atrás (para que siempre se note). Antes
 * de alcanzarte avisa (temblor, viento, arena y «¡QUE VIENE LA TORMENTA!»); si te alcanza, un revolcón: derribo con
 * ragdoll y mareo empujado hacia el mar, como mucho uno cada 9 s y tras 1,2 s dentro (no un tambor de golpes). Dentro, la
 * imagen se cierra (niebla y tinte de arena) y la tortuga tose (UTN_StormCoughComponent).
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
 * empieza un tramo nuevo desde donde está. El servidor decide a quién revuelca; los efectos son locales o por multicast.
 * Consola: TN.Beach.Storm.Start [metros por detrás] [cm/s], TN.Beach.Storm.Stop y TN.Beach.Storm.Info
 * (TN_BeachEnemyDebug.cpp).
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

	/** Velocidad normal del frente (cm/s; la tortuga anda a 450, esprinta a 800 y la media de la carrera es ~400). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0"))
	float DefaultSpeed = 300.f;

	/** Segundos quieta antes de echar a andar con StartStorm(). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0"))
	float DefaultGrace = 15.f;

	/** Aceleración al arrancar (cm/s²: de 0 a 3 m/s en 7,5 s) y al cambiar de velocidad después. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "1.0"))
	float StartAccel = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "1.0"))
	float SpeedChangeAccel = 25.f;

	/** Ronda larga: pasados estos segundos de marcha acelera SpeedRampPerMinute (cm/s por minuto) hasta MaxSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0"))
	float LateStartSeconds = 240.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0"))
	float SpeedRampPerMinute = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0"))
	float MaxSpeed = 520.f;

	/** Final de la ronda: con la primera tortuga pasado este tanto del recorrido, al menos EndRushSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EndRushProgress = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0"))
	float EndRushSpeed = 420.f;

	/** Si la última tortuga le saca más de CatchUpGap (cm), va a CatchUpSpeed hasta quedarse a CatchUpRelease. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0"))
	float CatchUpGap = 18000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0"))
	float CatchUpRelease = 12000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0"))
	float CatchUpSpeed = 470.f;

	/** Semiancho del frente (cm): la playa (140 m) y las palmeras de los lados. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "1000.0"))
	float HalfWidth = 22000.f;

	/** Margen por detrás del frente antes de contar como dentro (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0"))
	float InsideMargin = 600.f;

	/** Aviso a la tortuga local con el frente a menos de esto por detrás (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0"))
	float WarnDistance = 2500.f;

	/** Revolcón: tiempo dentro antes del primero, derribo (s) y cada cuánto como mucho mientras siga dentro. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0"))
	float FirstHitDelay = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.1"))
	float KnockSeconds = 2.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.5"))
	float HitInterval = 9.f;

	/** Empujón del ragdoll en el revolcón (cm/s): hacia delante (el mar), hacia arriba y de lado al azar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm")
	float PushForward = 650.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm")
	float PushUp = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm")
	float PushSide = 150.f;

protected:
	/** Todas las máquinas: la tormenta ha revolcado a Victim (el empujón va a su ragdoll en cuanto simule). */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastTumble(ATortugaCharacter* Victim, FVector_NetQuantize10 Push, FVector_NetQuantize10 Spin);

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
	TMap<TWeakObjectPtr<ATortugaCharacter>, double> LastHit;
	TMap<TWeakObjectPtr<ATortugaCharacter>, float> InsideFor;
	float CheckTimer = 0.f;
	float SpeedTimer = 0.f;
	/** Velocidad normal de esta tormenta, hora del servidor en que echó a andar y si va alcanzando a la última. */
	float BaseSpeed = 300.f;
	double MarchStartTime = 0.0;
	bool bCatchingUp = false;
	FRandomStream Rng;

	/** Generador de la playa (suelo sin trazas y progreso); se busca una vez. */
	TWeakObjectPtr<ATN_BeachRaceGenerator> Generator;
	bool bGeneratorLooked = false;

	// ── Efectos (solo con pantalla) ──

	/** Un trasto que vuela (posición y velocidad en el mundo) y la arena debajo. */
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
		bool bLive = false;
	};

	/** Un bañista que pisa dentro de la tormenta (Y local, profundidad tras el frente, paso) y la arena bajo él. */
	struct FBather
	{
		float SlotY = 0.f;
		float Depth = 3000.f;
		float Rate = 0.5f;
		float Phase = 0.f;
		float Scale = 1.f;
		float LastStep = 0.f;
		float Ground = 0.f;
		float GroundTimer = 0.f;
	};

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Veil;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> DebrisComps;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> BatherRoots;

	/** Por bañista: caderas, pierna izquierda y pierna derecha. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> BatherParts;

	UPROPERTY(Transient)
	TObjectPtr<UTN_BeachEnemySynthComponent> Voice;

	TArray<FDebris> Debris;
	TArray<FBather> Bathers;
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

	/** Avisos a la tortuga local («¡QUE VIENE LA TORMENTA!», «¡CORRE!») y el del revolcón. */
	FTNTrapPopText WarnPop;
	FTNTrapPopText HitPop;
	bool bWarned = false;
	bool bWarnedInside = false;

	/** Empujones al ragdoll de las revolcadas que aún no simulan en esta máquina. */
	FTNBeachRagdollPushes RagdollPushes;

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
	void ServerCheck();
	ATN_BeachRaceGenerator* FindGenerator();
	/** Arena bajo Where: la del generador (sin trazas) o una traza que no se deja engañar por los muros invisibles. */
	float GroundAt(const FVector& Where);
	void SetupFX();
	void TickFX(float DeltaSeconds);
	void TickDebris(float DeltaSeconds, float Front, const FVector& ViewLocal);
	void SpawnDebris(FDebris& D, float Front, const FVector& ViewLocal);
	void TickBathers(float DeltaSeconds, float Front, const FVector& ViewLocal);
	void TickCough(float DeltaSeconds);
	void ApplyInsideLook();
	void RestoreFog();
	float Rand01();
};
