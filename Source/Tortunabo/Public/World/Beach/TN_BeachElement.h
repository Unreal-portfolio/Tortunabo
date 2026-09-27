#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/Beach/TN_BeachTypes.h"
#include "TN_BeachElement.generated.h"

/**
 * Base de todo lo que el generador de la playa coloca (decorado, trampas y enemigos). El servidor la crea con
 * SpawnElement (clase por nombre, TNBeach::ClassNameOf) y replica Spec; cada máquina construye su malla en ApplySpec
 * (determinista con Spec.Seed). Las subclases sobrescriben ApplySpec y, si quieren, GetFootprintRadius.
 */
UCLASS(Abstract)
class TORTUNABO_API ATN_BeachElement : public AActor
{
	GENERATED_BODY()

public:
	ATN_BeachElement();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Servidor: crea el elemento Spec.Element (su clase, por nombre) en Transform y le pasa Spec antes de BeginPlay.
	 * Devuelve null si la clase aún no existe (se registra en el log y el generador sigue con el resto).
	 */
	static ATN_BeachElement* SpawnElement(UWorld* World, const FTransform& Transform, const FTNBeachElementSpec& InSpec);

	const FTNBeachElementSpec& GetSpec() const { return Spec; }

	/** Radio de la huella en planta (cm): por defecto, la nominal del contrato por SizeScale. */
	virtual float GetFootprintRadius() const;

protected:
	virtual void BeginPlay() override;

	/** Construye lo visual y la colisión a partir de Spec (en todas las máquinas; puede llamarse más de una vez). */
	virtual void ApplySpec() {}

	UPROPERTY(ReplicatedUsing = OnRep_Spec, BlueprintReadOnly, Category = "Beach")
	FTNBeachElementSpec Spec;

	UFUNCTION()
	void OnRep_Spec();

	bool bSpecApplied = false;
};
