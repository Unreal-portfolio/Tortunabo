#pragma once

#include "CoreMinimal.h"
#include "World/TN_DirectInteractableBase.h"
#include "TN_ChangingBooth.generated.h"

class UCameraComponent;
class UBoxComponent;
class UTextRenderComponent;

/**
 * Probador del lobby: media botella de cristal de mar puesta boca abajo (el culo hace de techo) con el tapón, una
 * chapa de corona roja, como puerta redonda.
 *
 * Al entrar (interactuar), el servidor mete a la tortuga dentro y cierra la puerta (Occupant replicado: todos la ven
 * cerrarse y la botella se menea mientras se cambia); el cliente aleja la cámara a la de la botella y abre el
 * probador (UTN_BoothWidget), donde solo salen los cosméticos que tiene. Al aceptar o cancelar, ReleaseOccupant abre
 * la puerta y la tortuga sale de un saltito con el conjunto puesto (los cosméticos se replican por el PlayerState).
 *
 * ATN_HQGameMode la coloca sola en las botellas de la maqueta si el nivel no tiene ninguna puesta a mano.
 * La puerta mira a su +X.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_ChangingBooth : public ATN_DirectInteractableBase
{
	GENERATED_BODY()

public:
	ATN_ChangingBooth();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract(APawn* Interactor) const override;

	/** Servidor: abre la puerta y saca a la tortuga de un saltito (si es la que está dentro). */
	void ReleaseOccupant(APawn* Pawn);

	APawn* GetOccupant() const { return Occupant; }

protected:
	virtual void OnInteracted_Implementation(APawn* Interactor) override;

	/** Botella, marco de la puerta, corcho y cuerda (malla en ejecución). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Booth")
	TObjectPtr<UStaticMeshComponent> Bottle;

	/** Bisagra: gira la puerta redonda. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Booth")
	TObjectPtr<USceneComponent> DoorHinge;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Booth")
	TObjectPtr<UStaticMeshComponent> Door;

	/** Paredes (octógono de cuatro cajas): nadie la atraviesa (la tortuga de dentro la ignora mientras se cambia). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Booth")
	TArray<TObjectPtr<UBoxComponent>> Walls;

	/** Cámara alejada que ve el que se cambia (se usa como objetivo de vista). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Booth")
	TObjectPtr<UCameraComponent> ViewCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Booth")
	TObjectPtr<UTextRenderComponent> Label;

	/** Tortuga que se está cambiando (nullptr = libre y con la puerta abierta). */
	UPROPERTY(ReplicatedUsing = OnRep_Occupant, BlueprintReadOnly, Category = "Booth")
	TObjectPtr<APawn> Occupant;

	UFUNCTION()
	void OnRep_Occupant();

private:
	/** 1 abierta, 0 cerrada (se anima hacia la de Occupant). */
	float DoorOpen = 1.f;
	/** Meneo de la botella al cerrarse y mientras alguien se cambia. */
	float WobbleTime = 0.f;
	float ShuffleClock = 0.f;
	/** Quien estaba dentro (para sacarlo de un saltito cuando Occupant pasa a nullptr). */
	TWeakObjectPtr<APawn> PreviousOccupant;

	void BuildMeshes();
	void HideBlockout();
};
