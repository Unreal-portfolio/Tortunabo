#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "TimerManager.h"
#include "TortugaCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UInputMappingContext;
class UInputAction;
class UTN_InventoryComponent;
class UTN_StaminaComponent;
class ATN_InteractableBase;
class USceneComponent;

UCLASS()
class TORTUNABO_API ATortugaCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ATortugaCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
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

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> SprintAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> DropItemAction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UTN_InventoryComponent> InventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stamina")
	TObjectPtr<UTN_StaminaComponent> StaminaComponent;

	// ── Leg Animation (blockout) ──────────────────────────────────────────────
	// Add child SceneComponents named "Pata1" and "Pata2" in your Blueprint.
	// Set their origin at the HIP PIVOT (see setup guide below).

	/** Swing amplitude in degrees while walking. */
	UPROPERTY(EditDefaultsOnly, Category = "Leg Animation", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float LegWalkAmplitudeDeg = 60.f;

	/** Oscillation frequency (cycles/s) while walking. */
	UPROPERTY(EditDefaultsOnly, Category = "Leg Animation", meta = (ClampMin = "0.1"))
	float LegWalkFrequency = 2.0f;

	/** Swing amplitude in degrees while sprinting. */
	UPROPERTY(EditDefaultsOnly, Category = "Leg Animation", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float LegSprintAmplitudeDeg = 90.f;

	/** Oscillation frequency (cycles/s) while sprinting. */
	UPROPERTY(EditDefaultsOnly, Category = "Leg Animation", meta = (ClampMin = "0.1"))
	float LegSprintFrequency = 3.5f;

	/**
	 * Rotation axis in the component's LOCAL space for the pendulum swing.
	 * (0,1,0) = local Y → forward/back swing (default, works for side-mounted legs).
	 * (1,0,0) = local X → lateral swing.
	 * Change if your component's local axes differ.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Leg Animation")
	FVector LegSwingAxis = FVector(0.f, 1.f, 0.f);

	/** Speed (cm/s) below which legs smoothly return to rest. */
	UPROPERTY(EditDefaultsOnly, Category = "Leg Animation", meta = (ClampMin = "0.0"))
	float LegMinSpeed = 20.f;

	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float InteractionScanInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float MaxInteractionDistance = 350.f;

	UPROPERTY(EditDefaultsOnly, Category = "Interaction|Networking", meta = (ClampMin = "0.0"))
	float MaxLagCompensationDistance = 120.f;

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

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LoadedSprintAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LoadedDropItemAction;

	TWeakObjectPtr<ATN_InteractableBase> FocusedInteractable;
	FTimerHandle InteractionScanTimerHandle;
	bool bInputAssetsLoaded = false;
	FVector2D LastMovementInput = FVector2D::ZeroVector;
	bool bSprintHeld = false;

	// ── Leg animation state (cosmetic, local-only, never replicated) ──────────
	float LegPhaseAccumulator    = 0.f;   // cycles [0,1)
	float LegAmplitudeMultiplier = 0.f;   // [0,1] fade envelope

	TWeakObjectPtr<USceneComponent> Pata1;
	TWeakObjectPtr<USceneComponent> Pata2;
	FRotator Pata1RestRot = FRotator::ZeroRotator;
	FRotator Pata2RestRot = FRotator::ZeroRotator;

	void Move(const FInputActionValue& Value);
	void OnMoveReleased();
	void Look(const FInputActionValue& Value);
	void TryInteract();
	void RotateInventory();
	void StartSprint();
	void StopSprint();
	void DropEquippedItem();
	void TryUseEquippedItem();
	void RefreshSprintRequest();
	void UpdateFocusedInteractable();
	FVector GetItemSpawnLocation() const;
	FVector GetItemForwardDirection() const;

	void TickLegAnimation(float DeltaTime);
	void ApplyLegAngle(USceneComponent* Comp, const FRotator& RestRot, float AngleDeg) const;
	USceneComponent* FindChildByName(FName Name) const;
	/** Trace descendente para encontrar el suelo bajo WorldLocation. Usado al soltar y aterrizar ítems. */
	FVector FindGroundBelow(const FVector& WorldLocation) const;

	UFUNCTION(Server, Reliable)
	void ServerTryInteract(ATN_InteractableBase* Interactable);

	UFUNCTION(Server, Reliable)
	void ServerUseEquippedItem();

	UFUNCTION(Server, Reliable)
	void ServerDropEquippedItem();

	void ApplyKnockdownVisual(bool bKnocked);
	void RecoverFromKnockdown();

	UFUNCTION()
	void OnRep_IsKnockedDown();

protected:
	// ── Knockdown state ──────────────────────────────────────────────────────
	/**
	 * Estado replicado de knockdown. true → mesh tiltado 100°, movimiento bloqueado.
	 * BlueprintReadOnly en protected para que BPs hijos puedan leerlo (ej. para UI).
	 */
	UPROPERTY(ReplicatedUsing = OnRep_IsKnockedDown, BlueprintReadOnly, Category = "Knockdown")
	bool bIsKnockedDown = false;

	FTimerHandle KnockdownTimerHandle;

	/** Rotación relativa del mesh al spawnear (guardada en BeginPlay para restaurarla). */
	FRotator MeshDefaultRelativeRotation = FRotator::ZeroRotator;


public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Aplica knockdown a este personaje durante Duration segundos.
	 * Solo tiene efecto si se llama en el servidor (HasAuthority).
	 * Accesible desde TN_ThrowableItemActor y cualquier otro actor de gameplay.
	 */
	UFUNCTION(BlueprintCallable, Category = "Knockdown")
	void ApplyKnockdown(float Duration);

	UFUNCTION(BlueprintCallable, Category = "Stamina")
	void GrantInfiniteStamina(float DurationSeconds);
};



