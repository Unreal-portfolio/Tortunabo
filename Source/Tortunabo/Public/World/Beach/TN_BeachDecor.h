#pragma once

#include "CoreMinimal.h"
#include "World/Beach/TN_BeachElement.h"
#include "TN_BeachDecor.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;

/**
 * Decorado gigante de la playa (Docs/Modo_Carrera.md, «Decorado gigante»): todos los elementos de la categoría Decor
 * (cocos, medusas, rocas, castillos, basura...). En ApplySpec saca de la caché (una malla por elemento y variante,
 * compartida por todos los iguales, con su colisión simple en el BodySetup) la malla de Spec.Element, la coloca con la
 * semilla (giro, inclinación, hundimiento en la arena) y la escala a Spec.SizeScale: el actor no se escala. Lo que se
 * mueve (medusa, almeja, jirón de vela, red, banderitas) se anima en cada máquina y solo cerca de una cámara local.
 * La pasarela y el caminito de palos se montan con piezas instanciadas a lo largo de Spec.Extent (eje X del actor,
 * centrado en su origen); su alto no se escala (se sube a la pasarela y se pasa por debajo de la cuerda).
 */
UCLASS()
class TORTUNABO_API ATN_BeachDecor : public ATN_BeachElement
{
	GENERATED_BODY()

public:
	ATN_BeachDecor();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void ApplySpec() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Malla fija, con la colisión de la pieza. */
	UPROPERTY(VisibleAnywhere, Category = "Beach")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** Parte que se mueve (sin colisión), colgada de BodyMesh en su pivote. */
	UPROPERTY(VisibleAnywhere, Category = "Beach")
	TObjectPtr<UStaticMeshComponent> AnimMesh;

	/** Piezas instanciadas de la pasarela y del caminito de palos (una por malla). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Tiles;

	void BuildSingle(ETNBeachElement Element, float Size);
	void BuildTiles(ETNBeachElement Element, float Size);
	void ClearTiles();

	/** Empieza a mirar (cada medio segundo) si hay una cámara cerca para mover la parte animada. */
	void StartAnimation(uint8 Kind, const FVector& Axis, float Amp, float Rate, float Range);
	void StopAnimation();
	void UpdateAnimActivity();
	void ApplyAnimPose();
	/** Alguna tortuga encima o al lado (la almeja no se abre con alguien encima). */
	bool IsSomeoneOnTop() const;

	FTimerHandle AnimCheckTimer;
	uint8 AnimKind = 0;
	FVector AnimAxis = FVector(0.0, 1.0, 0.0);
	float AnimAmp = 0.f;
	float AnimRate = 1.f;
	float AnimPhase = 0.f;
	float AnimTime = 0.f;
	float AnimRange = 10000.f;
	int32 ClamCycle = -1;
	bool bClamHeld = false;
};
