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
class UAudioComponent;
class USoundBase;

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

	/**
	 * Input actions for emotes 0–9.  The array must have exactly 10 elements.
	 * Assign IA_Emote0…IA_Emote9 here or override in your Blueprint Class Defaults.
	 * Keys 0–9 (numrow) should be mapped to each action in IMC_Player.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Emotes")
	TArray<TSoftObjectPtr<UInputAction>> EmoteActions;

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

	// ── Emote Config ─────────────────────────────────────────────────────────
	/** Seconds to smoothly interpolate all limbs back to rest after an emote ends. */
	UPROPERTY(EditDefaultsOnly, Category = "Emotes", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EmoteBlendOutDuration = 0.25f;

	/**
	 * Sound to play for each emote (index 0–9). Assign in Blueprint Class Defaults.
	 * Each element maps 1:1 to the emote at that index. Leave null for silent emotes.
	 * Sound loops while the emote is active; stops on cancel/blend-out.
	 * Uses proximity attenuation matching the voice chat range.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Emotes|Audio")
	TArray<TObjectPtr<USoundBase>> EmoteSounds;

	/** Inner radius (cm) for emote audio — full volume inside this range. Matches voice chat default. */
	UPROPERTY(EditDefaultsOnly, Category = "Emotes|Audio", meta = (ClampMin = "0.0"))
	float EmoteAudioInnerRadius = 300.f;

	/** Outer radius (cm) for emote audio — silent beyond this range. Matches voice chat default. */
	UPROPERTY(EditDefaultsOnly, Category = "Emotes|Audio", meta = (ClampMin = "0.0"))
	float EmoteAudioOuterRadius = 2500.f;

	/**
	 * Eje primario de los brazos en espacio del PADRE (T-Pose).
	 * Con SceneComponents a rot (0,0,0) y brazos por ±Y:
	 *   X (1,0,0) rojo   = adelante → sube/baja visto de frente (+X=arriba)
	 *   Y (0,1,0) verde  = derecha  → roll sobre eje largo del brazo (casi invisible en cubos)
	 *   Z (0,0,1) azul   = arriba   → adelante/atrás (−Z=adelante, +Z=atrás)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Emotes")
	FVector ArmSwingAxis = FVector(1.f, 0.f, 0.f);

	/** Up/down wag axis for Cola in LOCAL space.  (0,1,0) = local Y → tail wags up/down. */
	UPROPERTY(EditDefaultsOnly, Category = "Emotes")
	FVector TailUpDownAxis = FVector(0.f, 1.f, 0.f);

	/** Side-to-side wag axis for Cola in LOCAL space.  (0,0,1) = local Z → tail wags left/right. */
	UPROPERTY(EditDefaultsOnly, Category = "Emotes")
	FVector TailSideAxis = FVector(0.f, 0.f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float InteractionScanInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float MaxInteractionDistance = 350.f;

	UPROPERTY(EditDefaultsOnly, Category = "Interaction|Networking", meta = (ClampMin = "0.0"))
	float MaxLagCompensationDistance = 120.f;

	/** Ángulo adicional hacia arriba (grados) al lanzar objetos, para que hagan arco parabólico. */
	UPROPERTY(EditDefaultsOnly, Category = "Throwable", meta = (ClampMin = "0.0", ClampMax = "60.0"))
	float ThrowUpAngleDeg = 35.f;

	// ── Camera Cinematic Settings (AAA) ───────────────────────────────────────

	/** Longitud del brazo en reposo. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera|Cinematic", meta = (ClampMin = "50", ClampMax = "1200"))
	float CameraArmLengthDefault = 350.f;

	/** Longitud del brazo cuando el jugador está esprintando. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera|Cinematic", meta = (ClampMin = "50", ClampMax = "1200"))
	float CameraArmLengthSprint = 480.f;

	/** Velocidad de interpolación de la longitud del brazo (mayor = más rápido). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera|Cinematic", meta = (ClampMin = "0.5", ClampMax = "20.0"))
	float CameraArmLengthInterpSpeed = 5.f;

	/** Campo de visión (FOV) de la cámara en reposo. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera|Cinematic", meta = (ClampMin = "40.0", ClampMax = "120.0"))
	float CameraFOVDefault = 80.f;

	/** Campo de visión (FOV) de la cámara al esprintar (ligeramente mayor para sensación de velocidad). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera|Cinematic", meta = (ClampMin = "40.0", ClampMax = "120.0"))
	float CameraFOVSprint = 90.f;

	/** Velocidad de interpolación del FOV. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera|Cinematic", meta = (ClampMin = "0.5", ClampMax = "20.0"))
	float CameraFOVInterpSpeed = 5.f;

	/**
	 * Lag de posición del spring arm (qué tan fluido sigue a la cápsula).
	 * 6-10 = cinematic suave. 20+ = casi sin lag.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera|Cinematic", meta = (ClampMin = "1.0", ClampMax = "30.0"))
	float CameraPositionLagSpeed = 8.f;

	/** Lag de rotación del spring arm. 12-16 = respuesta rápida pero suavizada. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera|Cinematic", meta = (ClampMin = "1.0", ClampMax = "30.0"))
	float CameraRotationLagSpeed = 14.f;

	/**
	 * Offset del socket de la cámara respecto al pivot del spring arm.
	 * X = adelante/atrás, Y = derecha (over-the-shoulder), Z = arriba.
	 * Default (0, 55, 65) → estilo over-the-shoulder derecho, elevada.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera|Cinematic")
	FVector CameraSocketOffset = FVector(0.f, 55.f, 65.f);

	/**
	 * Offset relativo del pivot del spring arm en espacio del personaje (eleva el pivot).
	 * Default (0, 0, 40) → eleva el pivot 40 cm por encima de la raíz.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera|Cinematic")
	FVector CameraBoomRelativeOffset = FVector(0.f, 0.f, 40.f);

	/** Si true, el eje Y del ratón (arriba/abajo) se invierte. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera|Input")
	bool bInvertCameraY = false;

	/**
	 * Multiplicador de sensibilidad para el eje X (izquierda/derecha — Yaw).
	 * 1.0 = por defecto.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera|Input", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float LookSensitivityX = 1.0f;

	/**
	 * Multiplicador de sensibilidad para el eje Y (arriba/abajo — Pitch).
	 * 0.5 = la mitad que el X para evitar mareo. Ajusta al gusto.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera|Input", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float LookSensitivityY = 0.5f;

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

	// ── Emote State ─────────────────────────────────────────────────────────
	int32 ActiveEmoteIndex   = -1;   ///< -1 = no emote active (local animation driver)
	float EmoteTime          =  0.f; ///< seconds since emote started
	bool  bEmoteBlendingOut  = false;
	float EmoteBlendOutTimer =  0.f;

	/** Snapshots of component rotations captured when a blend-out begins. */
	FRotator SnapshotBrazo1;
	FRotator SnapshotBrazo2;
	FRotator SnapshotPata1;
	FRotator SnapshotPata2;
	FRotator SnapshotCola;
	FRotator SnapshotCabeza;

	/** Snapshots of component locations captured when a blend-out begins. */
	FVector SnapshotBrazo1Loc;
	FVector SnapshotBrazo2Loc;
	FVector SnapshotPata1Loc;
	FVector SnapshotPata2Loc;
	FVector SnapshotColaLoc;
	FVector SnapshotCabezaLoc;

	/** Cached arm, tail, and head component refs (found by name in BeginPlay, same as Pata1/Pata2). */
	TWeakObjectPtr<USceneComponent> Brazo1;
	TWeakObjectPtr<USceneComponent> Brazo2;
	TWeakObjectPtr<USceneComponent> Cola;
	TWeakObjectPtr<USceneComponent> Cabeza;
	FRotator Brazo1RestRot = FRotator::ZeroRotator;
	FRotator Brazo2RestRot = FRotator::ZeroRotator;
	FRotator ColaRestRot   = FRotator::ZeroRotator;
	FRotator CabezaRestRot = FRotator::ZeroRotator;

	/** Rest locations for transform-based emotes (Palmada Potente, Modo Loco 2). */
	FVector Brazo1RestLoc = FVector::ZeroVector;
	FVector Brazo2RestLoc = FVector::ZeroVector;
	FVector Pata1RestLoc  = FVector::ZeroVector;
	FVector Pata2RestLoc  = FVector::ZeroVector;
	FVector ColaRestLoc   = FVector::ZeroVector;
	FVector CabezaRestLoc = FVector::ZeroVector;

	/** Loaded emote Input Actions (one per slot 0-9, Transient). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> LoadedEmoteActions;

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
	void TickCameraInterp(float DeltaTime);
	void ApplyLegAngle(USceneComponent* Comp, const FRotator& RestRot, float AngleDeg) const;
	USceneComponent* FindChildByName(FName Name) const;
	/** Trace descendente para encontrar el suelo bajo WorldLocation. Usado al soltar y aterrizar ítems. */
	FVector FindGroundBelow(const FVector& WorldLocation) const;

	// ── Emote system ─────────────────────────────────────────────────────────
	/** Start emote at index (0-9). Called from input — starts locally + replicates. */
	void TriggerEmote(int32 Index);
	/** Internal: start emote animation on this machine (no ownership check). */
	void StartEmoteLocally(int32 Index);
	/** Cancel the active emote and begin a smooth blend-out back to rest. */
	void CancelEmote();
	/** Per-frame emote tick: advances animation and drives component rotations. */
	void TickEmote(float DeltaTime);

	// ── Emote Audio ──────────────────────────────────────────────────────────
	/** Lazily-created audio component for emote sounds. Attached to root, proximity-attenuated. */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> EmoteAudioComponent;

	/** Start playing the sound for the given emote index (looped, proximity-attenuated). */
	void PlayEmoteSound(int32 Index);
	/** Stop any currently playing emote sound. */
	void StopEmoteSound();
	/** Callback: restarts emote audio when the clip ends, while the emote is still active. */
	UFUNCTION()
	void OnEmoteAudioFinished();
	/** Apply a single-axis rotation additively on top of a component's rest rotation. */
	void ApplyEmoteAngle(USceneComponent* Comp, const FRotator& Rest, float AngleDeg, const FVector& Axis) const;
	/** Apply two-axis compound rotation (e.g. tail spiralling during Fiesta). */
	void ApplyEmoteAngles2(USceneComponent* Comp, const FRotator& Rest,
	                       float A1, const FVector& Ax1,
	                       float A2, const FVector& Ax2) const;
	void ApplyEmoteAngles3(USceneComponent* Comp, const FRotator& Rest,
	                       float A1, const FVector& Ax1,
	                       float A2, const FVector& Ax2,
	                       float A3, const FVector& Ax3) const;
	// Per-emote input handlers (one-liners, bound in SetupPlayerInputComponent)
	void OnEmote0(); void OnEmote1(); void OnEmote2(); void OnEmote3(); void OnEmote4();
	void OnEmote5(); void OnEmote6(); void OnEmote7(); void OnEmote8(); void OnEmote9();

	/** Server RPC: set emote index on the replicated property so all clients see it. */
	UFUNCTION(Server, Reliable)
	void ServerSetEmote(int32 Index);

	/** OnRep: fired on remote clients when ReplicatedEmoteIndex changes. */
	UFUNCTION()
	void OnRep_ReplicatedEmoteIndex();

	UFUNCTION(Server, Reliable)
	void ServerTryInteract(ATN_InteractableBase* Interactable);

	UFUNCTION(Server, Reliable)
	void ServerUseEquippedItem();

	UFUNCTION(Server, Reliable)
	void ServerDropEquippedItem();

	void ApplyKnockdownVisual(bool bKnocked);
	void RecoverFromKnockdown();

	/**
	 * Multicast RPC fiable — garantiza que TODOS los clientes reciban
	 * el cambio de knockdown inmediatamente, sin depender solo de OnRep.
	 */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastApplyKnockdownVisual(bool bKnocked);

	UFUNCTION()
	void OnRep_IsKnockedDown();

protected:
	// ── Emote replication ────────────────────────────────────────────────────
	/** Emote index replicado a todos los clientes. -1 = sin emote. */
	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedEmoteIndex)
	int32 ReplicatedEmoteIndex = -1;

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



