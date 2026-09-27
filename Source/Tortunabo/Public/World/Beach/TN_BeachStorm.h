#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/ProcMap/TN_ProcMapAmbientFX.h"
#include "TN_BeachStorm.generated.h"

class UExponentialHeightFogComponent;
class UPostProcessComponent;
class UStaticMeshComponent;
class UTN_BeachEnemySynthComponent;
class ATortugaCharacter;

/**
 * Tormenta de bañistas de la carrera en la playa: la tormenta del camino del cooperativo (ATN_PathStorm) llevada a la
 * playa. No es un elemento del reparto: la crea y la arranca el GameMode de la carrera en cada ronda.
 *
 * Un frente recto a lo ancho de la playa que sale de detrás de la salida y avanza hacia el mar. Son los bañistas que
 * llegan: una cortina de arena y polvo (el velo de ATN_PathStorm, color arena) con piernas gigantes que pisan dentro, y
 * sombrillas, cubos, sillas de playa, toallas, flotadores, palas, chanclas y pelotas volando a escala. A quien alcanza lo
 * revuelca: bola aturdida lanzada hacia delante, y otra vez cada 3 s mientras siga dentro (en carrera no se muere).
 * Dentro, la imagen se cierra (niebla y tinte de arena) y la tortuga tose (UTN_StormCoughComponent). Acelera poco a poco
 * (SpeedRampPerMinute): al final de una ronda larga hay que esprintar para no quedarse dentro.
 *
 * Marco: el del propio actor. Su X local es la dirección de la carrera (hacia el mar), su Y local el ancho (centro en
 * Y = 0) y su Z, el suelo de referencia. El frente es la recta X local = GetFrontDistance().
 *
 * Interfaz para el GameMode (servidor):
 *  - Crearla detrás de la salida, en el centro de la playa, girada hacia el mar, y llamar a StartStorm() (sin
 *    parámetros, por nombre: el frente sale del propio actor a DefaultSpeed tras DefaultGrace) o, desde C++, a
 *    StartStormAt(Desplazamiento, Velocidad, Gracia).
 *  - StopStorm() (sin parámetros): la para; se queda quieta y a la vista (recuento). Destruirla la quita.
 *  - IsLocationInside, GetFrontDistance, GetFrontLocation, IsStormActive, FindStorm.
 *
 * Red: replica solo el arranque (desplazamiento, velocidad, aceleración, hora del servidor, gracia); cada máquina calcula
 * el frente con el reloj del servidor. El servidor decide a quién revuelca; los efectos son locales o por multicast no
 * fiable. Consola: TN.Beach.Storm.Start [metros por detrás] [cm/s] y TN.Beach.Storm.Stop (TN_BeachEnemyDebug.cpp).
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

	/** Servidor: arranca desde el propio actor a DefaultSpeed (acelerando SpeedRampPerMinute) tras DefaultGrace. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Storm")
	void StartStorm();

	/** Servidor: el frente sale InStartOffset cm por delante del actor (negativo: detrás) y avanza a Speed cm/s tras GraceSeconds. */
	void StartStormAt(float InStartOffset, float Speed, float GraceSeconds = 0.f);

	/** Servidor: para la tormenta donde esté (se queda a la vista). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Storm")
	void StopStorm();

	/** Distancia del frente al actor a lo largo de la carrera (cm; cualquier máquina, con el reloj del servidor). */
	UFUNCTION(BlueprintPure, Category = "Storm")
	float GetFrontDistance() const;

	/** Punto del frente en el centro de la playa (a la altura del actor). */
	UFUNCTION(BlueprintPure, Category = "Storm")
	FVector GetFrontLocation() const;

	UFUNCTION(BlueprintPure, Category = "Storm")
	bool IsStormActive() const { return bActive; }

	/** Si una posición está dentro: por detrás del frente más que InsideMargin y dentro del ancho. */
	bool IsLocationInside(const FVector& WorldLocation) const;

	/** La tormenta de este mundo (la primera), o null. */
	static ATN_BeachStorm* FindStorm(const UObject* WorldContext);

	/** Velocidad del frente al arrancar con StartStorm() (cm/s; la tortuga anda a 450 y esprinta a 800). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0"))
	float DefaultSpeed = 330.f;

	/** Lo que gana de velocidad cada minuto (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0"))
	float SpeedRampPerMinute = 30.f;

	/** Segundos quieta antes de echar a andar con StartStorm(). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0"))
	float DefaultGrace = 6.f;

	/** Semiancho del frente (cm): la playa (140 m) y las palmeras de los lados. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "1000.0"))
	float HalfWidth = 22000.f;

	/** Margen por detrás del frente antes de contar como dentro (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.0"))
	float InsideMargin = 400.f;

	/** Aturdimiento de cada revolcón (s) y cada cuánto se repite mientras siga dentro. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.1"))
	float StunSeconds = 2.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm", meta = (ClampMin = "0.5"))
	float HitInterval = 3.f;

	/** Lanzamiento del revolcón (cm/s): hacia delante, hacia arriba y de lado al azar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm")
	float PushForward = 1100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm")
	float PushUp = 700.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm")
	float PushSide = 300.f;

protected:
	/** Todas las máquinas: la tormenta ha revolcado a Victim. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastTumble(ATortugaCharacter* Victim);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storm")
	TObjectPtr<USceneComponent> Root;

	/** Post-proceso de dentro (tinte de arena y viñeta) para el jugador local. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storm")
	TObjectPtr<UPostProcessComponent> InsidePostProcess;

private:
	UPROPERTY(Replicated)
	float StartOffset = 0.f;

	UPROPERTY(Replicated)
	float FrontSpeed = 0.f;

	/** Aceleración del frente (cm/s²). */
	UPROPERTY(Replicated)
	float FrontAccel = 0.f;

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
	float CheckTimer = 0.f;
	FRandomStream Rng;

	// ── Efectos (solo con pantalla) ──

	/** Un trasto que vuela (posición y velocidad en el mundo). */
	struct FDebris
	{
		int32 Item = 0;
		FVector Pos = FVector::ZeroVector;
		FVector Vel = FVector::ZeroVector;
		FQuat Rot = FQuat::Identity;
		FVector SpinAxis = FVector::UpVector;
		float SpinRate = 0.f;
		float Scale = 1.f;
		bool bLive = false;
	};

	/** Un bañista que pisa dentro de la tormenta (Y local, profundidad tras el frente y paso). */
	struct FBather
	{
		float SlotY = 0.f;
		float Depth = 3000.f;
		float Rate = 0.5f;
		float Phase = 0.f;
		float Scale = 1.f;
		float LastStep = 0.f;
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
	float FrontGroundZ = 0.f;
	float GroundTimer = 0.f;
	float CoughTimer = 0.f;
	uint32 FxRng = 0x51A7B00Du;
	TMap<TWeakObjectPtr<ATortugaCharacter>, float> CoughInside;
	TNAmbientFX::FEmitter FrontSand;
	TNAmbientFX::FEmitter FrontDust;
	TNAmbientFX::FEmitter FrontClouds;
	TNAmbientFX::FEmitter ViewSand;
	TNAmbientFX::FEmitter ViewDust;

	/** Niebla del nivel (se cierra dentro y vuelve a su estado al salir). */
	TWeakObjectPtr<UExponentialHeightFogComponent> Fog;
	bool bFogCached = false;
	bool bFogApplied = false;
	float FogDensity0 = 0.f;
	float FogFalloff0 = 0.f;
	float FogStart0 = 0.f;
	float FogOpacity0 = 1.f;
	FLinearColor FogColor0 = FLinearColor::White;

	void ServerCheck();
	void SetupFX();
	void TickFX(float DeltaSeconds);
	void TickDebris(float DeltaSeconds, float Front, const FVector& ViewLocal);
	void SpawnDebris(FDebris& D, float Front, const FVector& ViewLocal);
	void TickBathers(float Front, const FVector& ViewLocal);
	void TickCough(float DeltaSeconds);
	void ApplyInsideLook();
	void RestoreFog();
	float Rand01();
};
