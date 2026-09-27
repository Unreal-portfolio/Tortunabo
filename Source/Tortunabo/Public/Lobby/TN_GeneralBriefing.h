#pragma once

#include "CoreMinimal.h"
#include "World/TN_DirectInteractableBase.h"
#include "Core/TN_CosmeticsTypes.h"
#include "TN_GeneralBriefing.generated.h"

class UAnimationAsset;
class UBoxComponent;
class UCapsuleComponent;
class UMaterialInterface;
class UPointLightComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * El general del cuartel: el General Galápago (una tortuga grande con gorra de capitán) detrás de una mesa de madera
 * con una maqueta del castillo de arena del lobby hecha en código (murallas, torres, puerta, huevos, tienda, botellas y
 * banderitas). Al hablar con él se abre la sesión informativa (UTN_BriefingWidget) en el cliente que interactúa:
 * «Cómo se juega», «Modos de juego», «Reglas» y «Controles» (con las teclas reales de Enhanced Input).
 *
 * Como el tendero: se gira hacia el jugador local cuando se acerca y le saluda (efecto local). ATN_HQGameMode lo coloca
 * donde está el general de la maqueta (y esconde esa tortuga y su mesa) si el nivel no tiene uno puesto a mano.
 * Mira a su +X: la mesa queda delante.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_GeneralBriefing : public ATN_DirectInteractableBase
{
	GENERATED_BODY()

public:
	ATN_GeneralBriefing();

	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostRegisterAllComponents() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Delante de la mesa: desde ahí se habla con el general. */
	virtual FVector GetInteractionPoint() const override;

	FText GetGeneralName() const { return GeneralName; }
	FText GetHeadquartersName() const { return HeadquartersName; }

protected:
	virtual void OnInteracted_Implementation(APawn* Interactor) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "General")
	TObjectPtr<USkeletalMeshComponent> General;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "General")
	TObjectPtr<UStaticMeshComponent> GeneralHat;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "General")
	TObjectPtr<UCapsuleComponent> GeneralBlock;

	/** Mesa, maqueta del castillo, mástil con bandera y cartel (malla en ejecución). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "General")
	TObjectPtr<UStaticMeshComponent> Table;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "General")
	TObjectPtr<UBoxComponent> TableBlock;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "General")
	TObjectPtr<UTextRenderComponent> Sign;

	/** Pone el nombre del cuartel en el cartel y lo encoge para que quepa en el tablero. */
	void FitSignText();

	/** Luz del farol colgado dentro de la tienda militar. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "General")
	TObjectPtr<UPointLightComponent> TentLight;

	/** Lo que lleva puesto el general (filas de DT_Helmets y DT_Skins). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "General")
	FTN_TurtleLook GeneralLook;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "General")
	FText GeneralName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "General")
	FText HeadquartersName;

	/** Escala del general (el jugador lleva 2,5). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "General")
	float GeneralScale = 3.4f;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> GeneralDefaults;

	UPROPERTY(Transient)
	TObjectPtr<UAnimationAsset> IdleAnim;

	UPROPERTY(Transient)
	TObjectPtr<UAnimationAsset> SaluteAnim;

	float SaluteCooldown = 0.f;
	float SaluteTimeLeft = -1.f;
	float LookYaw = 0.f;

	void BuildTable();
	/** Esconde el general de la maqueta y su mesa («Boolean») en cada máquina. */
	void HideBlockout();
};
