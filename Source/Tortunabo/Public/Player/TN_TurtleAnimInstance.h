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
	/** Locomoción: fases de los ciclos y mezcla (andar sobre la espera, correr sobre andar). */
	float IdleTime = 0.f;
	float WalkTime = 0.f;
	float RunTime = 0.f;
	float WalkW = 0.f;
	float RunW = 0.f;
	/** Inclinación en las curvas y hacia delante al correr (grados). */
	float LeanRoll = 0.f;
	float LeanPitch = 0.f;
	/** Poses de estado. */
	float AirW = 0.f;
	float Falling = 0.f;
	float DiveW = 0.f;
	float SwimW = 0.f;
	float ShellW = 0.f;
	float CarryW = 0.f;
	float CarriedW = 0.f;
	float DownW = 0.f;
	float TiredW = 0.f;
	/** Lanzamiento en curso (segundos desde que empezó; negativo = ninguno) y si era con las dos manos. */
	float ThrowT = -1.f;
	bool bThrowBoth = false;
	/** Emote (índice del catálogo 0-9 o -1), su tiempo y su peso (entra y sale suave). */
	int32 Emote = -1;
	float EmoteTime = 0.f;
	float EmoteW = 0.f;
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
	const UAnimSequence* RunClip = nullptr;
	const UAnimSequence* CheerClip = nullptr;
};

/**
 * Animación de la tortuga del jugador sobre el esqueleto Mixamo de TotugaDemo_Rig, sin AnimBP: espera, andar y correr
 * con los clips (mezclados por velocidad, como ABS_Walk) y, encima, poses hechas en código para el salto, el
 * panzazo, el nado, el caparazón (se esconden cabeza y patas), llevar y ser llevado, el lanzamiento, el tumbado, el
 * cansancio y los diez emotes del catálogo. Hereda de UTN_ProcAnimInstance: los ajustes por hueso que escriben los
 * sistemas viejos se siguen aplicando al final.
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

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> IdleAnim;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> WalkAnim;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> RunAnim;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CheerAnim;

	FTNTurtleAnimFrame Frame;
	float PrevYaw = 0.f;
	bool bWasCarrying = false;
	int32 LastEmote = -1;
};
