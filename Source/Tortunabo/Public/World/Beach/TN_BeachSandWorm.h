#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "GameFramework/Actor.h"
#include "World/Beach/TN_BeachTrapCommon.h"
#include "World/ProcMap/TN_ProcMapAmbientFX.h"
#include "TN_BeachSandWorm.generated.h"

class ACharacter;
class APlayerController;
class UStaticMeshComponent;
class UTextRenderComponent;
class UTN_BeachSandWormSynthComponent;

/**
 * Gusano de arena gigante del modo carrera (Docs/Modo_Carrera.md, «Gusano de arena»): cuando se acaba la cuenta atrás
 * de 10 s tras la primera tortuga en el agua, a cada tortuga que no ha llegado le sale de debajo de la arena un gusano
 * enorme que se la come de un bocado y se vuelve a hundir. Es un remate cómico: nadie muere (la tortuga se queda dentro,
 * oculta, hasta la ronda siguiente, que vuelve a crear a todas en la salida).
 *
 * Contrato: ATN_BeachRaceGameMode llama a EatTurtle en el servidor al llegar la cuenta a 0 y espera EatSeconds antes
 * del recuento. Implementación: Private/World/Beach/TN_BeachSandWorm.cpp.
 *
 * Escena (EatSeconds, igual en todas las máquinas con el reloj del servidor): aviso con la arena que tiembla y se hunde en
 * un remolino, el gusano que sale en vertical con la boca abierta como una flor y la tortuga dentro, bocado («¡ÑAM!»),
 * trago (un bulto que baja por el cuerpo), eructo de arena y vuelta a la arena, con un cráter que se cierra.
 *
 * Red (servidor escucha): el servidor crea el actor (replicado, siempre relevante) con la tortuga, la hora de inicio, el
 * desfase, el punto del suelo, el sentido del arco y la semilla; nada más viaja. Cada máquina anima el gusano y coloca a
 * la tortuga con las mismas cuentas (sin RPC por fotograma). El servidor le quita a la tortuga el aturdimiento, la bola, el
 * derribo y la carga, le para el movimiento, le quita la colisión y la oculta en el bocado; la tortuga del jugador local
 * se queda sin control y su cámara mira la escena desde fuera. El gusano sigue vivo (sin nada que ver) hasta que la
 * tortuga reaparece (alguien la vuelve a mostrar) o deja de existir; entonces el servidor lo destruye.
 *
 * Consola: TN.Beach.Worm [jugador] (en el anfitrión) se come ya a esa tortuga.
 */
UCLASS()
class TORTUNABO_API ATN_BeachSandWorm : public AActor
{
	GENERATED_BODY()

public:
	/** Duración total de la escena (aviso en la arena, salida, bocado y vuelta a la arena), en segundos. */
	static constexpr float EatSeconds = 3.2f;

	/**
	 * Servidor: crea (replicado) un gusano que sale bajo Turtle y se la come; la tortuga queda quieta, sin control y
	 * oculta desde el bocado. Devuelve el gusano, o nullptr si no se puede (sin autoridad, tortuga nula o ya comida).
	 */
	static ATN_BeachSandWorm* EatTurtle(ACharacter* Turtle);

	/** true si Turtle ya está en la boca de un gusano (en cualquier máquina). */
	static bool IsBeingEaten(const ACharacter* Turtle);

	ATN_BeachSandWorm();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** Vista de la tortuga comida (su cámara mira la escena desde fuera, a un lado). */
	virtual void CalcCamera(float DeltaTime, struct FMinimalViewInfo& OutResult) override;

	/** La tortuga que se come. */
	ACharacter* GetVictim() const { return Victim; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// ── Replicado (se fija al crearlo y no cambia) ──────────────────────────

	UPROPERTY(Replicated)
	TObjectPtr<ACharacter> Victim;

	/** Reloj del servidor al crearlo. */
	UPROPERTY(Replicated)
	float StartTime = 0.f;

	/** Desfase (s, 0..0,3) con los demás gusanos de la misma tanda; la escena se aprieta para acabar igual a EatSeconds. */
	UPROPERTY(Replicated)
	float StartDelay = 0.f;

	/** Suelo bajo la tortuga: por ahí sale. */
	UPROPERTY(Replicated)
	FVector_NetQuantize10 GroundPoint = FVector_NetQuantize10(0.0, 0.0, 0.0);

	/** Hacia dónde se dobla en arco al tragar (grados de guiñada). */
	UPROPERTY(Replicated)
	float ArcYaw = 0.f;

	/** Paleta y pequeñas variaciones. */
	UPROPERTY(Replicated)
	int32 LookSeed = 0;

	// ── Piezas (solo con pantalla) ───────────────────────────────────────────

	UPROPERTY(VisibleAnywhere, Category = "Beach")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Lips;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Segments;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Crater;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Whirl;

	UPROPERTY(Transient)
	TObjectPtr<UTextRenderComponent> NomText;

	UPROPERTY(Transient)
	TObjectPtr<UTN_BeachSandWormSynthComponent> Voice;

	/** Postura del gusano en este fotograma (las mismas cuentas en todas las máquinas). */
	struct FWormPose
	{
		/** Cuerpo fuera de la arena (cm de columna, del suelo al borde de la boca; negativo = aún bajo tierra). */
		double Exposed = -1000.0;
		/** Arco de la parte de arriba (radianes) y ondulación de lado (amplitud en cm y fase). */
		double Bend = 0.0;
		double SwayAmp = 0.0;
		double SwayPhase = 0.0;
		/** Borde de la boca y hacia dónde mira. */
		FVector Rim = FVector::ZeroVector;
		FVector Axis = FVector::UpVector;
	};

	FWormPose Pose;

	/** Hora del servidor suavizada (para animar igual en todas las máquinas sin saltos). */
	FTNTrapClock SceneClock;
	float LastTau = -100.f;
	bool bHasScreen = false;
	bool bVisualsBuilt = false;
	bool bSlowTick = false;

	// Tortuga en esta máquina.
	bool bControlLocal = false;
	bool bHiddenLocal = false;
	bool bReleasedLocal = false;
	bool bSmoothingSaved = false;
	uint8 SavedSmoothing = 0;
	bool bCorrectionsOff = false;
	bool bInputOff = false;
	bool bCollisionOff = false;
	bool bPoseSet = false;
	FVector ControlFrom = FVector::ZeroVector;
	float ControlFromTau = 0.f;
	float VictimYaw = 0.f;

	// Cámara de la tortuga comida (solo en la máquina de su jugador).
	TWeakObjectPtr<APlayerController> CameraPC;
	bool bCameraTaken = false;
	bool bCameraReady = false;
	bool bCameraGaveUp = false;
	FVector CamDir = FVector::ForwardVector;
	float CamDist = 1500.f;
	float CamHeight = 400.f;
	FVector CamLook = FVector::ZeroVector;
	FVector CamLoc = FVector::ZeroVector;
	FRotator CamRot = FRotator::ZeroRotator;

	// Efectos.
	TNAmbientFX::FEmitter Dust;
	TNAmbientFX::FEmitter SandFall;
	TNAmbientFX::FEmitter BurpCloud;
	FTNTrapBurst Pebbles;
	FTNTrapBurst Clods;
	float PebbleTimer = 0.f;
	float WhirlSpin = 0.f;
	/** Normal de la arena bajo el gusano (el cráter y el remolino se apoyan en ella); se mira en cada máquina. */
	FVector GroundNormal = FVector::UpVector;
	float NomAge = 10.f;
	FVector NomAt = FVector::ZeroVector;

	/** Segundos de escena en el reloj del servidor Now (negativo durante el desfase; con desfase, algo más deprisa). */
	float SceneTime(double Now) const;

	/** Suelo, sentido del arco y su perpendicular. */
	FVector GetGround() const { return FVector(GroundPoint); }
	FVector GetArcDir() const;
	FVector GetSideDir() const;

	/** Todas las máquinas: columna del gusano a Along cm del suelo (negativo = bajo tierra) y hacia dónde va. */
	void SpineAt(double Along, FVector& OutPoint, FVector& OutTangent) const;
	void UpdatePose(float Tau);

	// ── Servidor ──
	void ServerTick(float Tau);
	/** Le quita a la tortuga aturdimiento, bola, derribo y carga, y la deja sin movimiento ni colisión. */
	void ServerCalmVictim();

	// ── La tortuga (todas las máquinas) ──
	void TickVictim(float Tau);
	void BeginControlLocal(float Tau);
	void EndSceneControlLocal();
	/** La tortuga ha reaparecido o el gusano se va: devuelve lo tocado en esta máquina (control, cámara, marcas). */
	void ReleaseLocal();
	FVector VictimLocationAt(float Tau, float HalfHeight) const;

	// ── Lo que se ve y se oye (máquinas con pantalla) ──
	void BuildVisuals();
	void VisualTick(float Tau, float DeltaSeconds);
	void PoseBody(float Tau, float DeltaSeconds);
	void FireEvents(float Tau, float PrevTau);
	void TickNomText(float DeltaSeconds);
	void ShowNom(const FVector& WorldAt);

	// ── Cámara ──
	void TickCamera(float Tau, float DeltaSeconds);
	APlayerController* FindVictimLocalController() const;
};
