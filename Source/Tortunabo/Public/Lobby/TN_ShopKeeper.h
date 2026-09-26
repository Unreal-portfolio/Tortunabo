#pragma once

#include "CoreMinimal.h"
#include "World/TN_DirectInteractableBase.h"
#include "Core/TN_CosmeticsTypes.h"
#include "TN_ShopKeeper.generated.h"

class UAnimationAsset;
class UBoxComponent;
class UCapsuleComponent;
class UMaterialInterface;
class USkeletalMeshComponent;
class UTextRenderComponent;

/**
 * Tienda del lobby: Don Tortugo, el tendero (una tortuga grande con su propio conjunto), detrás de un mostrador con
 * toldo de rayas y cartel. Al hablar con él se abre la tienda (UTN_ShopWidget) en el cliente que interactúa.
 *
 * Cuando el jugador local se acerca, se gira hacia él y le saluda (efecto local de cada cliente). ATN_HQGameMode la
 * coloca sola donde está el tendero de la maqueta si el nivel no tiene una puesta a mano (ver SpawnLobbyShops).
 * Mira a su +X: el mostrador queda delante.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_ShopKeeper : public ATN_DirectInteractableBase
{
	GENERATED_BODY()

public:
	ATN_ShopKeeper();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	FText GetKeeperName() const { return KeeperName; }
	FText GetShopName() const { return ShopName; }

protected:
	virtual void OnInteracted_Implementation(APawn* Interactor) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop")
	TObjectPtr<USkeletalMeshComponent> Keeper;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop")
	TObjectPtr<UStaticMeshComponent> KeeperHat;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop")
	TObjectPtr<UCapsuleComponent> KeeperBlock;

	/** Mostrador, postes, toldo y cartel (malla en ejecución). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop")
	TObjectPtr<UStaticMeshComponent> Stall;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop")
	TObjectPtr<UBoxComponent> CounterBlock;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop")
	TObjectPtr<UTextRenderComponent> Sign;

	/** Lo que lleva puesto el tendero (filas de DT_Helmets y DT_Skins). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	FTN_TurtleLook KeeperLook;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	FText KeeperName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	FText ShopName;

	/** Escala del tendero (el jugador lleva 2.5). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	float KeeperScale = 3.4f;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> KeeperDefaults;

	UPROPERTY(Transient)
	TObjectPtr<UAnimationAsset> IdleAnim;

	UPROPERTY(Transient)
	TObjectPtr<UAnimationAsset> WaveAnim;

	float WaveCooldown = 0.f;
	float WaveTimeLeft = -1.f;
	float LookYaw = 0.f;

	void BuildStall();
	void HideBlockoutKeeper();
};
