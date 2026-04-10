#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "TimerManager.h"
#include "Core/TN_CosmeticsTypes.h"
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
class UStaticMeshComponent;
class UTN_EmoteWheelDataAsset;
struct FTN_EmoteWheelEntry;

UCLASS()
class TORTUNABO_API ATortugaCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ATortugaCharacter();

	/** Índice de emote reservado para el knockdown visual.
	 *  Cuando el servidor aplica knockdown, establece ReplicatedEmoteIndex = KNOCKDOWN_EMOTE_ID.
	 *  El sistema de emotes maneja toda la replicación visual automáticamente.
	 */
	static constexpr int32 KNOCKDOWN_EMOTE_ID = 100;

	/**
	 * Re-aplica el Enhanced Input Mapping Context localmente.
	 * Llamar desde el servidor para el listen-server tras un revival, un tick después
	 * de Possess + ClientRestart, para garantizar que el input queda activo.
	 */
	void ReapplyInputMapping();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PawnClientRestart() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/**
	 * Called when the PlayerState reference is replicated to this client.
	 * Used to apply cosmetics (helmet) in case they arrived BEFORE the pawn was
	 * possessed and the BeginPlay timer-for-next-tick already fired without them.
	 */
	virtual void OnRep_PlayerState() override;

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

	/**
	 * Mesh del casco equipado. Se adjunta al SceneComponent "Sombrero" en BeginPlay.
	 * Añade un SceneComponent hijo en BP_TortugaCharacter con nombre exacto "Sombrero"
	 * y colócalo sobre la cabeza de la tortuga.
	 * Si no existe "Sombrero", el casco se adjunta al root del personaje.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cosmetics")
	TObjectPtr<UStaticMeshComponent> HelmetMeshComp;

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

	// ── Air Dash (double-jump) ────────────────────────────────────────────────
	/** Horizontal velocity applied on air dash (cm/s). Overrides current XY velocity. */
	UPROPERTY(EditDefaultsOnly, Category = "Movement|AirDash", meta = (ClampMin = "0.0"))
	float AirDashHorizontalForce = 1400.f;

	/** Vertical velocity applied on air dash (cm/s). Overrides current Z velocity. */
	UPROPERTY(EditDefaultsOnly, Category = "Movement|AirDash", meta = (ClampMin = "0.0"))
	float AirDashVerticalBoost = 300.f;

	// ── Emote Config ─────────────────────────────────────────────────────────
	/** Seconds to smoothly interpolate all limbs back to rest after an emote ends. */
	UPROPERTY(EditDefaultsOnly, Category = "Emotes", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EmoteBlendOutDuration = 0.25f;

	/** Catálogo editable de emotes para la rueda radial y validación de cooldown/IDs. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Emotes|Data")
	TObjectPtr<UTN_EmoteWheelDataAsset> EmoteWheelDataAsset;

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
	float ThrowUpAngleDeg = 15.f;

	// ── Camera Cinematic Settings (AAA) ───────────────────────────────────────

	/** Longitud del brazo en reposo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Cinematic", meta = (ClampMin = "50", ClampMax = "1200"))
	float CameraArmLengthDefault = 170.f;

	/** Longitud del brazo cuando el jugador está esprintando. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Cinematic", meta = (ClampMin = "50", ClampMax = "1200"))
	float CameraArmLengthSprint = 240.f;

	/** Velocidad de interpolación de la longitud del brazo (mayor = más rápido). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Cinematic", meta = (ClampMin = "0.5", ClampMax = "20.0"))
	float CameraArmLengthInterpSpeed = 6.f;

	/** Campo de visión (FOV) de la cámara en reposo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Cinematic", meta = (ClampMin = "40.0", ClampMax = "120.0"))
	float CameraFOVDefault = 72.f;

	/** Campo de visión (FOV) de la cámara al esprintar (ligeramente mayor para sensación de velocidad). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Cinematic", meta = (ClampMin = "40.0", ClampMax = "120.0"))
	float CameraFOVSprint = 82.f;

	/** Velocidad de interpolación del FOV. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Cinematic", meta = (ClampMin = "0.5", ClampMax = "20.0"))
	float CameraFOVInterpSpeed = 5.f;

	/**
	 * Lag de posición del spring arm (qué tan fluido sigue a la cápsula).
	 * 6-10 = cinematic suave. 20+ = casi sin lag.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Cinematic", meta = (ClampMin = "1.0", ClampMax = "30.0"))
	float CameraPositionLagSpeed = 14.f;

	/** Lag de rotación del spring arm. 12-16 = respuesta rápida pero suavizada. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Cinematic", meta = (ClampMin = "1.0", ClampMax = "30.0"))
	float CameraRotationLagSpeed = 20.f;

	/**
	 * Offset del socket de la cámara respecto al pivot del spring arm.
	 * X = adelante/atrás, Y = derecha (over-the-shoulder), Z = arriba.
	 * (0, 80, 70) → over-the-shoulder derecho ajustado, estilo God of War.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Cinematic")
	FVector CameraSocketOffset = FVector(0.f, 80.f, 70.f);

	/**
	 * Offset relativo del pivot del spring arm en espacio del personaje (eleva el pivot).
	 * (0, 0, 55) → pivot en zona del tronco/hombros de la tortuga.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Cinematic")
	FVector CameraBoomRelativeOffset = FVector(0.f, 0.f, 55.f);

	/**
	 * Inclina la cámara hacia abajo respecto al spring arm (grados, valor negativo = abajo).
	 * -14°: cámara más picada sobre el personaje, estilo God of War.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Cinematic", meta = (ClampMin = "-30.0", ClampMax = "0.0"))
	float CameraAimPitchOffset = -14.f;

	/** Si true, el eje Y del ratón (arriba/abajo) se invierte. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Input")
	bool bInvertCameraY = false;

	/**
	 * Multiplicador de sensibilidad para el eje X (izquierda/derecha — Yaw).
	 * 1.0 = por defecto.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Input", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float LookSensitivityX = 1.0f;

	/**
	 * Multiplicador de sensibilidad para el eje Y (arriba/abajo — Pitch).
	 * 0.5 = la mitad que el X para evitar mareo. Ajusta al gusto.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Input", meta = (ClampMin = "0.1", ClampMax = "5.0"))
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

	/** Cached SceneComponent "Sombrero" found in BeginPlay. Helmet mesh attaches to it. */
	TWeakObjectPtr<USceneComponent> SombreroSocket;

	/** Timer handle for repeating cosmetic application retry in BeginPlay. */
	FTimerHandle CosmeticRetryTimerHandle;
	int32 CosmeticRetryCount = 0;

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
	/** Snapshot of KnockdownVisualComp rotation — captured when knockdown emote blend-out begins. */
	FRotator SnapshotKnockdownComp;
	bool     bKnockdownCompSnapshotValid = false;

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
	FVector  CabezaRestScale = FVector::OneVector; // Escala original de Cabeza (para restaurar tras BigHead)

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

	/**
	 * Materiales originales de cada StaticMeshComponent del cuerpo, cacheados en BeginPlay.
	 * Usados para restaurar el aspecto por defecto cuando se desequipa un skin (NAME_None).
	 * Transient: se recalcula cada vez que spawnea el pawn.
	 */
	TMap<TWeakObjectPtr<UStaticMeshComponent>, TArray<TObjectPtr<UMaterialInterface>>> DefaultBodyMaterials;

	// ── Leg animation state (cosmetic, local-only, never replicated) ──────────
	float LegPhaseAccumulator    = 0.f;   // cycles [0,1)
	float LegAmplitudeMultiplier = 0.f;   // [0,1] fade envelope

	TWeakObjectPtr<USceneComponent> Pata1;
	TWeakObjectPtr<USceneComponent> Pata2;
	FRotator Pata1RestRot = FRotator::ZeroRotator;
	FRotator Pata2RestRot = FRotator::ZeroRotator;

	// ── Air Dash internals ────────────────────────────────────────────────────
	/** True after landing; false after the first air dash of a jump. Not replicated — tracked per-machine. */
	bool bCanAirDash = true;

	virtual void Jump() override;
	virtual void Landed(const FHitResult& Hit) override;
	void PerformAirDashLocally();

	UFUNCTION(Server, Reliable)
	void ServerPerformAirDash();

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
	void CancelEmoteLocalOnly();
	/** Internal: start emote animation on this machine (no ownership check). */
	void StartEmoteLocally(int32 Index);
	/** Cancel the active emote and begin a smooth blend-out back to rest. */
	void CancelEmote();
	/** Per-frame emote tick: advances animation and drives component rotations. */
	void TickEmote(float DeltaTime);
	const FTN_EmoteWheelEntry* ResolveWheelEmoteEntry(int32 EmoteID) const;
	void PlayWheelEmoteMontage(int32 EmoteID);
	void StopWheelEmoteMontage(int32 EmoteID, float BlendOutTime);

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

	UFUNCTION(Client, Reliable)
	void ClientRejectEmote(int32 Index);

	UFUNCTION(Server, Reliable)
	void ServerTryInteract(ATN_InteractableBase* Interactable);

	UFUNCTION(Server, Reliable)
	void ServerUseEquippedItem();

	UFUNCTION(Server, Reliable)
	void ServerDropEquippedItem();

	void ApplyKnockdownVisual(bool bKnocked);

	/**
	 * Multicast RPC fiable — garantiza que TODOS los clientes reciban
	 * el cambio de knockdown inmediatamente, sin depender solo de OnRep.
	 */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastApplyKnockdownVisual(bool bKnocked);

	UFUNCTION()
	void OnRep_IsKnockedDown();

	UFUNCTION()
	void OnRep_IsDead();

	UFUNCTION()
	void OnRep_bBigHead();

	/** Escala la Cabeza al BigHeadScale en reposo o la restaura. */
	void ApplyBigHeadVisual(bool bBig);

	/**
	 * Multicast fiable: fuerza el visual de muerte en todos los clientes.
	 */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastSetDeadVisual(bool bDead);

	/** Oculta extremidades, cabeza, cola y casco (solo visual). */
	void HideLimbs();

	/** Restaura la visibilidad de extremidades, cabeza, cola y casco. */
	void ShowLimbs();

	/**
	 * Server RPC: el jugador interactuando intenta revivir a un cadáver cercano.
	 * Busca el TortugaCharacter muerto más cercano y llama RevivePlayer del GameMode.
	 */
	UFUNCTION(Server, Reliable)
	void ServerTryReviveNearby();

	// ── Revive channeling (server-driven) ────────────────────────────────────
	/** Try to start reviving a nearby DBNO player. Called from ServerSetEmote when emote starts. */
	void TryStartReviveChannel();
	/** Cancel any active revive channel. Called when emote ends, player moves, or conditions fail. */
	void CancelReviveChannel();
	/** Tick the revive channel (server timer, 0.1s). Checks proximity + conditions. */
	void TickReviveChannel();

	TWeakObjectPtr<APlayerController> ReviveTargetPC;
	float ReviveChannelElapsed = 0.f;
	FTimerHandle ReviveChannelTimerHandle;

	// ── DBNO/Revive Audio (private) ─────────────────────────────────────────

	/** Audio component for revive channel sound (spatialized, on the reviver). Lazy-init. */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ReviveAudioComponent;

	/** Audio component for DBNO heartbeat (non-spatialized, local player only). Lazy-init. */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> DBNOAudioComponent;

	/** Create ReviveAudioComponent if it doesn't exist (spatialized, proximity-attenuated). */
	UAudioComponent* EnsureReviveAudioComponent();
	/** Create DBNOAudioComponent if it doesn't exist (non-spatialized, local-only). */
	UAudioComponent* EnsureDBNOAudioComponent();

	void PlayReviveChannelSound();
	void StopReviveChannelSound();
	void PlayReviveSuccessSound();
	void PlayDBNOHeartbeatSound();
	void StopDBNOHeartbeatSound();

	/** Callback for ReviveAudioComponent: re-loops revive channel sound while channeling. */
	UFUNCTION()
	void OnReviveAudioFinished();

	/** Callback for DBNOAudioComponent: re-loops heartbeat while DBNO. */
	UFUNCTION()
	void OnDBNOAudioFinished();

protected:
	// ── Emote replication ────────────────────────────────────────────────────
	/** Emote index replicado a todos los clientes. -1 = sin emote. */
	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedEmoteIndex)
	int32 ReplicatedEmoteIndex = -1;

	// ── Knockdown state ──────────────────────────────────────────────────────
	/**
	 * Estado replicado de knockdown. true → mesh tiltado 180°, movimiento bloqueado.
	 * BlueprintReadOnly en protected para que BPs hijos puedan leerlo (ej. para UI).
	 */
	UPROPERTY(ReplicatedUsing = OnRep_IsKnockedDown, BlueprintReadOnly, Category = "Knockdown")
	bool bIsKnockedDown = false;

	/**
	 * Nombre del componente visual a rotar durante knockdown.
	 * Debe coincidir con el nombre de un SceneComponent en el Blueprint (ej. "Cuerpo").
	 * Si está vacío o no se encuentra, se usa el fallback automático (primer StaticMesh).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Knockdown")
	FName KnockdownComponentName = TEXT("Cuerpo");

	/**
	 * Estado replicado de "muerte visual". true → extremidades/cabeza/cola/casco ocultos.
	 * El pawn NO se destruye: queda como cadáver interactuable para revive.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_IsDead, BlueprintReadOnly, Category = "Death")
	bool bIsDead = false;

	FTimerHandle KnockdownTimerHandle;

	// ── Big Head consumable ──────────────────────────────────────────────────
	/** true mientras el efecto de cabeza grande está activo. Replicado para que todos los clientes lo vean. */
	UPROPERTY(ReplicatedUsing = OnRep_bBigHead, BlueprintReadOnly, Category = "BigHead")
	bool bBigHead = false;

	/** Factor de escala de la cabeza cuando el efecto está activo (multiplicador sobre la escala en reposo). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "BigHead", meta = (ClampMin = "1.5", ClampMax = "10.0"))
	float BigHeadScale = 3.5f;

	/** Duración en segundos del efecto de cabeza grande. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "BigHead", meta = (ClampMin = "1.0"))
	float BigHeadDurationSeconds = 8.f;

	FTimerHandle BigHeadTimerHandle;

	/** Rotación relativa del mesh al spawnear (guardada en BeginPlay para restaurarla). */
	FRotator MeshDefaultRelativeRotation = FRotator::ZeroRotator;

	/** Componente visual para el tilt de knockdown (SkeletalMesh o StaticMesh blockout). */
	TWeakObjectPtr<USceneComponent> KnockdownVisualComp;


public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Aplica knockdown a este personaje durante Duration segundos.
	 * Solo tiene efecto si se llama en el servidor (HasAuthority).
	 * Accesible desde TN_ThrowableItemActor y cualquier otro actor de gameplay.
	 */
	UFUNCTION(BlueprintCallable, Category = "Knockdown")
	void ApplyKnockdown(float Duration);

	/** Recover from knockdown immediately (server-only). Used by RunGameMode::RevivePlayer. */
	UFUNCTION(BlueprintCallable, Category = "Knockdown")
	void RecoverFromKnockdown();

	/** Returns true if this character is currently in a knockdown/DBNO state. */
	UFUNCTION(BlueprintPure, Category = "Knockdown")
	bool IsKnockedDown() const { return bIsKnockedDown; }

	/**
	 * Activa/desactiva el visual de muerte: oculta extremidades, cola, cabeza, casco.
	 * El pawn permanece en el mundo como cadáver interactuable.
	 * Solo llamar desde el servidor — replica via OnRep + Multicast.
	 */
	UFUNCTION(BlueprintCallable, Category = "Death")
	void SetDeadVisual(bool bDead);

	UFUNCTION(BlueprintCallable, Category = "Emotes")
	void RequestWheelEmote(uint8 EmoteID);

	UFUNCTION(BlueprintCallable, Category = "Knockdown")
	void GrantInfiniteStamina(float DurationSeconds);

	/**
	 * Actualiza el mesh del casco en el socket "Sombrero" del personaje.
	 * Llamado desde TN_CoopPlayerState::OnRep_EquippedHelmetId (clientes)
	 * y desde MP_GamePlayerController::ServerSetEquippedHelmet (servidor/listen-server).
	 * HelmetId == NAME_None → oculta el casco.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	void UpdateHelmetMesh(FName HelmetId);

	/**
	 * Aplica el skin de personaje indicado.
	 * Busca FTN_SkinData en DT_Skins (vía MP_GameInstance::GetSkinDataTable) y aplica
	 * el BodyMaterial en todos los componentes del cuerpo (Cuerpo, Pata1, Pata2, Cola, Cabeza).
	 * SkinId == NAME_None → restaura los materiales por defecto del BP.
	 * Implementable en BP para personalizar el comportamiento visual.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	void UpdateSkinVisual(FName SkinId);

	// ── Revive system (DBNO) ─────────────────────────────────────────────────

	/** Radius in cm within which a teammate can revive a DBNO player. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "DBNO", meta = (ClampMin = "50.0"))
	float ReviveRadiusCm = 300.f;

	/** Seconds required to channel a revive (must stay in range and keep emoting). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "DBNO", meta = (ClampMin = "0.5"))
	float ReviveDurationSeconds = 3.f;

	/**
	 * True while this character is actively channeling a revive on a DBNO teammate.
	 * Replicated for HUD visualization on all clients.
	 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "DBNO")
	bool bIsReviving = false;

	/**
	 * Revive channel progress [0..1]. Owner-only replication for the reviver's HUD.
	 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "DBNO")
	float ReviveProgress = 0.f;

	// ── DBNO/Revive Audio ────────────────────────────────────────────────────

	/**
	 * Sound played in loop on the REVIVER while channeling a revive.
	 * Proximity-attenuated so nearby players hear it.
	 * Assign in Blueprint Class Defaults. Leave null for silence.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "DBNO|Audio")
	TObjectPtr<USoundBase> ReviveChannelSound;

	/**
	 * One-shot sound played on the REVIVED player when revive completes.
	 * Proximity-attenuated — all nearby players hear the success cue.
	 * Assign in Blueprint Class Defaults. Leave null for silence.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "DBNO|Audio")
	TObjectPtr<USoundBase> ReviveSuccessSound;

	/**
	 * Looped sound played locally on the DBNO player (heartbeat / tension).
	 * Only the incapacitated player hears this (non-spatialized, local only).
	 * Assign in Blueprint Class Defaults. Leave null for silence.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "DBNO|Audio")
	TObjectPtr<USoundBase> DBNOHeartbeatSound;

	/** Inner radius (cm) for DBNO/revive audio attenuation (full volume). */
	UPROPERTY(EditDefaultsOnly, Category = "DBNO|Audio", meta = (ClampMin = "0.0"))
	float ReviveAudioInnerRadius = 300.f;

	/** Outer radius (cm) for DBNO/revive audio attenuation (silence). */
	UPROPERTY(EditDefaultsOnly, Category = "DBNO|Audio", meta = (ClampMin = "0.0"))
	float ReviveAudioOuterRadius = 2500.f;

private:
	bool IsValidWheelEmoteId(int32 EmoteID) const;
	float GetWheelEmoteCooldown(int32 EmoteID) const;
};
