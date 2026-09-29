#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "TN_PickupGlowComponent.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

/**
 * Marca de «esto se puede coger» de los objetos del suelo (Docs/Botin_Decorados.md, «Brillo de lo que se coge»). La
 * lleva de serie ATN_PickupInteractableBase, así que sale en todos los modos (cooperativo, carrera y lobby) y en todos
 * sus Blueprints sin tocarlos: objetos de las zonas de objetos, los que salen de rebuscar o del cofre, lo que se suelta
 * y la bola lanzada cuando se para.
 *
 * Solo en las máquinas con pantalla (nada en el servidor dedicado) y solo visual, sin réplica:
 *  - Anillo dorado de guiones en el suelo, que gira despacio y respira (se apoya en el suelo que haya debajo, inclinado
 *    con él). Se dibuja hasta RingDrawDistance.
 *  - Columna de luz tenue que sube del anillo y se desvanece (hasta BeamDrawDistance): lo que se ve de lejos.
 *  - Chispitas doradas que suben del anillo (las de los decorados que se rebuscan), cerca de la cámara.
 *  - Luz suave dorada sin sombras, solo muy cerca de la cámara (LightRange) y apagándose al alejarse.
 *  - El objeto (SetFloatTarget: la malla del pickup) sube un poco, flota y gira despacio, cerca de la cámara.
 *
 * Mientras el objeto se mueve (el saltito al salir de un rebuscable, p. ej.) la marca se esconde; al pararse, aparece
 * creciendo. Lejos de la cámara el tick va cada 0,35 s y solo mira la distancia.
 */
UCLASS(ClassGroup = (Tortunabo), meta = (BlueprintSpawnableComponent))
class TORTUNABO_API UTN_PickupGlowComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UTN_PickupGlowComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Lo que flota y gira (la malla del objeto) y su altura de reposo (cm, relativa a su padre). El dueño lo llama cada
	 * vez que coloca esa malla; la altura tiene que ser la de reposo, no la del objeto ya flotando.
	 */
	void SetFloatTarget(USceneComponent* InTarget, float InRestZ);

	/** Enciende o apaga la marca entera (al recogerse el objeto se apaga). */
	UFUNCTION(BlueprintCallable, Category = "Pickup Glow")
	void SetGlowEnabled(bool bOn);

	UFUNCTION(BlueprintPure, Category = "Pickup Glow")
	bool IsGlowEnabled() const { return bGlowEnabled; }

	/** Radio del anillo (cm); 0 = según el tamaño del objeto (55-110 cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup Glow", meta = (ClampMin = "0.0"))
	float RingRadius = 0.f;

	/** Alto de la columna de luz (cm; 0 = sin columna). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup Glow", meta = (ClampMin = "0.0"))
	float BeamHeight = 360.f;

	/** El objeto sube, flota y gira (cerca de la cámara). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup Glow")
	bool bFloatAndSpin = true;

	/** Cuánto sube el objeto sobre su sitio (cm) y cuánto sube y baja al flotar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup Glow", meta = (ClampMin = "0.0"))
	float FloatLift = 14.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup Glow", meta = (ClampMin = "0.0"))
	float FloatBob = 6.f;

	/** Vueltas por segundo del objeto (el anillo gira más despacio, al revés). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup Glow", meta = (ClampMin = "0.0"))
	float SpinTurnsPerSecond = 0.2f;

	/** Luz (lúmenes; 0 = sin luz) y su alcance (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup Glow", meta = (ClampMin = "0.0"))
	float LightLumens = 420.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup Glow", meta = (ClampMin = "50.0"))
	float LightRadius = 380.f;

	/** Distancias (cm) a la cámara local hasta las que hay luz, chispitas y movimiento del objeto y del anillo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup Glow|Distance", meta = (ClampMin = "0.0"))
	float LightRange = 1800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup Glow|Distance", meta = (ClampMin = "0.0"))
	float SparkleRange = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup Glow|Distance", meta = (ClampMin = "0.0"))
	float AnimRange = 4000.f;

	/** Distancias (cm) hasta las que se dibujan el anillo y la columna (más allá, nada). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup Glow|Distance", meta = (ClampMin = "0.0"))
	float RingDrawDistance = 9000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup Glow|Distance", meta = (ClampMin = "0.0"))
	float BeamDrawDistance = 15000.f;

private:
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> RingComp;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> BeamComp;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> LightComp;

	UPROPERTY(EditAnywhere, Category = "Pickup Glow")
	bool bGlowEnabled = true;

	/** Lo que flota y gira, su altura y su giro de reposo. */
	TWeakObjectPtr<USceneComponent> FloatTarget;
	float RestZ = 0.f;
	FQuat RestRotation = FQuat::Identity;
	bool bHasRest = false;

	/** Suelo bajo el objeto (se busca al pararse) y su inclinación. */
	FVector GroundPoint = FVector::ZeroVector;
	FQuat GroundTilt = FQuat::Identity;
	bool bGrounded = false;

	FVector LastOwnerLocation = FVector::ZeroVector;
	/** Segundos quieto y cuánto se ve la marca (0-1: crece al pararse, se esconde al moverse). */
	float StillTime = 0.f;
	float Appear = 0.f;
	float Clock = 0.f;
	float AppliedLight = -1.f;
	float RingScaleBase = 1.f;
	int32 SparkleFx = INDEX_NONE;
	bool bVisualsBuilt = false;
	bool bNearView = false;

	bool HasScreen() const;
	void BuildVisuals();
	void HideVisuals();
	float ComputeRingRadius() const;
	void SnapToGround();
	void RestoreFloatTarget();
};
