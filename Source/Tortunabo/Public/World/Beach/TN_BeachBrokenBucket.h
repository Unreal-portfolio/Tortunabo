#pragma once

#include "CoreMinimal.h"
#include "World/Beach/TN_BeachElement.h"
#include "TN_BeachBrokenBucket.generated.h"

class UProceduralMeshComponent;
class UStaticMeshComponent;

/**
 * Cubo de playa enorme tumbado y roto que se atraviesa como un túnel, a lo largo de X (X local = sentido de la carrera):
 * se entra por la boca (lado -X, con una rampa de arena que sube al suelo de dentro) y se sale por la grieta del culo
 * (lado +X: un agujero dentado de ~74 % del radio y una lengua de arena que baja). Con SizeScale = 1 mide 504 de largo,
 * 238 de radio en la boca y 182 en el culo (un cubo de juguete de 18 cm a escala 28), con paredes de 26; dentro hay un
 * suelo de arena plano a 0,38 del radio del culo (se pasa de pie: ~2,5 m de altura libre en la salida). Se ajusta a la
 * huella (450·SizeScale, entre 0,7 y 1,15 veces esas medidas).
 *
 * Colores de juguete desteñidos por el sol (según la semilla), nervios y reborde en la boca, el asa caída sobre el lomo y
 * grietas oscuras que salen del agujero. Estático: sin Tick ni red más allá del Spec; colisión convexa por tramos (pared
 * en 16 tramos, culo en 12, suelo y rampas).
 */
UCLASS()
class TORTUNABO_API ATN_BeachBrokenBucket : public ATN_BeachElement
{
	GENERATED_BODY()

public:
	ATN_BeachBrokenBucket();

protected:
	virtual void ApplySpec() override;

	UPROPERTY(VisibleAnywhere, Category = "Cubo")
	TObjectPtr<UStaticMeshComponent> BucketMesh;

	UPROPERTY(VisibleAnywhere, Category = "Cubo")
	TObjectPtr<UProceduralMeshComponent> BucketCollision;
};
