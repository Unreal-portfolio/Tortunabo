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
class UPostProcessComponent;
class UTN_EmoteWheelDataAsset;
struct FTN_EmoteWheelEntry;

/**
 * @brief Personaje principal del jugador. Tortuga antropomórfica con cámara tercera persona, dive aéreo, knockdown, emotes y cosméticos.
 *
 * Concentra la mayoría de la lógica de gameplay del jugador:
 *  - Movimiento: Walk, sprint vía UTN_StaminaComponent, dive aéreo con momentum preservation.
 *  - Combate / hazards: knockdown con tilt 180° + ragdoll opcional, mareo, Big Head, sombrilla anti-tormenta.
 *  - Items: UTN_InventoryComponent con rotación equipado/guardado y consumo de Use Type.
 *  - Cosméticos: helmet (mesh attacheado a "Sombrero") y skin (materiales por slot del SkM).
 *  - Animación procedural: piernas vía pendulum inline + UTN_ProcAnimInstance para overrides component-space.
 *  - Emotes: catálogo replicado (incluido KNOCKDOWN_EMOTE_ID=100 que dispara el visual de tumbado).
 *  - VOIP: UProximityVoiceComponent attached como child component.
 *  - Cámara: SpringArm con lag, zoom dinámico de sprint, FOV interpolado.
 *  - Input: Enhanced Input con soft refs a IMC_Player + IA_*. Se carga en BeginPlay.
 */
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

	/**
	 * Cancela el efecto Big Head activo (si lo hay): restaura tamaño de cabeza,
	 * limpia el timer y marca bBigHead = false.
	 * Disparado en el servidor. Aplica el efecto de mareo en todas las máquinas (#2).
	 * Solo ejecutar en el servidor.
	 */
	void RemoveBigHeadEffect();

	/**
	 * Aplica el efecto de mareo (ralentización + feedback visual) en todas las máquinas.
	 * Llamado automáticamente desde RemoveBigHeadEffect.
	 * También disponible para otros sistemas que quieran causar mareo (#2).
	 */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastApplyMareoEffect(float Duration);

	/** Devuelve true si el efecto Big Head está activo en este momento. */
	bool HasBigHeadActive() const { return bBigHead; }

	/** Devuelve true si la protección de sombrilla está activa (#29). */
	bool HasUmbrellaProtection() const { return bHasUmbrellaProtection; }

	/** Activa/desactiva la protección de sombrilla. Solo llamar desde el servidor. */
	void SetUmbrellaProtection(bool bActive) { bHasUmbrellaProtection = bActive; }

	/** Devuelve el componente de inventario (acceso de solo lectura para sistemas externos). */
	UTN_InventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

	/**
	 * Aplica el efecto de tinta de calamar (#13) en la máquina local del jugador afectado.
	 * Muestra un overlay de material sobre la cámara durante Duration segundos.
	 * Solo surte efecto en la máquina que controla localmente este personaje (IsLocallyControlled).
	 */
	void ApplyInkEffect(float Duration);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PawnClientRestart() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void Jump() override;
	virtual void OnJumped_Implementation() override;

	/**
	 * @brief Llamado cuando la referencia al PlayerState se replica a este cliente.
	 *        Reaplica cosméticos (helmet/skin) en caso de que llegaran ANTES de que el pawn
	 *        fuera poseído y el timer-for-next-tick de BeginPlay ya hubiese disparado sin ellos.
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

	// ── Dive Config ───────────────────────────────────────────────────────────

	/** Horizontal impulse speed applied on dive (cm/s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Dive", meta=(ClampMin="200.0", ClampMax="3000.0"))
	float DiveForwardSpeed = 420.f;

	/**
	 * Eje de rotación del tilt del dive EN ESPACIO LOCAL DEL MESH (component frame).
	 * Default (1,0,0) = rota sobre X local — que con DiveMeshDefaultRot Yaw=90
	 * equivale a pitch lateral world (tortuga se inclina adelante).
	 * Probar (0,1,0) si rota sobre el eje frontal en vez del lateral (o viceversa).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Dive")
	FVector DiveTiltAxis = FVector(1.f, 0.f, 0.f);

	/** Downward component of the dive impulse (cm/s, ≥0 pushes toward ground). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Dive", meta=(ClampMin="0.0"))
	float DiveDownwardSpeed = 200.f;

	/** Speed threshold below which the dive recovery ends (cm/s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Dive", meta=(ClampMin="1.0"))
	float DiveStopSpeedThreshold = 80.f;

	/** Minimum lock duration after a dive, even if the character stops early (s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Dive", meta=(ClampMin="0.0"))
	float DiveMinLockDuration = 0.65f;

	/** How fast (1/s) the body tilts into/out of the dive pose (lerp speed). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Dive", meta=(ClampMin="1.0"))
	float DiveTiltSpeed = 12.f;

	/** Velocidad de rotación del actor Yaw hacia DiveDir al iniciar el dash (deg/seg).
	 *  720 → completa 180° en 250 ms. Subir = más responsivo (más cerca de snap).
	 *  Bajar = más fluido (puede no completar la rotación durante el dash). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Dive", meta=(ClampMin="60.0", ClampMax="3600.0"))
	float DiveYawInterpSpeed = 720.f;

	// ── Dive momentum preservation ─────────────────────────────────────────────
	// Captura velocity horizontal AL SALTAR. En el dash, compara cámara ACTUAL
	// contra dirección del salto. Bonus = JumpStartSpeed * Factor(alignment).
	// El bonus puede ser NEGATIVO (resta de la base) si miras opuesto al salto:
	//   alignment = +1 → cámara coincide con salto → Forward factor (positivo, suma)
	//   alignment =  0 → cámara lateral al salto    → Lateral factor (default 0, no efecto)
	//   alignment = -1 → cámara opuesta al salto    → Backward factor (negativo, resta)
	// Si Backward es lo bastante negativo, TotalSpeed sale negativo → DiveDir
	// invierte → el char "vuelve" hacia donde había saltado.

	/** Multiplicador cuando la cámara coincide con la dirección del salto. >0 suma. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Dive|Momentum", meta=(ClampMin="0.0", ClampMax="2.0"))
	float DiveMomentumForwardFactor = 1.0f;

	/** Multiplicador cuando la cámara está PERPENDICULAR al salto (90°). Default 0 = sin efecto. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Dive|Momentum", meta=(ClampMin="-1.0", ClampMax="2.0"))
	float DiveMomentumLateralFactor = 0.0f;

	/** Multiplicador cuando la cámara está OPUESTA al salto. Negativo = resta de la base. -0.5 hace que dashear atrás vaya un poco hacia el salto original. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Dive|Momentum", meta=(ClampMin="-2.0", ClampMax="0.0"))
	float DiveMomentumBackwardFactor = -0.5f;

	/** Cap absoluto del speed combinado |TotalSpeed| ≤ Max. Permite valores negativos (dash retroceso). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Dive|Momentum", meta=(ClampMin="200.0", ClampMax="5000.0"))
	float DiveMaxTotalSpeed = 1500.f;

	/** Velocity horizontal capturada en OnJumped (justo cuando el char salta).
	 *  Multi-safe: OnJumped se llama en todas las máquinas autoritativas. No
	 *  necesita replicar — cada máquina captura localmente al saltar y la usa
	 *  para el dash posterior (el server-side cálculo del dash lee la del server). */
	FVector JumpStartHorizontalVelocity = FVector::ZeroVector;

	/** Yaw target hacia el que el actor interpola al iniciar el dash. Replicado
	 *  para que clientes remotos (no owner) también interpolen suavemente al
	 *  recibir la dirección. */
	UPROPERTY(Replicated)
	float DiveTargetYaw = 0.f;

	/** Activo mientras la rotación del dash se está interpolando. Se desactiva
	 *  al alcanzar el target (ε ≤ 1°) o al terminar el dash. */
	UPROPERTY(Replicated)
	bool bDiveYawInterpActive = false;

	/**
	 * CapsuleHalfHeight while diving — shrinks the hitbox to match the horizontal pose.
	 * The capsule radius stays unchanged; halving the height makes it a flat disc shape.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Dive", meta=(ClampMin="15.0"))
	float DiveCapsuleHalfHeight = 35.f;

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

	// ── Arm Animation (blockout) ─────────────────────────────────────────────
	// Requires SceneComponents named "Brazo1" and "Brazo2" in the Blueprint.
	// Pivot must be at the SHOULDER joint.

	/** Resting offset from T-pose (degrees). Brazo1 applies -value, Brazo2 applies +value.
	 *  70° makes both arms hang down naturally from T-pose. */
	UPROPERTY(EditDefaultsOnly, Category = "Arm Animation", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float ArmRestAngleDeg = 70.f;

	/** Swing amplitude while walking (degrees). Matches style of leg animation. */
	UPROPERTY(EditDefaultsOnly, Category = "Arm Animation", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float ArmWalkAmplitudeDeg = 35.f;

	/** Swing amplitude while sprinting (degrees). */
	UPROPERTY(EditDefaultsOnly, Category = "Arm Animation", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float ArmSprintAmplitudeDeg = 65.f;

	// ── Head Animation ───────────────────────────────────────────────────────
	/** Yaw offset (°) que corrige la dirección en reposo del hueso Cabeza.
	 *  Incrementar si la cabeza mira a la izquierda en reposo; decrementar si mira a la derecha. */
	UPROPERTY(EditDefaultsOnly, Category = "Head Animation", meta = (ClampMin = "-180.0", ClampMax = "180.0"))
	float HeadRestYawDeg = 0.f;

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
	FVector LegSwingAxis = FVector(1.f, 0.f, 0.f);

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
	 * Eje de reposo/swing de los brazos en component space del SKM unificado.
	 * Corregido R_z(-90°) respecto al blockout: (0,-1,0) = -Y → sube/baja visto de frente.
	 * Ajustar en BP si el binding pose del nuevo SKM requiere otro valor.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Emotes")
	FVector ArmSwingAxis = FVector(0.f, -1.f, 0.f);

	/** Up/down wag axis for Cola in component space. R_z(-90°) corrected: (1,0,0) = X. */
	UPROPERTY(EditDefaultsOnly, Category = "Emotes")
	FVector TailUpDownAxis = FVector(1.f, 0.f, 0.f);

	/** Side-to-side wag axis for Cola in component space. Z is invariant: (0,0,1). */
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
	 * CAM-01 floor clamp: estado interno del lift actual aplicado al boom relative
	 * Z para evitar clipping contra el suelo. Interpola suavemente hacia el valor
	 * deseado calculado cada tick en TickCameraInterp.
	 */
	float CameraFloorLiftCurrent = 0.f;

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

	/** Socket name on the Skeletal Mesh where the helmet attaches. */
	UPROPERTY(EditDefaultsOnly, Category = "Cosmetics")
	FName HelmetSocketName = TEXT("Sombrero");

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

	/** Bone names resolved from sockets in BeginPlay. Used by SetAnimBoneRot. */
	FName Brazo1Bone;
	FName Brazo2Bone;
	FName ColaBone;
	FName CabezaBone;
	FRotator Brazo1RestRot = FRotator::ZeroRotator;
	FRotator Brazo2RestRot = FRotator::ZeroRotator;
	FRotator ColaRestRot   = FRotator::ZeroRotator;
	FRotator CabezaRestRot = FRotator::ZeroRotator;
	FVector  CabezaRestScale = FVector::OneVector;

	/**
	 * HEAD-NECK FOLLOW — workaround para skinning de la cara del cuello:
	 * los polígonos del mesh que están pesados al hueso del cuerpo/cuello se quedan
	 * fijos cuando la cabeza rota. Definir aquí el nombre del hueso padre/cuello
	 * en el skeleton (p.ej. "Cuerpo" o "Neck") y un ratio [0..1]: la cabeza rota
	 * 100% del yaw/pitch y este hueso rota RATIO× lo mismo → la piel del cuello
	 * se arrastra parcialmente y oculta la discontinuidad.
	 *
	 * Configurar en BP_TortugaCharacter → Class Defaults → Head Animation:
	 *  - NeckFollowBone: nombre exacto del hueso del cuello/tronco en el skeleton
	 *  - NeckFollowRatio: 0.3f por defecto; subir si la piel sigue pegada.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Head Animation")
	FName NeckFollowBone = NAME_None;

	UPROPERTY(EditDefaultsOnly, Category="Head Animation", meta=(ClampMin="0.0", ClampMax="1.0"))
	float NeckFollowRatio = 0.3f;

	/** Rest rot del NeckFollowBone capturada en BeginPlay. */
	FRotator NeckFollowRestRot = FRotator::ZeroRotator;

	/** Rest locations in component space (for SetLoc emotes). Re-derived in BeginPlay. */
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
	 * Materiales originales del SkeletalMesh unificado, cacheados en BeginPlay.
	 * Usados para restaurar el aspecto por defecto cuando se desequipa un skin (NAME_None).
	 * Transient: se recalcula cada vez que spawnea el pawn.
	 */
	TArray<TObjectPtr<UMaterialInterface>> DefaultSkelMeshMaterials;

	// ── Leg animation state (cosmetic, local-only, never replicated) ──────────
	float LegPhaseAccumulator    = 0.f;   // cycles [0,1)
	float LegAmplitudeMultiplier = 0.f;   // [0,1] fade envelope

	FName Pata1Bone;
	FName Pata2Bone;
	FRotator Pata1RestRot = FRotator::ZeroRotator;
	FRotator Pata2RestRot = FRotator::ZeroRotator;

	// ── Air Dash internals ────────────────────────────────────────────────────
	/** True after landing; false after the first air dash of a jump. Not replicated — tracked per-machine. */
	bool bCanAirDash = true;

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
	void ApplyLegAngle(FName BoneName, const FRotator& RestRot, float AngleDeg) const;
	void ApplyArmAngle(FName BoneName, const FRotator& RestRot, float RestOffsetDeg, float SwingDeg) const;
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
	/** Apply a single-axis rotation additively on top of a bone's rest rotation. */
	void ApplyEmoteAngle(FName BoneName, const FRotator& Rest, float AngleDeg, const FVector& Axis) const;
	/** Apply two-axis compound rotation on a bone. */
	void ApplyEmoteAngles2(FName BoneName, const FRotator& Rest,
	                       float A1, const FVector& Ax1,
	                       float A2, const FVector& Ax2) const;
	void ApplyEmoteAngles3(FName BoneName, const FRotator& Rest,
	                       float A1, const FVector& Ax1,
	                       float A2, const FVector& Ax2,
	                       float A3, const FVector& Ax3) const;

	/** Set a bone's rotation in component space. No-op if bone is NAME_None or mesh is null. */
	void SetAnimBoneRot(FName BoneName, const FRotator& Rot) const;
	/** Get a bone's current rotation in component space. */
	FRotator GetAnimBoneRot(FName BoneName) const;
	/** Set a bone's location in component space. */
	void SetAnimBoneLoc(FName BoneName, const FVector& Loc) const;
	/** Get a bone's current location in component space. */
	FVector GetAnimBoneLoc(FName BoneName) const;
	/** Set a bone's scale in component space. Pass FVector::OneVector to clear override. */
	void SetAnimBoneScale(FName BoneName, const FVector& Scale) const;
	// Per-emote input handlers (one-liners, bound in SetupPlayerInputComponent)
	void OnEmote0(); void OnEmote1(); void OnEmote2(); void OnEmote3(); void OnEmote4();
	void OnEmote5(); void OnEmote6(); void OnEmote7(); void OnEmote8(); void OnEmote9();

	/** Server RPC: set emote index on the replicated property so all clients see it. */
	UFUNCTION(Server, Reliable, WithValidation)
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
	/**
	 * Multicast con la pos suelo final como parámetro. Pattern del Codex DualMax
	 * round 3: el dato del transform inicial viaja CON el RPC, no en canal
	 * separado vía bReplicateMovement. Sin esto, el cliente recibe el MC antes
	 * que la replicación de SetActorLocation server (UE replica RPCs antes que
	 * propiedades en el mismo bunch) → arranca sim en pos vieja → divergencia.
	 */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastSetDeadVisual(bool bDead, FVector GroundLocation);

	/**
	 * Patrón canónico Epic para activar ragdoll sin que el cuerpo "salga lanzado":
	 *  1. StopAllMontages + bPauseAnims=true (CRÍTICO: evita que AnimBP pelee con el solver)
	 *  2. Capsule NoCollision
	 *  3. CMC StopMovementImmediately + DisableMovement + tick off
	 *  4. Line trace al suelo (evita penetración inicial → impulso de resolución)
	 *  5. SetCollisionProfileName("Ragdoll") en SkM
	 *  6. bBlendPhysics=true (CRÍTICO: la pose anim deja de dominar los bones)
	 *  7. SetSimulatePhysics(true) + SetAllBodiesSimulatePhysics(true)
	 *  8. SetAllPhysicsLinearVelocity(0) defensivo (post-simulate, no antes)
	 *  9. WakeAllRigidBodies
	 * Llamado en server (SetDeadVisual) y cliente (MulticastSetDeadVisual, OnRep_IsDead).
	 */
	void EnterRagdollState();

	/** Reverso de EnterRagdollState: detiene simulación y restaura state vivo. */
	void ExitRagdollState();

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
	 * Nombre del componente visual a rotar durante knockdown (Ruta B: tilt manual).
	 * NAME_None → usa GetMesh() directamente (correcto con mesh único sin SCs adicionales).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Knockdown")
	FName KnockdownComponentName = NAME_None;

	/**
	 * Multiplicador aplicado a la velocidad horizontal del jugador en el momento del knockdown.
	 * 1.0 = conserva el momentum actual, 0.0 = caída vertical, >1.0 = amplifica el impulso.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Knockdown", meta = (ClampMin = "0.0", ClampMax = "3.0"))
	float KnockdownHorizontalMultiplier = 1.0f;

	/**
	 * Fuerza hacia abajo (cm/s, valor positivo → se aplica en -Z) añadida al momentum de knockdown.
	 * Asegura que el personaje caiga aunque tenga velocidad vertical neutra o positiva.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Knockdown", meta = (ClampMin = "0.0", ClampMax = "1500.0"))
	float KnockdownDownwardForce = 400.f;

	/**
	 * Umbral de velocidad horizontal (cm/s) bajo el cual el CMC se bloquea al
	 * aterrizar durante knockdown. Sobre este umbral, el slide continúa y la
	 * fricción del CMC lo va frenando de forma natural (resbalón del plátano).
	 * Bajarlo a 0 = desliza hasta parar completamente. Subirlo = stop más brusco.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Knockdown", meta = (ClampMin = "0.0"))
	float KnockdownGroundLockSpeed = 50.f;

	/**
	 * Si true y el SkelMesh (GetMesh) tiene PhysicsAsset asignado, el knockdown
	 * activa ragdoll físico completo (`SetSimulatePhysics(true)`) en lugar del
	 * tilt de -180° manual. El ragdoll se desactiva en RecoverFromKnockdown y
	 * el mesh se re-attachea al capsule + restaura pose por defecto.
	 *
	 * También controla el ragdoll de muerte en SetDeadVisual.
	 *
	 * Si el BP no tiene PhysicsAsset, se cae silenciosamente al tilt tradicional
	 * — ningún cambio visible respecto al comportamiento previo.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Knockdown")
	bool bUsePhysicsRagdoll = true;

	/**
	 * Perfil de colisión aplicado al SkelMesh durante ragdoll. "Ragdoll" es el
	 * preset estándar de UE que permite físicas pero ignora al pawn. Cambiar
	 * sólo si el proyecto define perfiles custom.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Knockdown")
	FName RagdollCollisionProfile = TEXT("Ragdoll");

	/**
	 * Hueso de referencia para el tracking de posición durante el ragdoll de muerte.
	 * El servidor mueve el actor a este hueso cada tick → bReplicateMovement envía
	 * la posición real del cuerpo mientras cae, no el punto de muerte estancado.
	 * "pelvis" es el valor correcto para el esqueleto Mannequin/UE5 estándar.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Knockdown")
	FName RagdollTrackingBone = TEXT("pelvis");

	/**
	 * Small upward lift applied before death ragdoll starts. Prevents low leg
	 * bodies from spawning already penetrated into the floor.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Death", meta = (ClampMin = "0.0", ClampMax = "160.0"))
	float DeathRagdollSpawnLift = 60.f;

	/** Extra clearance between the skeletal mesh lower bound and the floor at death ragdoll startup. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Death", meta = (ClampMin = "0.0", ClampMax = "80.0"))
	float DeathRagdollFloorClearance = 25.f;

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

	// ── Mareo (#2) ────────────────────────────────────────────────────────────

	/** Duración del efecto de mareo al expirar/cancelar el BigHead (segundos). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "BigHead|Mareo", meta = (ClampMin = "0.0"))
	float MareoDurationSeconds = 3.f;

	/**
	 * Velocidad máxima durante el mareo (cm/s). Por defecto ~55% de la velocidad base.
	 * 0 = sin penalización de velocidad.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "BigHead|Mareo", meta = (ClampMin = "0.0"))
	float MareoSpeedCap = 250.f;

	/**
	 * Evento de mareo disparado en el cliente local (usa para camera shake, VFX, audio).
	 * Duration = MareoDurationSeconds del servidor.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "BigHead|Mareo")
	void OnMareoEffect(float Duration);

	/**
	 * Llamado en TODAS las máquinas (servidor + clientes) tras aplicar el visual
	 * de muerte o resurrección. Úsalo en BP_TortugaCharacter para activar el raptor,
	 * VFX de muerte, audio, etc.
	 * @param bDead  true = jugador acaba de morir, false = fue revivido.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Death")
	void OnDeathVisualSet(bool bDead);

	FTimerHandle BigHeadTimerHandle;
	FTimerHandle MareoTimerHandle;
	FTimerHandle InkEffectTimerHandle;

	void ClearInkEffect();

	/** Llamado cuando expira el timer de mareo — restaura el speed cap de stamina.
	 *  Usa CreateUObject (no lambda) para que ClearAllTimersForObject lo cancele en EndPlay. */
	void ClearMareoSpeedCap();

	/** Rotación relativa del mesh al spawnear (guardada en BeginPlay para restaurarla). */
	FRotator MeshDefaultRelativeRotation = FRotator::ZeroRotator;

	/** Componente visual para el tilt de knockdown (SkeletalMesh o StaticMesh blockout). */
	TWeakObjectPtr<USceneComponent> KnockdownVisualComp;

	// ── Ragdoll knockdown state ─────────────────────────────────────────────
	/** Transform relativo del SkelMesh antes de activar ragdoll — para restaurar pose. */
	FTransform SnapshotSkelMeshRelTransform;
	/** Perfil de colisión del SkelMesh antes del ragdoll — para restaurar al recover. */
	FName SnapshotSkelMeshCollisionProfile = NAME_None;
	/** true mientras el ragdoll físico está activo (evita doble-activación y no-ops al recover). */
	bool bKnockdownRagdollActive = false;

	// ── Dive state ────────────────────────────────────────────────────────────

	/**
	 * true durante toda la fase de dive (vuelo + slide de recuperación).
	 * OnRep aplica el visual (tilt + capsule resize) en todos los clientes.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_IsDiving, BlueprintReadOnly, Category = "Dive")
	bool bIsDiving = false;

	// ── Sombrilla (#29) ───────────────────────────────────────────────────────

	/**
	 * true mientras la sombrilla está abierta y protege de la gaviota.
	 * La gaviota dinámica (TN_EnemySeagull) comprueba este flag antes de matar.
	 * Replicado para que todos los clientes puedan mostrar el estado visual.
	 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Umbrella")
	bool bHasUmbrellaProtection = false;

	// ── Tinta de calamar (#13) ────────────────────────────────────────────────

	/**
	 * Material de overlay de tinta. Asignar un material de post-process (Domain=PostProcess)
	 * en BP_TortugaCharacter. Solo se muestra en la máquina local del jugador afectado.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ink")
	TObjectPtr<UMaterialInterface> InkOverlayMaterial;

	/** Post-process local que aplica el overlay de tinta. bEnabled=false en reposo. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ink")
	TObjectPtr<UPostProcessComponent> InkPostProcess;

	// ── Head Look replication ─────────────────────────────────────────────────
	/** Yaw (°) de la cabeza relativo al cuerpo. Positivo = mira a la derecha. Replicado a clientes remotos. */
	UPROPERTY(Replicated)
	float ReplicatedHeadYaw   = 0.f;

	/** Pitch (°) de la cabeza. Positivo = mira hacia arriba. Replicado a clientes remotos. */
	UPROPERTY(Replicated)
	float ReplicatedHeadPitch = 0.f;

	/** Tiempo acumulado desde que comenzó el dive (para DiveMinLockDuration). */
	float DiveLockTimer = 0.f;

	/** Alpha del tilt del cuerpo [0 = reposo, 1 = pose completa de dive]. Cosmético, local. */
	float DiveTiltAlpha = 0.f;

	/** HalfHeight original de la cápsula (guardada en BeginPlay). */
	float DiveCapsuleOrigHalfHeight = 88.f;

	/** Rotación relativa por defecto de GetMesh() (guardada en BeginPlay). */
	FRotator DiveMeshDefaultRot = FRotator::ZeroRotator;

	// ── Jump Animation state (cosmetic, local-only) ───────────────────────────
	bool  bJumpAnimActive = false;
	float JumpAnimTime    = 0.f;

	// ── Head Look state (local + smoothing para clientes remotos) ─────────────
	float LocalHeadRelativeYaw = 0.f;   ///< calculado cada tick en el owner, nunca va a la red
	float LocalHeadPitch       = 0.f;
	float SmoothedHeadYaw      = 0.f;   ///< interpolado en clientes remotos hacia ReplicatedHead*
	float SmoothedHeadPitch    = 0.f;

	void TickHeadLook(float DeltaTime);
	void ApplyHeadLookToCabeza(float Yaw, float Pitch);

	UFUNCTION(Server, Unreliable, WithValidation)
	void ServerUpdateHeadRotation(float Yaw, float Pitch);

	void TryDive();

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_StartDive(FVector DiveDir);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnDiveVisual(bool bEnter);

	UFUNCTION()
	void OnRep_IsDiving();

	void EndDive();
	void TickDive(float DeltaTime);
	void TickJumpAnim(float DeltaTime);


public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Aplica knockdown a este personaje durante Duration segundos.
	 * Solo tiene efecto si se llama en el servidor (HasAuthority).
	 * Accesible desde TN_ThrowableItemActor y cualquier otro actor de gameplay.
	 */
	/**
	 * @param Duration      Segundos que dura el knockdown.
	 * @param ImpulseOverride Si != ZeroVector, usa este impulso en vez del momentum actual.
	 */
	UFUNCTION(BlueprintCallable, Category = "Knockdown")
	void ApplyKnockdown(float Duration, FVector ImpulseOverride = FVector::ZeroVector);

	/** Recover from knockdown immediately (server-only). Used by RunGameMode::RevivePlayer. */
	UFUNCTION(BlueprintCallable, Category = "Knockdown")
	void RecoverFromKnockdown();

	/** Returns true if this character is currently in a knockdown/DBNO state. */
	UFUNCTION(BlueprintPure, Category = "Knockdown")
	bool IsKnockedDown() const { return bIsKnockedDown; }

	/** Returns true while the character is diving or in the locked recovery slide. */
	UFUNCTION(BlueprintPure, Category = "Dive")
	bool IsDiving() const { return bIsDiving; }

	/**
	 * Activa/desactiva el visual de muerte: oculta extremidades, cola, cabeza, casco.
	 * El pawn permanece en el mundo como cadáver interactuable.
	 * Solo llamar desde el servidor — replica via OnRep + Multicast.
	 */
	/**
	 * Punto centralizado para matar a este personaje.
	 * Resuelve RunGameMode y PlayerController internamente.
	 * Los actores del mundo llaman esto en vez de acceder a GameMode directamente.
	 * Solo tiene efecto en el servidor (HasAuthority).
	 */
	UFUNCTION(BlueprintCallable, Category = "Death")
	void RequestKill(AActor* KillInstigator = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Death")
	void SetDeadVisual(bool bDead);

	// ── Tótem auto-revive ─────────────────────────────────────────────────────

	/**
	 * Sonido reproducido en todas las máquinas cuando el tótem del inventario
	 * te salva de morir. Arrastra tu SoundCue/Wave aquí — sin nodos BP.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Totem|Audio")
	TObjectPtr<USoundBase> TotemSelfReviveSound;

	/**
	 * VFX reproducido en todas las máquinas al auto-revivir con el tótem.
	 * Arrastra tu Particle System aquí — sin nodos BP.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Totem|FX")
	TObjectPtr<UParticleSystem> TotemSelfReviveVFX;

	/**
	 * Evento BP disparado en TODAS las máquinas cuando el tótem del inventario
	 * impide tu muerte. Usar para lógica BP extra (HUD, cámara shake, etc.).
	 * Audio/VFX básicos ya se reproducen solos.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Totem")
	void OnTotemAutoRevive();

	/** Notifica a todos los clientes del auto-revive (dispara OnTotemAutoRevive + Audio/VFX). */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_OnTotemAutoRevive();

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
	 * Busca FTN_SkinData en DT_Skins (vía MP_GameInstance::GetSkinDataTable) y mapea
	 * 5 materiales a los slots del SkM unificado:
	 *   0=Belly · 1=EyeShine · 2=EyesMouth · 3=Skin · 4=Shell.
	 * Slots con material null se mantienen con el default cacheado en BeginPlay.
	 * SkinId == NAME_None → restaura los materiales por defecto del BP.
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

	// ── SFX | Player Action Sounds ───────────────────────────────────────────
	// Sonidos del propio personaje (pasos, salto, knockdown, kill, pickup,
	// throw, consume). Se reproducen at-location en todas las máquinas vía
	// MulticastPlaySfx → cada cliente los oye en 3D según la atenuación que
	// traiga su SoundCue. Asignar en BP_TortugaCharacter Class Defaults.

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SFX|Player")
	TObjectPtr<USoundBase> FootstepSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SFX|Player", meta = (ClampMin = "0.05"))
	float FootstepWalkInterval = 0.45f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SFX|Player", meta = (ClampMin = "0.05"))
	float FootstepSprintInterval = 0.28f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SFX|Player", meta = (ClampMin = "0.0"))
	float FootstepMinGroundSpeed = 50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SFX|Player")
	TObjectPtr<USoundBase> JumpSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SFX|Player")
	TObjectPtr<USoundBase> KnockdownSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SFX|Player")
	TObjectPtr<USoundBase> KillSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SFX|Player")
	TObjectPtr<USoundBase> PickupSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SFX|Player")
	TObjectPtr<USoundBase> ThrowSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SFX|Player")
	TObjectPtr<USoundBase> ConsumeSound;

	/**
	 * Multicast: spawnea Sound at-location en todas las máquinas. Llamar SOLO
	 * desde el servidor. Usado por TN_InventoryComponent (pickup/consume) y
	 * por el propio Character (jump/knockdown/kill/throw).
	 */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlaySfx(USoundBase* Sound);

private:
	bool IsValidWheelEmoteId(int32 EmoteID) const;
	float GetWheelEmoteCooldown(int32 EmoteID) const;

	/** Spawn at-location del SFX en este actor (proxy local de MulticastPlaySfx). */
	void PlaySfxAtSelf(USoundBase* Sound) const;

	/** Tiempo restante hasta el siguiente paso (cosmetic, local-only, no replicado). */
	float FootstepCooldown = 0.f;
};
