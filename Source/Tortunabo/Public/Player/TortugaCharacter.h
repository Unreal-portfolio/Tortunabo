#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "TortugaCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UInputMappingContext;
class UInputAction;
class UTN_InventoryComponent;
class ATN_InteractableBase;

UCLASS()
class TORTUNABO_API ATortugaCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ATortugaCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PawnClientRestart() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> InteractAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> RotateInventoryAction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UTN_InventoryComponent> InventoryComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float InteractionScanInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float MaxInteractionDistance = 350.f;

private:
	void CacheInputAssets();
	void ApplyInputMappingIfLocal();

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> LoadedMappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LoadedMoveAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LoadedLookAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LoadedJumpAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LoadedInteractAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LoadedRotateInventoryAction;

	TWeakObjectPtr<ATN_InteractableBase> FocusedInteractable;
	FTimerHandle InteractionScanTimerHandle;
	bool bInputAssetsLoaded = false;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void TryInteract();
	void RotateInventory();
	void UpdateFocusedInteractable();
	void ResolveInteractionViewPoint(FVector& OutLocation, FVector& OutDirection) const;

	UFUNCTION(Server, Reliable)
	void ServerTryInteract(ATN_InteractableBase* Interactable);
};

