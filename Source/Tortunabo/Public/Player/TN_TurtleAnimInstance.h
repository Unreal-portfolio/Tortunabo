#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstanceProxy.h"
#include "Player/TN_ProcAnimInstance.h"
#include "TN_TurtleAnimInstance.generated.h"

class UAnimSequence;

/**
 * Estado de la tortuga que el hilo de juego pasa cada fotograma a la evaluación de la pose (que puede correr en otro
 * hilo). Pesos de 0 a 1 ya suavizados; tiempos en segundos.
 */
struct FTNTurtleAnimFrame
{
	float Clock = 0.f;
	/** Locomoción: tiempo de los clips de espera y andar, ciclos de la carrera y mezcla (andar sobre la espera, correr encima). */
	float IdleTime = 0.f;
	float WalkTime = 0.f;
	float RunTime = 0.f;
	float WalkW = 0.f;
	float RunW = 0.f;
	/** Cuánto de sprint lleva la carrera (braceo e inclinación mayores). */
	float SprintW = 0.f;
	/** Amplitud (grados) del paso de la carrera: la justa para que el pie apoyado no patine a esa velocidad. */
	float RunStride = 40.f;
	/** Inclinación en las curvas y hacia delante al correr (grados). */
	float LeanRoll = 0.f;
	float LeanPitch = 0.f;
	/** Poses de estado. */
	float AirW = 0.f;
	float Falling = 0.f;
	float DiveW = 0.f;
	/** Panzazo sobre la tripa en el suelo (arrastre o reptar), su velocidad (0..1, a 7 m/s) y los golpes (caer, chocar). */
	float SlideW = 0.f;
	float SlideSpeed = 0.f;
	float SlideImpact = 0.f;
	/** Levantarse de la tripa: empujón de brazos y rodillas (sube y baja). */
	float BellyGetUpW = 0.f;
	float SwimW = 0.f;
	float ShellW = 0.f;
	float CarryW = 0.f;
	float CarriedW = 0.f;
	float DownW = 0.f;
	float TiredW = 0.f;
	/** El caparazón tiene cuerpo físico (la malla va tumbada sobre la caja): sin bajar el cuerpo al meterse. */
	bool bShellBody = false;
/** Lanzamiento en curso (segundos desde que empezó; negativo = ninguno) y si era con las dos manos. */
	float ThrowT = -1.f;
	bool bThrowBoth = false;
	/** Emote (índice del catálogo 0-9 o -1), su tiempo y su peso (entra y sale suave). */
	int32 Emote = -1;
	float EmoteTime = 0.f;
	float EmoteW = 0.f;
	/** Emote anterior mientras se funde con el nuevo (cambio de un emote a otro sin cortes; -1 = ninguno). */
	int32 PrevEmote = -1;
	float PrevEmoteTime = 0.f;
	float PrevEmoteW = 0.f;
	/** Levantarse del derribo: peso de la pose del suelo (1 → 0) y del empujón de brazos y rodillas (sube y baja). */
	float GetUpW = 0.f;
	float GetUpFlex = 0.f;
};

/** Evaluación en C++ de la pose de la tortuga (clips de Mixamo y poses procedurales encima). */
struct FTNTurtleAnimProxy : public FAnimInstanceProxy
{
	FTNTurtleAnimProxy() = default;
	explicit FTNTurtleAnimProxy(UAnimInstance* InInstance) : FAnimInstanceProxy(InInstance) {}

	virtual bool Evaluate(FPoseContext& Output) override;

	FTNTurtleAnimFrame Frame;
	const UAnimSequence* IdleClip = nullptr;
	const UAnimSequence* WalkClip = nullptr;
	const UAnimSequence* CheerClip = nullptr;
	/** Pose en la que quedó el ragdoll, en locales e indexada por hueso de la malla (vacía si no se está levantando). */
	TArray<FTransform> GetUpPose;
};

/**
 * Animación de la tortuga del jugador sobre el esqueleto Mixamo de TotugaDemo_Rig, sin AnimBP: espera y andar con los
 * clips (mezclados por velocidad, como ABS_Walk), la carrera del sprint hecha en código y, encima, poses para el
 * salto, el panzazo (en el aire y arrastrándose sobre la tripa, con el empujón para levantarse), el nado, el caparazón
 * (se esconden cabeza y patas), llevar y ser llevado, el lanzamiento, el tumbado, el cansancio y los emotes del catálogo.
 * Hereda de UTN_ProcAnimInstance: los ajustes por hueso que escriben los sistemas viejos se siguen aplicando al final.
 *
 * Las poses se escriben como giros en el espacio de la malla (mira a +Y, arriba +Z, su izquierda +X) sobre la
 * postura de referencia (en T) y se mezclan con la de los clips.
 */
UCLASS(Transient)
class TORTUNABO_API UTN_TurtleAnimInstance : public UTN_ProcAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/**
	 * Empieza la animación de levantarse desde la pose del ragdoll (LocalPose: transformaciones locales indexadas por
	 * hueso de la malla, con la malla ya devuelta a la cápsula): la tortuga gira desde el suelo hasta ponerse de pie en
	 * Seconds, con un empujón de brazos contra el suelo y las rodillas dobladas a mitad de camino.
	 */
	void BeginGetUp(const TArray<FTransform>& LocalPose, float Seconds);

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> IdleAnim;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> WalkAnim;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CheerAnim;

	FTNTurtleAnimFrame Frame;
	float PrevYaw = 0.f;
	bool bWasCarrying = false;
	int32 LastEmote = -1;

	/** Panzazo en el fotograma anterior: sobre la tripa en el suelo, en el aire y velocidad (golpes y levantarse). */
	bool bWasBellyGround = false;
	bool bWasBellyAir = false;
	FVector2D PrevBellyVelocity = FVector2D::ZeroVector;
	/** Levantarse de la tripa: segundos desde que empezó (negativo = no se está levantando). */
	float BellyGetUpElapsed = -1.f;

	/** Levantarse: pose del suelo, tiempo transcurrido y duración (0 = no se está levantando). */
	TArray<FTransform> GetUpPose;
	float GetUpElapsed = 0.f;
	float GetUpDuration = 0.f;
	bool bGetUpPoseSent = false;
};
