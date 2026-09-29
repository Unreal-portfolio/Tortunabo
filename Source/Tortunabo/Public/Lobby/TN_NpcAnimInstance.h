#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "TN_NpcAnimInstance.generated.h"

class UAnimationAsset;
class UAnimSequence;
class USkeletalMeshComponent;

/** Lo que el hilo de juego pasa a la evaluación de la pose del personaje del lobby. */
struct FTNNpcAnimFrame
{
	float IdleTime = 0.f;
	float GestureTime = 0.f;
	/** Peso del gesto sobre la espera (0-1, ya suavizado). */
	float GestureW = 0.f;
};

/** Evaluación de la pose: espera en bucle y el gesto encima, mezclados por su peso. */
struct FTNNpcAnimProxy : public FAnimInstanceProxy
{
	FTNNpcAnimProxy() = default;
	explicit FTNNpcAnimProxy(UAnimInstance* InInstance) : FAnimInstanceProxy(InInstance) {}

	virtual bool Evaluate(FPoseContext& Output) override;

	FTNNpcAnimFrame Frame;
	const UAnimSequence* IdleClip = nullptr;
	const UAnimSequence* GestureClip = nullptr;
};

/**
 * Animación de los personajes del lobby (tendero y general), sin AnimBP: la espera en bucle (sin salto al volver a
 * empezar) y, encima, un gesto (saludar) que entra y sale fundido en vez de cortar de golpe como PlayAnimation.
 */
UCLASS(Transient)
class TORTUNABO_API UTN_NpcAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** Clip de espera (en bucle). */
	void SetIdle(UAnimSequence* InIdle);

	/** Hace el gesto una vez: entra en BlendIn segundos y sale en BlendOut al acabar el clip. */
	void PlayGesture(UAnimSequence* InGesture, float BlendIn = 0.25f, float BlendOut = 0.4f);

	/** Segundos que le quedan al gesto en curso (0 si no hay). */
	float GetGestureTimeLeft() const;

	/**
	 * Pone esta animación en Mesh con Idle en bucle. Si Idle no es una secuencia, cae a PlayAnimation (sin fundidos) y
	 * devuelve nullptr.
	 */
	static UTN_NpcAnimInstance* SetupOn(USkeletalMeshComponent* Mesh, UAnimationAsset* Idle);

	/** Hace el gesto en Mesh: fundido si lleva esta animación; si no, con PlayAnimation. */
	static void PlayGestureOn(USkeletalMeshComponent* Mesh, UAnimationAsset* Gesture);

	/** Vuelve a la espera: con esta animación no hace falta (el gesto sale solo); si no, PlayAnimation de Idle. */
	static void ReturnToIdleOn(USkeletalMeshComponent* Mesh, UAnimationAsset* Idle);

	/** En el mundo del editor (sin jugar), pone a Mesh en su espera animada en vez de en la postura en T. */
	static void PreviewInEditor(USkeletalMeshComponent* Mesh, UAnimationAsset* Idle);

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> IdleAnim;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> GestureAnim;

	FTNNpcAnimFrame Frame;
	float GestureBlendIn = 0.25f;
	float GestureBlendOut = 0.4f;
	bool bGestureActive = false;
};
