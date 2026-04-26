#include "Player/TortugaCharacter.h"
#include "Player/MP_GamePlayerController.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "GameFramework/PlayerController.h"
#include "Components/PostProcessComponent.h"
#include "Player/TN_InventoryComponent.h"
#include "Player/TN_StaminaComponent.h"
#include "Player/TN_ProcAnimInstance.h"
#include "World/TN_InteractableBase.h"
#include "World/TN_PickupInteractableBase.h"
#include "World/TN_ThrowableItemActor.h"
#include "World/TN_ConchPickup.h"
#include "World/TN_InkProjectile.h"
#include "GameFramework/PlayerState.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/AudioComponent.h"
#include "Materials/MaterialInterface.h"
#include "Animation/AnimInstance.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Engine/DataTable.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "DrawDebugHelpers.h"
#include "Core/TN_CoopPlayerState.h"
#include "Core/TN_CosmeticsTypes.h"
#include "Game/TN_RunGameMode.h"
#include "Multiplayer/MP_GameInstance.h"
#include "UI/HUD/TN_EmoteWheelDataAsset.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"

// ── CVar de debug ─────────────────────────────────────────────────────────────
// Activar en consola con: TN.Debug.Interaction 1
// Desactivar con: TN.Debug.Interaction 0
static TAutoConsoleVariable<int32> CVarDebugInteraction(
	TEXT("TN.Debug.Interaction"),
	0,
	TEXT("1 = Draw debug lines/spheres para el raycast de interacción y logs detallados. 0 = off."),
	ECVF_Cheat);

ATortugaCharacter::ATortugaCharacter()
{
	PrimaryActorTick.bCanEverTick = true;   // needed for leg animation
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	SetNetUpdateFrequency(60.f);
	SetMinNetUpdateFrequency(30.f);
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 360.f, 0.f);
	GetCharacterMovement()->NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;

	// bEnablePhysicsInteraction habilita PushForceFactor/TouchForceFactor sobre rigid bodies
	// por contacto. 0.5 = empuje sutil suficiente para mover cajas/bolas en abierto pero
	// NO para tunnelearlas contra paredes estáticas (sandwich-through). Tunable en runtime
	// desde BP si se quiere iterar.
	GetCharacterMovement()->bEnablePhysicsInteraction = true;
	GetCharacterMovement()->PushForceFactor = 0.5f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 350.f;
	CameraBoom->bUsePawnControlRotation = true;

	// ── Colisión del spring arm con paredes ───────────────────────────────────
	// Usamos el comportamiento nativo del SpringArm (bDoCollisionTest) en lugar de
	// casts manuales: es O(1) por frame, se ejecuta dentro del componente y ya
	// gestiona interpolación de retracción/extensión. Explicitamos los campos
	// por si algún Class Default en BP los hubiera puesto a false.
	CameraBoom->bDoCollisionTest = true;
	CameraBoom->ProbeChannel     = ECC_Camera;  // todas las paredes bloquean ECC_Camera por defecto
	CameraBoom->ProbeSize        = 30.f;         // CAM-01 iter 2: 22 seguía clipando suelo al mirar arriba.
	                                             // 30 cubre la mayoría de casos. Complementado con un
	                                             // floor-clamp manual en Tick (ver ClampCameraAboveFloor).

	// ── Cinematic camera lag ──────────────────────────────────────────────────
	// Suaviza la posición de la cámara para un feel AAA fluido.
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 8.f;                 // match CameraPositionLagSpeed default

	// Suaviza la rotación de la cámara independientemente de la posición.
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraRotationLagSpeed = 14.f;        // match CameraRotationLagSpeed default

	// Distancia máxima que el lag puede acumular antes de hacer "snap".
	CameraBoom->CameraLagMaxDistance = 180.f;

	// Over-the-shoulder offset: ligeramente a la derecha y elevada.
	CameraBoom->SocketOffset = FVector(0.f, 55.f, 65.f);

	// Eleva el pivot del boom sobre la raíz del personaje (encima de la cabeza).
	CameraBoom->SetRelativeLocation(FVector(0.f, 0.f, 40.f));

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->FieldOfView = 80.f;                 // match CameraFOVDefault

	// Paths must match actual asset locations in Content/Blueprints/Gameplay/Controls/.
	// BP_TortugaCharacter can override these in Class Defaults.
	DefaultMappingContext = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(TEXT("/Game/Blueprints/Gameplay/Controls/IMC_Player.IMC_Player")));
	MoveAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Blueprints/Gameplay/Controls/IA_Move.IA_Move")));
	LookAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Blueprints/Gameplay/Controls/IA_Look.IA_Look")));
	JumpAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Blueprints/Gameplay/Controls/IA_Jump.IA_Jump")));
	InteractAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Blueprints/Gameplay/Controls/IA_Interact.IA_Interact")));
	RotateInventoryAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Blueprints/Gameplay/Controls/IA_RotateInventory.IA_RotateInventory")));
	SprintAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Blueprints/Gameplay/Controls/IA_Sprint.IA_Sprint")));
	DropItemAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Blueprints/Gameplay/Controls/IA_DropItem.IA_DropItem")));

	// Emote actions: configured in BP_TortugaCharacter Class Defaults.
	// Array vacío por defecto para que el editor permita añadir/borrar filas individualmente.

	InventoryComponent = CreateDefaultSubobject<UTN_InventoryComponent>(TEXT("InventoryComponent"));
	StaminaComponent = CreateDefaultSubobject<UTN_StaminaComponent>(TEXT("StaminaComponent"));

	// Casco cosmético: adjunto directamente a GetMesh() (SkeletalMeshComponent).
	// Al estar en el árbol del mesh, recibe el network smoothing del CMC → sin lag.
	// Sin mesh asignado → invisible hasta que se equipe un casco real.
	HelmetMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HelmetMesh"));
	HelmetMeshComp->SetupAttachment(GetMesh()); // IMPORTANTE: GetMesh, no RootComponent
	HelmetMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HelmetMeshComp->SetIsReplicated(false); // Solo cosmético, no necesita replicación
	HelmetMeshComp->SetHiddenInGame(true);

	// Emote sounds: assign in BP Class Defaults. Array vacío por defecto.

	// Overlay de tinta: PostProcess local, desactivado por defecto.
	// bUnbound=true → afecta toda la pantalla del cliente local.
	// Se activa solo en IsLocallyControlled() — los demás clientes nunca lo ven.
	InkPostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("InkPostProcess"));
	InkPostProcess->SetupAttachment(RootComponent);
	InkPostProcess->bEnabled = false;
	InkPostProcess->bUnbound = true;

	// Make capsule AND mesh invisible to camera traces → the spring arm won't collide
	// with other players. Each player's own pawn is already auto-ignored.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
}

void ATortugaCharacter::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Log, TEXT("[TortugaCharacter] BeginPlay '%s' — LocallyControlled=%s  HasAuthority=%s  Controller=%s"),
		*GetName(),
		IsLocallyControlled() ? TEXT("YES") : TEXT("NO"),
		HasAuthority() ? TEXT("YES") : TEXT("NO"),
		GetController() ? *GetController()->GetName() : TEXT("NULL"));

	CacheInputAssets();
	ApplyInputMappingIfLocal();

	if (IsLocallyControlled() && InteractionScanInterval > 0.f)
	{
		GetWorldTimerManager().SetTimer(InteractionScanTimerHandle, this, &ATortugaCharacter::UpdateFocusedInteractable, InteractionScanInterval, true);
		UE_LOG(LogTemp, Log, TEXT("[TortugaCharacter] Interaction scan timer started (interval=%.2fs)"), InteractionScanInterval);
	}

	// Force UTN_ProcAnimInstance regardless of any AnimBP set in Blueprint.
	// Must run before InitBone so the instance exists when the first Tick fires.
	if (GetMesh())
	{
		GetMesh()->SetAnimInstanceClass(UTN_ProcAnimInstance::StaticClass());

		// HQ-WARN-01 defensive: si BP defaults o seamless travel dejan el SkM en
		// modo simulate-physics, el primer SetActorLocation/AttachToComponent dispara
		// "Attempting to move a fully simulated skeletal mesh". Reset explícito aquí.
		if (GetMesh()->IsSimulatingPhysics())
		{
			GetMesh()->SetSimulatePhysics(false);
			GetMesh()->bPauseAnims = false;
			UE_LOG(LogTemp, Warning, TEXT("[TortugaCharacter] BeginPlay: reset stale physics on %s"), *GetName());
		}
	}

	// ── ROUND 3 · FORZAR valores críticos sobre cualquier override de BP ──
	// Los UPROPERTY EditDefaultsOnly permiten al BP grabar valores antiguos que
	// pisan el constructor. Reasignar aquí garantiza que los fixes aplican.
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->bEnablePhysicsInteraction = true;
		CMC->PushForceFactor           = 1.0f;  // 2.0 tunneleaba · 0.5 no movía · 1.0 middle ground
		CMC->TouchForceFactor          = 1.0f;
	}
	if (CameraBoom)
	{
		CameraBoom->ProbeSize        = 30.f;
		CameraBoom->ProbeChannel     = ECC_Camera;
		CameraBoom->bDoCollisionTest = true;
	}

	// ── ROUND 3 · DIAGNOSTIC LOGS ──
	// Si los fixes siguen sin aplicar, estos logs revelan el estado real en runtime.
	UE_LOG(LogTemp, Warning, TEXT("[Diagnostic] %s BeginPlay: PushForceFactor=%.2f ProbeSize=%.2f bUsePhysicsRagdoll=%s PhysicsAsset=%s"),
		*GetName(),
		GetCharacterMovement() ? GetCharacterMovement()->PushForceFactor : -1.f,
		CameraBoom ? CameraBoom->ProbeSize : -1.f,
		bUsePhysicsRagdoll ? TEXT("Y") : TEXT("N"),
		(GetMesh() && GetMesh()->GetPhysicsAsset()) ? TEXT("ASSIGNED") : TEXT("NULL"));

	// Resolve socket names to bone names on the Skeletal Mesh and capture rest poses.
	// Sockets must exist on the mesh with these exact names; angles will be tuned later.
	// Rest pose is read from the SOCKET transform (component-space), not directly from
	// the bone. This lets artists correct the rest orientation per-bone by rotating the
	// socket in the Skeleton Editor without recompiling.
	auto InitBone = [&](FName SocketName, FName& OutBone, FRotator& OutRestRot, FVector& OutRestLoc)
	{
		if (!GetMesh()) { return; }
		OutBone = GetMesh()->GetSocketBoneName(SocketName);
		if (OutBone != NAME_None)
		{
			const FTransform SocketTM = GetMesh()->GetSocketTransform(SocketName, RTS_Component);
			OutRestRot = SocketTM.Rotator();
			OutRestLoc = SocketTM.GetTranslation();
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[TortugaCharacter] Socket '%s' not found on mesh — animation for this bone disabled."), *SocketName.ToString());
		}
	};

	InitBone(TEXT("Pata1"),  Pata1Bone,  Pata1RestRot,  Pata1RestLoc);
	InitBone(TEXT("Pata2"),  Pata2Bone,  Pata2RestRot,  Pata2RestLoc);
	InitBone(TEXT("Brazo1"), Brazo1Bone, Brazo1RestRot, Brazo1RestLoc);
	InitBone(TEXT("Brazo2"), Brazo2Bone, Brazo2RestRot, Brazo2RestLoc);
	InitBone(TEXT("Cola"),   ColaBone,   ColaRestRot,   ColaRestLoc);
	InitBone(TEXT("Cabeza"), CabezaBone, CabezaRestRot, CabezaRestLoc);

	// NeckFollow: capturar rest rot del hueso del cuello/tronco (si está configurado)
	// para poder aplicar rotación parcial en ApplyHeadLookToCabeza.
	if (NeckFollowBone != NAME_None && GetMesh())
	{
		if (GetMesh()->GetBoneIndex(NeckFollowBone) != INDEX_NONE)
		{
			// Rest rot en component space usando socket transform (igual que InitBone).
			NeckFollowRestRot = GetMesh()->GetSocketTransform(NeckFollowBone, RTS_Component).Rotator();
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[TortugaCharacter] NeckFollowBone '%s' no existe en el skeleton — head-neck follow DESACTIVADO."),
				*NeckFollowBone.ToString());
		}
	}

	// Bone scale at rest is (1,1,1) in all UE5 skeletons — no query needed.
	CabezaRestScale = FVector::OneVector;

	// Log de diagnóstico: estado final de todos los huesos de emote
	UE_LOG(LogTemp, Log, TEXT("[TortugaCharacter] Resolved bones — Brazo1='%s' Brazo2='%s' Pata1='%s' Pata2='%s' Cola='%s' Cabeza='%s'"),
		*Brazo1Bone.ToString(), *Brazo2Bone.ToString(),
		*Pata1Bone.ToString(),  *Pata2Bone.ToString(),
		*ColaBone.ToString(),   *CabezaBone.ToString());

	// Network smoothing: con el mesh unificado GetMesh() es el único componente visual.
	// El CMC ya aplica smoothing a GetMesh() y sus hijos directamente — no se necesita
	// re-adjuntar nada. El HelmetMeshComp está adjunto al socket "Sombrero" en GetMesh().

	// Guardar la rotación por defecto del mesh para restaurarla tras knockdown.
	// 1) Buscar por nombre configurable (KnockdownComponentName).
	if (KnockdownComponentName != NAME_None)
	{
		if (USceneComponent* Named = FindChildByName(KnockdownComponentName))
		{
			KnockdownVisualComp = Named;
			UE_LOG(LogTemp, Log, TEXT("[TortugaCharacter] KnockdownVisualComp → '%s' (por KnockdownComponentName)"), *Named->GetName());
		}
	}
	// 2) Fallback: SkeletalMesh con asset.
	if (!KnockdownVisualComp.IsValid())
	{
		if (USkeletalMeshComponent* SkelMesh = GetMesh())
		{
			if (SkelMesh->GetSkeletalMeshAsset())
			{
				KnockdownVisualComp = SkelMesh;
			}
		}
	}
	if (!KnockdownVisualComp.IsValid())
	{
		// Blockout: buscar el primer StaticMeshComponent hijo (directo)
		for (UActorComponent* Comp : GetComponents())
		{
			if (UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Comp))
			{
				if (SMC != GetRootComponent() && SMC != HelmetMeshComp && SMC->GetStaticMesh())
				{
					KnockdownVisualComp = SMC;
					break;
				}
			}
		}
	}
	if (!KnockdownVisualComp.IsValid())
	{
		// Blockout con SceneComponents anidados: buscar recursivamente en hijos del root
		TArray<USceneComponent*> AllChildren;
		if (GetRootComponent())
		{
			GetRootComponent()->GetChildrenComponents(true, AllChildren);
		}
		for (USceneComponent* Child : AllChildren)
		{
			if (UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Child))
			{
				if (SMC != HelmetMeshComp && SMC->GetStaticMesh())
				{
					KnockdownVisualComp = SMC;
					break;
				}
			}
		}
	}
	// Último fallback: el SkeletalMesh aunque esté vacío (para que el tilt se guarde)
	if (!KnockdownVisualComp.IsValid())
	{
		KnockdownVisualComp = GetMesh();
	}

	if (KnockdownVisualComp.IsValid())
	{
		MeshDefaultRelativeRotation = KnockdownVisualComp->GetRelativeRotation();
	}

	// ── Dive: guardar rotaciones por defecto y HalfHeight de la cápsula ─────────
	DiveCapsuleOrigHalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	if (USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		DiveMeshDefaultRot = SkelMesh->GetRelativeRotation();
	}

	// ── Aplicar camera settings serializables ────────────────────────────────
	// Los valores UPROPERTY pueden haberse sobrescrito en el BP hijo → aplicarlos aquí.
	if (CameraBoom)
	{
		CameraBoom->TargetArmLength        = CameraArmLengthDefault;
		CameraBoom->CameraLagSpeed         = CameraPositionLagSpeed;
		CameraBoom->CameraRotationLagSpeed = CameraRotationLagSpeed;
		CameraBoom->SocketOffset           = CameraSocketOffset;
		CameraBoom->SetRelativeLocation(CameraBoomRelativeOffset);
	}
	if (FollowCamera)
	{
		FollowCamera->FieldOfView = CameraFOVDefault;
		// Inclinar la cámara ligeramente hacia abajo para bajar el punto de mira.
		// No afecta a Controller->GetControlRotation() — solo es cosmético en la cámara.
		FollowCamera->SetRelativeRotation(FRotator(CameraAimPitchOffset, 0.f, 0.f));
	}

	// ── Vincular inventario al sistema de stamina para el peso ────────────────
	// Solo en el servidor (donde la stamina se actualiza), pero linkear en todos
	// es inofensivo porque GetTotalCarriedWeight solo lee datos replicados.
	if (StaminaComponent && InventoryComponent)
	{
		StaminaComponent->SetInventoryComponent(InventoryComponent);
	}

	// ── Cosmetics: casco inicial ──────────────────────────────────────────────
	// Adjuntar HelmetMeshComp al socket del Skeletal Mesh. El socket debe existir
	// en el mesh con el nombre configurado en HelmetSocketName.
	if (HelmetMeshComp && GetMesh())
	{
		HelmetMeshComp->AttachToComponent(GetMesh(),
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			HelmetSocketName);
		UE_LOG(LogTemp, Log, TEXT("[TortugaCharacter] HelmetMeshComp adjunto al socket '%s'."), *HelmetSocketName.ToString());
	}

	// Cachear materiales originales del SKM unificado (slots 0-4).
	// Deben guardarse ANTES del timer para que UpdateSkinVisual(NAME_None) pueda restaurarlos.
	DefaultSkelMeshMaterials.Reset();
	if (USkeletalMeshComponent* SKM = GetMesh())
	{
		const int32 NumMats = SKM->GetNumMaterials();
		for (int32 i = 0; i < NumMats; ++i)
		{
			DefaultSkelMeshMaterials.Add(SKM->GetMaterial(i));
		}
		UE_LOG(LogTemp, Log, TEXT("[SKIN-DEBUG] '%s' cached %d default materials from SKM"),
			*GetName(), NumMats);
		for (int32 i = 0; i < DefaultSkelMeshMaterials.Num(); ++i)
		{
			UE_LOG(LogTemp, Verbose, TEXT("[SKIN-DEBUG]   slot %d: %s"),
				i,
				DefaultSkelMeshMaterials[i] ? *DefaultSkelMeshMaterials[i]->GetName() : TEXT("NULL"));
		}
	}

	// Restaurar cosméticos al (re)spawnar en el mapa.
	// En clientes, PlayerState puede llegar tarde → timer repetitivo que reintenta
	// cada 0.3s hasta éxito o 10 intentos (3s). Cubre pawns remotos cuyo
	// PlayerState no está disponible en el primer tick.
	CosmeticRetryCount = 0;
	GetWorldTimerManager().SetTimer(CosmeticRetryTimerHandle,
		[WeakThis = TWeakObjectPtr<ATortugaCharacter>(this)]()
		{
			if (!WeakThis.IsValid())
			{
				return;
			}
			if (const ATN_CoopPlayerState* TNPS = WeakThis->GetPlayerState<ATN_CoopPlayerState>())
			{
				WeakThis->UpdateHelmetMesh(TNPS->EquippedHelmetId);
				WeakThis->UpdateSkinVisual(TNPS->EquippedSkinId);
				WeakThis->GetWorldTimerManager().ClearTimer(WeakThis->CosmeticRetryTimerHandle);
				return;
			}
			if (++WeakThis->CosmeticRetryCount >= 10)
			{
				UE_LOG(LogTemp, Warning, TEXT("[TortugaCharacter] '%s' — cosméticos: agotados 10 reintentos sin PlayerState."),
					*WeakThis->GetName());
				WeakThis->GetWorldTimerManager().ClearTimer(WeakThis->CosmeticRetryTimerHandle);
			}
		}, 0.3f, true, 0.1f);  // first fire at 0.1s, repeat every 0.3s
}

void ATortugaCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// (Ragdoll: cada máquina simula físicas localmente con state idéntico.
	//  Sin replicate movement durante muerte — convergencia natural.)

	// ── Knockdown ground lock ─────────────────────────────────────────────
	// Si el personaje fue lanzado (PufferFish, banana) durante knockdown,
	// HandlePendingLaunch pone MOVE_Falling para que vuele por los aires.
	// Cuando aterriza, CMC pone MOVE_Walking. Re-desactivamos movimiento aquí
	// para que siga inmovilizado mientras dure el knockdown.
	//
	// IMPORTANTE: sólo lockeamos si la velocidad horizontal ya cayó por debajo
	// del umbral. Así un knockdown con deslizamiento (banana) sigue rodando por
	// el suelo hasta que la fricción del CMC lo frena de forma natural.
	// Sin este chequeo, DisableMovement se dispara en el primer tick de contacto
	// con el suelo y corta en seco el slide (bug Q4-03 residual).
	if (HasAuthority() && bIsKnockedDown)
	{
		if (UCharacterMovementComponent* MC = GetCharacterMovement())
		{
			if (MC->IsMovingOnGround() && MC->Velocity.Size2D() < KnockdownGroundLockSpeed)
			{
				MC->DisableMovement();
			}
		}
	}

	TickDive(DeltaTime);           // dive physics recovery + procedural animation
	TickJumpAnim(DeltaTime);       // jump procedural animation (suppressed during dive)
	TickEmote(DeltaTime);          // emote system (overrides leg anim when active)
	TickLegAnimation(DeltaTime);   // normal locomotion (suppressed during emotes/dive/jump)
	TickCameraInterp(DeltaTime);   // cinematic camera zoom/FOV interpolation
	TickHeadLook(DeltaTime);       // head tracks camera direction, replicated a todos los clientes
}

void ATortugaCharacter::TickLegAnimation(float DeltaTime)
{
	// Suppressed while an emote (or its blend-out) controls all 5 components.
	if (ActiveEmoteIndex >= 0 || bEmoteBlendingOut)
	{
		return;
	}

	// Suppressed during knockdown — the character is tipped over, legs shouldn't animate.
	if (bIsKnockedDown)
	{
		return;
	}

	// Suppressed during dive and jump anim — those systems drive the leg components.
	if (bIsDiving || DiveTiltAlpha > 0.f || bJumpAnimActive)
	{
		return;
	}

	// Bail out early if neither leg bone is available.
	if (Pata1Bone == NAME_None && Pata2Bone == NAME_None)
	{
		return;
	}

	// GetVelocity() is replicated by CharacterMovement — works on every machine.
	const float Speed = GetVelocity().Size2D();

	const bool bIsSprinting = StaminaComponent && StaminaComponent->IsSprinting();

	const float TargetAmplitude = bIsSprinting ? LegSprintAmplitudeDeg : LegWalkAmplitudeDeg;
	const float TargetFrequency = bIsSprinting ? LegSprintFrequency    : LegWalkFrequency;

	// Fade the amplitude envelope smoothly when starting/stopping movement.
	const float TargetMult = (Speed > LegMinSpeed) ? 1.f : 0.f;
	LegAmplitudeMultiplier = FMath::FInterpTo(LegAmplitudeMultiplier, TargetMult, DeltaTime, 8.f);

	// Advance phase only while the character is moving (avoids phase pop on stop/resume).
	if (Speed > LegMinSpeed)
	{
		LegPhaseAccumulator += TargetFrequency * DeltaTime;
		LegPhaseAccumulator  = FMath::Fmod(LegPhaseAccumulator, 1.f); // keep in [0,1)
	}

	// Current pendulum angle.
	const float Angle = TargetAmplitude * LegAmplitudeMultiplier
	                  * FMath::Sin(LegPhaseAccumulator * 2.f * PI);

	// Pata1 and Pata2 are 180° out of phase → diagonal trot gait.
	if (Pata1Bone != NAME_None) { ApplyLegAngle(Pata1Bone, Pata1RestRot,  Angle); }
	if (Pata2Bone != NAME_None) { ApplyLegAngle(Pata2Bone, Pata2RestRot, -Angle); }

	// ── Arm swing — same phase accumulator, contralateral to legs ────────────
	// Arms hang down at ArmRestAngleDeg from T-pose (applied in parent space via
	// ArmSwingAxis / ApplyArmAngle). Brazo1 swings opposite to Pata1 for natural gait.
	const float ArmAmplitude = bIsSprinting ? ArmSprintAmplitudeDeg : ArmWalkAmplitudeDeg;
	const float ArmAngle     = ArmAmplitude * LegAmplitudeMultiplier
	                         * FMath::Sin(LegPhaseAccumulator * 2.f * PI);

	// Brazo1 (right): -ArmRestAngleDeg → arm falls DOWN (+AX = up, so -AX = down).
	// Brazo2 (left, mirrored): +ArmRestAngleDeg → arm falls DOWN (-AX = up for left arm).
	// Swing uses AZ axis: +ArmAngle naturally pushes right arm BACK and left arm FORWARD
	// because they sit on opposite sides of the body (+Y vs -Y in T-pose).
	if (Brazo1Bone != NAME_None) { ApplyArmAngle(Brazo1Bone, Brazo1RestRot, -ArmRestAngleDeg, ArmAngle); }
	if (Brazo2Bone != NAME_None) { ApplyArmAngle(Brazo2Bone, Brazo2RestRot,  ArmRestAngleDeg, -ArmAngle); }
}

// ─────────────────────────────────────────────────────────────────────────────
// TickCameraInterp — Cinematic camera zoom & FOV interpolation (local only)
// ─────────────────────────────────────────────────────────────────────────────
void ATortugaCharacter::TickCameraInterp(float DeltaTime)
{
	// Solo aplica en el cliente local que controla este pawn.
	if (!IsLocallyControlled()) { return; }
	if (!CameraBoom || !FollowCamera) { return; }

	const bool bSprinting = StaminaComponent && StaminaComponent->IsSprinting();

	// ── Interpolación de longitud del brazo ───────────────────────────────────
	const float TargetArmLength = bSprinting ? CameraArmLengthSprint : CameraArmLengthDefault;
	CameraBoom->TargetArmLength = FMath::FInterpTo(
		CameraBoom->TargetArmLength,
		TargetArmLength,
		DeltaTime,
		CameraArmLengthInterpSpeed
	);

	// ── Interpolación de FOV ──────────────────────────────────────────────────
	const float TargetFOV = bSprinting ? CameraFOVSprint : CameraFOVDefault;
	FollowCamera->FieldOfView = FMath::FInterpTo(
		FollowCamera->FieldOfView,
		TargetFOV,
		DeltaTime,
		CameraFOVInterpSpeed
	);

	// ── Sync live lag speeds (por si se editan en runtime desde Blueprint) ────
	CameraBoom->CameraLagSpeed         = CameraPositionLagSpeed;
	CameraBoom->CameraRotationLagSpeed = CameraRotationLagSpeed;

	// ── CAM-01 floor clamp ────────────────────────────────────────────────────
	// Prueba ambos canales (ECC_Camera + ECC_Visibility) — si el suelo no bloquea
	// Camera en este proyecto, Visibility suele bloquear seguro.
	{
		const FVector CamWorld = FollowCamera->GetComponentLocation();
		const FVector TraceEnd = CamWorld - FVector(0.f, 0.f, 120.f);
		FHitResult HitCam, HitVis;
		FCollisionQueryParams QP(SCENE_QUERY_STAT(CamFloorClamp), /*bTraceComplex=*/false, this);
		const bool bHitCam = GetWorld() && GetWorld()->LineTraceSingleByChannel(
			HitCam, CamWorld, TraceEnd, ECC_Camera, QP);
		const bool bHitVis = GetWorld() && GetWorld()->LineTraceSingleByChannel(
			HitVis, CamWorld, TraceEnd, ECC_Visibility, QP);

		// Usa el hit más cercano entre ambos canales.
		float DistToFloor = TNumericLimits<float>::Max();
		if (bHitCam) { DistToFloor = FMath::Min(DistToFloor, CamWorld.Z - HitCam.ImpactPoint.Z); }
		if (bHitVis) { DistToFloor = FMath::Min(DistToFloor, CamWorld.Z - HitVis.ImpactPoint.Z); }

		float DesiredLiftZ = 0.f;
		constexpr float MinFloor = 40.f;
		if (DistToFloor < MinFloor)
		{
			DesiredLiftZ = (MinFloor - DistToFloor);
		}
		CameraFloorLiftCurrent = FMath::FInterpTo(CameraFloorLiftCurrent, DesiredLiftZ, DeltaTime, 10.f);

		FVector RelLoc = CameraBoomRelativeOffset;
		RelLoc.Z += CameraFloorLiftCurrent;
		CameraBoom->SetRelativeLocation(RelLoc);

		// DIAGNOSTIC: log cada 1s con datos del trace (para depurar si clamp activa).
		static float LogAccumulator = 0.f;
		LogAccumulator += DeltaTime;
		if (LogAccumulator > 1.0f)
		{
			LogAccumulator = 0.f;
			UE_LOG(LogTemp, Verbose,
				TEXT("[Diagnostic] FloorClamp HitCam=%s HitVis=%s DistToFloor=%.1f Lift=%.1f"),
				bHitCam ? TEXT("Y") : TEXT("N"),
				bHitVis ? TEXT("Y") : TEXT("N"),
				DistToFloor > 1e8f ? -1.f : DistToFloor,
				CameraFloorLiftCurrent);
		}
	}
}

void ATortugaCharacter::ApplyLegAngle(FName BoneName, const FRotator& RestRot, float AngleDeg) const
{
	const FQuat SwingQuat(LegSwingAxis.GetSafeNormal(), FMath::DegreesToRadians(AngleDeg));
	SetAnimBoneRot(BoneName, (FQuat(RestRot) * SwingQuat).Rotator());
}

void ATortugaCharacter::ApplyArmAngle(FName BoneName, const FRotator& RestRot,
                                       float RestOffsetDeg, float SwingDeg) const
{
	const FVector RestAxis  = ArmSwingAxis.GetSafeNormal();
	const FVector SwingAxis = FVector(1.f, 0.f, 0.f);
	const FQuat   OffsetQuat(RestAxis,  FMath::DegreesToRadians(RestOffsetDeg));
	const FQuat   SwingQuat (SwingAxis, FMath::DegreesToRadians(SwingDeg));
	SetAnimBoneRot(BoneName, (SwingQuat * OffsetQuat * FQuat(RestRot)).Rotator());
}

USceneComponent* ATortugaCharacter::FindChildByName(FName Name) const
{
	// 1. Exact FName match (fastest)
	for (UActorComponent* Comp : GetComponents())
	{
		if (Comp && Comp->GetFName() == Name)
		{
			return Cast<USceneComponent>(Comp);
		}
	}

	// 2. Fallback: case-insensitive substring match on the component name.
	//    Catches "Cabeza_0", "cabeza", "SM_Cabeza", etc.
	const FString NameStr = Name.ToString();
	for (UActorComponent* Comp : GetComponents())
	{
		if (Comp)
		{
			const FString CompName = Comp->GetName();
			if (CompName.Contains(NameStr, ESearchCase::IgnoreCase))
			{
				UE_LOG(LogTemp, Warning, TEXT("[TortugaCharacter] '%s' not found as exact name — matched '%s' (fuzzy). Rename to '%s' in BP for best results."),
					*NameStr, *CompName, *NameStr);
				return Cast<USceneComponent>(Comp);
			}
		}
	}

	return nullptr;
}

void ATortugaCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Log diagnóstico para investigar despawn intermitente del ragdoll-pickup.
	// Si veo "EndPlay reason=Destroyed bIsDead=true" justo después de
	// FinalizeDeathVisual → algo destruye el pawn que está siendo el visual
	// del rescue pickup. Casos legítimos: Logout del PC propietario, travel,
	// quit. Casos buggy: alguien llama Destroy() directo sin checks.
	UE_LOG(LogTemp, Log,
		TEXT("[DEATH-DEBUG] %s EndPlay reason=%d bIsDead=%d bIsKnocked=%d HasOwner=%d"),
		*GetName(), (int32)EndPlayReason, bIsDead ? 1 : 0, bIsKnockedDown ? 1 : 0,
		GetOwner() != nullptr);

	FocusedInteractable = nullptr;
	ActiveEmoteIndex   = -1;
	bEmoteBlendingOut  = false;
	bIsDiving          = false;
	DiveLockTimer      = 0.f;
	DiveTiltAlpha      = 0.f;
	bJumpAnimActive    = false;
	JumpAnimTime       = 0.f;

	GetWorldTimerManager().ClearTimer(CosmeticRetryTimerHandle);
	StopEmoteSound();
	StopReviveChannelSound();
	StopDBNOHeartbeatSound();

	GetWorldTimerManager().ClearTimer(InteractionScanTimerHandle);
	GetWorldTimerManager().ClearTimer(KnockdownTimerHandle);
	GetWorldTimerManager().ClearTimer(ReviveChannelTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void ATortugaCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	CacheInputAssets();
	ApplyInputMappingIfLocal();

	// ── Re-aplicar casco tras seamless travel ────────────────────────────────
	// Después del viaje, el pawn es nuevo. OnRep_EquippedHelmetId no se dispara
	// (el valor no cambió) y OnRep_PlayerState puede llegar antes de que la ref
	// al pawn sea válida en el PlayerState. PawnClientRestart es el hook seguro:
	// en este punto el PlayerController y PlayerState ya están disponibles en el cliente.
	if (const ATN_CoopPlayerState* TNPS = GetPlayerState<ATN_CoopPlayerState>())
	{
		UpdateHelmetMesh(TNPS->EquippedHelmetId);
		UpdateSkinVisual(TNPS->EquippedSkinId);
	}

	// ── Restaurar input mode y foco del viewport ─────────────────────────────
	// ApplyGameplayInputMode() se llama en BeginPlay del PC, pero en ese momento
	// el viewport puede no estar completamente listo (especialmente en builds
	// empaquetados). PawnClientRestart se dispara DESPUÉS de que la posesión es
	// confirmada en el cliente, garantizando que el foco se aplica correctamente.
	// ForceRestoreInput = ResetIgnoreInputFlags + ApplyGameplayInputMode.
	if (AMP_GamePlayerController* PC = Cast<AMP_GamePlayerController>(GetController()))
	{
		PC->ForceRestoreInput();

		// ── Re-añadir HUD widgets al viewport (cliente tras seamless travel) ─
		// OnPossess solo se ejecuta en el SERVIDOR. En el cliente, PawnClientRestart
		// es el hook equivalente (disparado por ClientRestart RPC).
		// Los widgets del PC persisten entre mapas pero son eliminados del viewport
		// por UWorld::CleanupWorld durante la transición. Aquí los volvemos a añadir.
		PC->RefreshHUDAfterPossession();
	}

	// ── Scan timer de interacción ─────────────────────────────────────────────
	// BeginPlay no puede arrancar el timer porque IsLocallyControlled() es false
	// antes de que el PC posea el pawn. PawnClientRestart se dispara DESPUÉS de
	// la posesión (vía ClientRestart RPC), cuando IsLocallyControlled() ya es true.
	// Cubre tanto la posesión inicial como la re-posesión tras seamless travel.
	if (IsLocallyControlled() && InteractionScanInterval > 0.f
		&& !GetWorldTimerManager().IsTimerActive(InteractionScanTimerHandle))
	{
		GetWorldTimerManager().SetTimer(InteractionScanTimerHandle, this,
			&ATortugaCharacter::UpdateFocusedInteractable, InteractionScanInterval, true);
		UE_LOG(LogTemp, Log, TEXT("[TortugaCharacter] Interaction scan timer started in PawnClientRestart (interval=%.2fs)"),
			InteractionScanInterval);
	}
}

void ATortugaCharacter::CacheInputAssets()
{
	if (bInputAssetsLoaded)
	{
		return;
	}

	LoadedMappingContext = DefaultMappingContext.LoadSynchronous();
	LoadedMoveAction = MoveAction.LoadSynchronous();
	LoadedLookAction = LookAction.LoadSynchronous();
	LoadedJumpAction = JumpAction.LoadSynchronous();
	LoadedInteractAction = InteractAction.LoadSynchronous();
	LoadedRotateInventoryAction = RotateInventoryAction.LoadSynchronous();
	LoadedSprintAction = SprintAction.LoadSynchronous();
	LoadedDropItemAction = DropItemAction.LoadSynchronous();

	// Load emote actions (tamaño dinámico — configurado en el BP)
	LoadedEmoteActions.SetNum(EmoteActions.Num());
	for (int32 i = 0; i < EmoteActions.Num(); i++)
	{
		LoadedEmoteActions[i] = EmoteActions[i].LoadSynchronous();
	}

	bInputAssetsLoaded = true;

	// ── Log de cada asset para diagnosticar qué falta ─────────────────────────
	auto LogAsset = [](const TCHAR* Name, const UObject* Asset)
	{
		if (Asset)
		{
			UE_LOG(LogTemp, Log, TEXT("[Input] ✓ %s loaded: %s"), Name, *Asset->GetPathName());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[Input] ✗ %s FAILED TO LOAD — create this asset in /Game/Blueprints/Gameplay/Controls/"), Name);
		}
	};

	LogAsset(TEXT("IMC_Player"), LoadedMappingContext);
	LogAsset(TEXT("IA_Move"), LoadedMoveAction);
	LogAsset(TEXT("IA_Look"), LoadedLookAction);
	LogAsset(TEXT("IA_Jump"), LoadedJumpAction);
	LogAsset(TEXT("IA_Interact"), LoadedInteractAction);
	LogAsset(TEXT("IA_RotateInventory"), LoadedRotateInventoryAction);
	LogAsset(TEXT("IA_Sprint"),           LoadedSprintAction);
	LogAsset(TEXT("IA_DropItem"),         LoadedDropItemAction);

	for (int32 i = 0; i < LoadedEmoteActions.Num(); i++)
	{
		const FString EmoteName = FString::Printf(TEXT("IA_Emote%d"), i);
		LogAsset(*EmoteName, LoadedEmoteActions[i]);
	}
}

void ATortugaCharacter::ApplyInputMappingIfLocal()
{
	if (!LoadedMappingContext)
	{
		return;
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				InputSubsystem->ClearAllMappings();
				InputSubsystem->AddMappingContext(LoadedMappingContext, 0);
			}
		}
	}
}

void ATortugaCharacter::ReapplyInputMapping()
{
	CacheInputAssets();
	ApplyInputMappingIfLocal();
}

void ATortugaCharacter::RemoveBigHeadEffect()
{
	if (!bBigHead) { return; }

	GetWorldTimerManager().ClearTimer(BigHeadTimerHandle);
	bBigHead = false;
	ApplyBigHeadVisual(false);

	// Disparar efecto de mareo en todas las máquinas (#2).
	if (HasAuthority() && MareoDurationSeconds > 0.f)
	{
		MulticastApplyMareoEffect(MareoDurationSeconds);
	}
}

void ATortugaCharacter::MulticastApplyMareoEffect_Implementation(float Duration)
{
	// ── Reducir velocidad durante la duración del mareo ───────────────────────
	if (MareoSpeedCap > 0.f)
	{
		if (UTN_StaminaComponent* SC = FindComponentByClass<UTN_StaminaComponent>())
		{
			SC->SetSpeedCap(MareoSpeedCap);

			FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &ATortugaCharacter::ClearMareoSpeedCap);
			GetWorldTimerManager().SetTimer(MareoTimerHandle, Del, Duration, false);
		}
	}

	// ── Feedback local (camera shake, VFX, audio) — solo cliente local ───────
	if (IsLocallyControlled())
	{
		OnMareoEffect(Duration);
	}
}

void ATortugaCharacter::ClearMareoSpeedCap()
{
	if (UTN_StaminaComponent* SC = FindComponentByClass<UTN_StaminaComponent>())
	{
		SC->ClearSpeedCap();
	}
}

// ── Tinta de calamar (#13) ─────────────────────────────────────────────────────

void ATortugaCharacter::ApplyInkEffect(float Duration)
{
	if (!IsLocallyControlled() || !InkOverlayMaterial || !InkPostProcess) { return; }

	// Registrar el material en el PostProcess local y activarlo.
	// AddOrUpdateBlendable garantiza que no se acumulan entradas duplicadas
	// si ApplyInkEffect se llama varias veces antes de que expire el timer.
	InkPostProcess->AddOrUpdateBlendable(InkOverlayMaterial, 1.f);
	InkPostProcess->bEnabled = true;

	GetWorldTimerManager().ClearTimer(InkEffectTimerHandle);
	FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &ATortugaCharacter::ClearInkEffect);
	GetWorldTimerManager().SetTimer(InkEffectTimerHandle, Del, Duration, false);
}

void ATortugaCharacter::ClearInkEffect()
{
	if (InkPostProcess) { InkPostProcess->bEnabled = false; }
}

void ATortugaCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UE_LOG(LogTemp, Log, TEXT("[Input] SetupPlayerInputComponent called on '%s' (LocallyControlled=%s)"),
		*GetName(), IsLocallyControlled() ? TEXT("YES") : TEXT("NO"));

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		CacheInputAssets();
		if (LoadedMoveAction)
		{
			EnhancedInput->BindAction(LoadedMoveAction, ETriggerEvent::Triggered, this, &ATortugaCharacter::Move);
			EnhancedInput->BindAction(LoadedMoveAction, ETriggerEvent::Completed, this, &ATortugaCharacter::OnMoveReleased);
			EnhancedInput->BindAction(LoadedMoveAction, ETriggerEvent::Canceled, this, &ATortugaCharacter::OnMoveReleased);
		}
		if (LoadedLookAction)
		{
			EnhancedInput->BindAction(LoadedLookAction, ETriggerEvent::Triggered, this, &ATortugaCharacter::Look);
		}
		if (LoadedJumpAction)
		{
			EnhancedInput->BindAction(LoadedJumpAction, ETriggerEvent::Started, this, &ATortugaCharacter::Jump);
			EnhancedInput->BindAction(LoadedJumpAction, ETriggerEvent::Completed, this, &ATortugaCharacter::StopJumping);
		}
		if (LoadedInteractAction)
		{
			EnhancedInput->BindAction(LoadedInteractAction, ETriggerEvent::Started, this, &ATortugaCharacter::TryInteract);
			UE_LOG(LogTemp, Log, TEXT("[Input] ✓ IA_Interact bound to TryInteract"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[Input] ✗ IA_Interact NOT bound — asset is null! Create /Game/Blueprints/Gameplay/Controls/IA_Interact"));
		}
		if (LoadedRotateInventoryAction)
		{
			EnhancedInput->BindAction(LoadedRotateInventoryAction, ETriggerEvent::Started, this, &ATortugaCharacter::RotateInventory);
		}
		if (LoadedSprintAction)
		{
			EnhancedInput->BindAction(LoadedSprintAction, ETriggerEvent::Started, this, &ATortugaCharacter::StartSprint);
			EnhancedInput->BindAction(LoadedSprintAction, ETriggerEvent::Completed, this, &ATortugaCharacter::StopSprint);
			EnhancedInput->BindAction(LoadedSprintAction, ETriggerEvent::Canceled, this, &ATortugaCharacter::StopSprint);
		}
		if (LoadedDropItemAction)
		{
			EnhancedInput->BindAction(LoadedDropItemAction, ETriggerEvent::Started, this, &ATortugaCharacter::DropEquippedItem);
		}
		// ── Emotes 0–9 ────────────────────────────────────────────────────────
		static void (ATortugaCharacter::* const EmoteHandlers[10])() =
		{
			&ATortugaCharacter::OnEmote0, &ATortugaCharacter::OnEmote1,
			&ATortugaCharacter::OnEmote2, &ATortugaCharacter::OnEmote3,
			&ATortugaCharacter::OnEmote4, &ATortugaCharacter::OnEmote5,
			&ATortugaCharacter::OnEmote6, &ATortugaCharacter::OnEmote7,
			&ATortugaCharacter::OnEmote8, &ATortugaCharacter::OnEmote9,
		};
		for (int32 i = 0; i < LoadedEmoteActions.Num() && i < 10; i++)
		{
			if (LoadedEmoteActions[i])
			{
				EnhancedInput->BindAction(LoadedEmoteActions[i], ETriggerEvent::Started, this, EmoteHandlers[i]);
				UE_LOG(LogTemp, Log, TEXT("[Input] ✓ IA_Emote%d bound"), i);
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[Input] ✗ PlayerInputComponent is NOT an EnhancedInputComponent! Check DefaultInput.ini uses EnhancedPlayerInput."));
	}
}

void ATortugaCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();

	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		JumpStartHorizontalVelocity = CMC->Velocity;
		JumpStartHorizontalVelocity.Z = 0.f;
	}
	UE_LOG(LogTemp, Log, TEXT("[Jump] %s jumped · captured horizontal velocity=%s (speed=%.0f)"),
		*GetName(), *JumpStartHorizontalVelocity.ToString(), JumpStartHorizontalVelocity.Size());
}

void ATortugaCharacter::Jump()
{
	if (bIsKnockedDown || bIsDead) { return; }

	// Segundo press de salto en el aire → dive (igual que Fall Guys)
	if (GetCharacterMovement()->IsFalling())
	{
		TryDive();
		return;
	}

	// Grounded y no en dive → salto normal + trigger jump procedural animation
	if (!bIsDiving)
	{
		if (CanJump())
		{
			bJumpAnimActive = true;
			JumpAnimTime    = 0.f;
		}
		Super::Jump();
	}
}

void ATortugaCharacter::PerformAirDashLocally()
{
	bCanAirDash = false;
	const FVector DashVelocity = GetActorForwardVector() * AirDashHorizontalForce
	                           + FVector::UpVector * AirDashVerticalBoost;
	LaunchCharacter(DashVelocity, true, true);
}

void ATortugaCharacter::ServerPerformAirDash_Implementation()
{
	if (!GetCharacterMovement()->IsFalling() || !bCanAirDash || bIsKnockedDown || bIsDead)
	{
		return;
	}
	bCanAirDash = false;
	const FVector DashVelocity = GetActorForwardVector() * AirDashHorizontalForce
	                           + FVector::UpVector * AirDashVerticalBoost;
	LaunchCharacter(DashVelocity, true, true);
}

void ATortugaCharacter::Move(const FInputActionValue& Value)
{
	// Movement is locked during the dive and recovery slide
	if (bIsDiving) { return; }
	// Movement is locked during knockdown — momentum from LaunchCharacter takes over
	if (bIsKnockedDown) { return; }

	// Cancel any active emote the moment the player moves —
	// EXCEPT emotes 5 (Baile Irlandés) and 6 (Superman) which are walkable,
	// and the knockdown emote which must not be interrupted by movement input.
	if (!bIsKnockedDown && ActiveEmoteIndex >= 0 && ActiveEmoteIndex != 5 && ActiveEmoteIndex != 6) { CancelEmote(); }

	const FVector2D MovementVector = Value.Get<FVector2D>();
	if (Controller)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}

	LastMovementInput = MovementVector;
	RefreshSprintRequest();
}

void ATortugaCharacter::OnMoveReleased()
{
	LastMovementInput = FVector2D::ZeroVector;
	RefreshSprintRequest();
}

void ATortugaCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	if (Controller)
	{
		// Sensibilidad independiente por eje.
		const float Yaw   =  LookAxisVector.X * LookSensitivityX;
		// bInvertCameraY: true → invertir eje vertical (arriba/abajo del ratón).
		const float Pitch = LookAxisVector.Y * LookSensitivityY * (bInvertCameraY ? -1.f : 1.f);

		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ATortugaCharacter::TryInteract()
{
	if (bIsKnockedDown) { return; }

	const bool bDebug = CVarDebugInteraction.GetValueOnGameThread() != 0;

	if (bDebug)
	{
		UE_LOG(LogTemp, Log, TEXT("[Interact:DEBUG] === E PRESSED === FocusedInteractable: %s"),
			FocusedInteractable.IsValid() ? *FocusedInteractable->GetName() : TEXT("(none)"));
	}

	// Si no hay foco, intentar un scan inmediato
	if (!FocusedInteractable.IsValid())
	{
		UpdateFocusedInteractable();
	}

	// Si tras el scan sigue sin haber interactuable → usar ítem equipado (lanzar bola, etc.)
	if (!FocusedInteractable.IsValid())
	{

		if (bDebug)
		{
			UE_LOG(LogTemp, Log, TEXT("[Interact:DEBUG] No interactable in focus → TryUseEquippedItem"));
		}
		TryUseEquippedItem();
		return;
	}

	if (bDebug)
	{
		UE_LOG(LogTemp, Log, TEXT("[Interact:DEBUG] Sending ServerTryInteract → %s (CanInteract client-side: %s)"),
			*FocusedInteractable->GetName(),
			FocusedInteractable->CanInteract(this) ? TEXT("YES") : TEXT("NO"));
	}

	ServerTryInteract(FocusedInteractable.Get());
}

void ATortugaCharacter::RotateInventory()
{
	if (InventoryComponent)
	{
		InventoryComponent->RotateItems();
	}
}

void ATortugaCharacter::StartSprint()
{
	if (bIsDiving) { return; }
	bSprintHeld = true;
	RefreshSprintRequest();
}

void ATortugaCharacter::StopSprint()
{
	bSprintHeld = false;
	RefreshSprintRequest();
}

void ATortugaCharacter::DropEquippedItem()
{
	ServerDropEquippedItem();
}

void ATortugaCharacter::TryUseEquippedItem()
{
	ServerUseEquippedItem();
}

void ATortugaCharacter::RefreshSprintRequest()
{
	if (!StaminaComponent)
	{
		return;
	}

	// Sprint funciona en cualquier dirección de movimiento (delante, lateral, diagonal).
	// Solo se desactiva cuando el jugador solta el stick/WASD por completo.
	const bool bHasMovementInput = LastMovementInput.SizeSquared() > (0.25f * 0.25f);
	StaminaComponent->SetSprintRequested(bSprintHeld && bHasMovementInput);
}

void ATortugaCharacter::UpdateFocusedInteractable()
{
	// Si el pawn ya no tiene controlador (terminó la carrera, murió, espectador)
	// limpiar el foco y no hacer overlap queries sobre el pawn oculto.
	if (!GetWorld() || !GetController()) { FocusedInteractable = nullptr; return; }

	const bool bDebug = CVarDebugInteraction.GetValueOnGameThread() != 0;

	// ── Detección por proximidad: esfera alrededor del personaje ─────────────
	// No usa raycast ni cámara — el jugador solo tiene que acercarse al objeto.
	// Busca todos los actores WorldDynamic en el radio y escoge el más cercano
	// que sea un ATN_InteractableBase válido.
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TN_InteractionProximity), false);
	QueryParams.AddIgnoredActor(this);

	GetWorld()->OverlapMultiByObjectType(
		Overlaps,
		GetActorLocation(),
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_WorldDynamic),
		FCollisionShape::MakeSphere(MaxInteractionDistance),
		QueryParams);

	ATN_InteractableBase* BestCandidate = nullptr;
	float BestDistSq = FLT_MAX;

	for (const FOverlapResult& Result : Overlaps)
	{
		ATN_InteractableBase* Interactable = Cast<ATN_InteractableBase>(Result.GetActor());
		if (!Interactable || !Interactable->CanInteract(this)) { continue; }

		const float DistSq = FVector::DistSquared(GetActorLocation(), Interactable->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestCandidate = Interactable;
		}
	}

	if (FocusedInteractable.Get() != BestCandidate)
	{
		FocusedInteractable = BestCandidate;
		if (bDebug)
		{
			UE_LOG(LogTemp, Log, TEXT("[Interact:DEBUG] Focus → %s  (dist=%.0f)"),
				BestCandidate ? *BestCandidate->GetName() : TEXT("(none)"),
				BestCandidate ? FVector::Dist(GetActorLocation(), BestCandidate->GetActorLocation()) : 0.f);
		}
	}

	if (bDebug)
	{
		// Mostrar la esfera de detección
		DrawDebugSphere(GetWorld(), GetActorLocation(), MaxInteractionDistance,
			16, BestCandidate ? FColor::Green : FColor::Silver,
			false, InteractionScanInterval * 1.5f, 0, 0.8f);

		if (BestCandidate)
		{
			DrawDebugLine(GetWorld(), GetActorLocation(), BestCandidate->GetActorLocation(),
				FColor::Cyan, false, InteractionScanInterval * 1.5f, 0, 2.f);
		}
	}
}

FVector ATortugaCharacter::FindGroundBelow(const FVector& WorldLocation) const
{
	if (!GetWorld()) { return WorldLocation; }

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TN_GroundTrace), false, this);
	const FVector Start = WorldLocation + FVector(0.f, 0.f, 30.f);
	const FVector End   = WorldLocation - FVector(0.f, 0.f, 1500.f);

	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
	{
		// Offset por la mitad de la extensión vertical del mesh para que el
		// borde inferior del objeto quede apoyado en el suelo (no el centro).
		// El valor por defecto de 15cm cubre la mayoría de items pequeños.
		return Hit.ImpactPoint + FVector(0.f, 0.f, 15.f);
	}
	return WorldLocation;
}


void ATortugaCharacter::ServerTryInteract_Implementation(ATN_InteractableBase* Interactable)
{
	const bool bDebug = CVarDebugInteraction.GetValueOnGameThread() != 0;

	if (!Interactable)
	{
		if (bDebug) { UE_LOG(LogTemp, Warning, TEXT("[Interact:SERVER] Interactable is NULL — client sent invalid reference")); }
		return;
	}

	if (!Interactable->CanInteract(this))
	{
		// Si falla en un pickup Y tenemos ítem equipado → asumir "inventario lleno"
		// y usar/lanzar el ítem directamente, sin desperdiciar el input del jugador.
		if (Cast<ATN_PickupInteractableBase>(Interactable)
			&& InventoryComponent && InventoryComponent->HasEquippedItem())
		{
			if (bIsKnockedDown || bIsDead)
			{
				return;
			}
			if (bDebug)
			{
				UE_LOG(LogTemp, Log, TEXT("[Interact:SERVER] Pickup '%s' no recogible + inventario lleno → usando ítem equipado."),
					*Interactable->GetName());
			}
			ServerUseEquippedItem_Implementation();
		}
		else if (bDebug)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Interact:SERVER] CanInteract=FALSE para '%s' — tomado, desactivado o sin espacio."), *Interactable->GetName());
		}
		return;
	}

	const float MaxDistance = FMath::Max(MaxInteractionDistance, Interactable->GetInteractionDistance());
	float PingDistanceAllowance = 0.f;
	if (const APlayerState* PS = GetPlayerState())
	{
		PingDistanceAllowance = FMath::Clamp(PS->ExactPing * 0.25f, 0.f, MaxLagCompensationDistance);
	}

	const float TotalAllowed = MaxDistance + 100.f + PingDistanceAllowance;
	const float ActualDist = FVector::Dist(GetActorLocation(), Interactable->GetActorLocation());

	if (ActualDist > TotalAllowed)
	{
		if (bDebug) { UE_LOG(LogTemp, Warning, TEXT("[Interact:SERVER] TOO FAR — dist=%.1f  allowed=%.1f  (MaxDist=%.1f + 100 + ping=%.1f)"), ActualDist, TotalAllowed, MaxDistance, PingDistanceAllowance); }
		return;
	}

	if (bDebug) { UE_LOG(LogTemp, Log, TEXT("[Interact:SERVER] ✓ Calling Interact on '%s' — dist=%.1f"), *Interactable->GetName(), ActualDist); }

	Interactable->Interact(this);
}

void ATortugaCharacter::ServerUseEquippedItem_Implementation()
{
	if (bIsKnockedDown || bIsDead)
	{
		return;
	}
	if (!InventoryComponent || !StaminaComponent)
	{
		return;
	}

	if (!InventoryComponent->HasEquippedItem())
	{
		return;
	}

	const FTN_InventoryItem EquippedItem = InventoryComponent->GetEquippedItem();
	if (!EquippedItem.IsValid())
	{
		return;
	}

	if (EquippedItem.UseType == ETN_ItemUseType::SelfStaminaBoost)
	{
		FTN_InventoryItem ConsumedItem;
		if (!InventoryComponent->TryConsumeEquippedItem(ConsumedItem))
		{
			return;
		}

		// Aplicar penalización post-boost específica del ítem (sobreescribe el valor global del componente).
		StaminaComponent->SetPostBoostExhaustionSeconds(EquippedItem.StaminaBoostData.PostBoostExhaustionSeconds);
		GrantInfiniteStamina(EquippedItem.StaminaBoostData.DurationSeconds);
		return;
	}

	// #3 — Barrita Energética: recuperación instantánea al máximo, sin boost de duración ni penalización.
	if (EquippedItem.UseType == ETN_ItemUseType::SelfStaminaFull)
	{
		FTN_InventoryItem ConsumedItem;
		if (!InventoryComponent->TryConsumeEquippedItem(ConsumedItem))
		{
			return;
		}

		// Resetear penalización post-boost heredada antes de restaurar,
		// para que no se aplique agotamiento si el jugador usó un boost antes.
		StaminaComponent->SetPostBoostExhaustionSeconds(0.f);
		StaminaComponent->RestoreStaminaToFull();
		return;
	}

	if (EquippedItem.UseType == ETN_ItemUseType::BigHead)
	{
		FTN_InventoryItem ConsumedItem;
		if (!InventoryComponent->TryConsumeEquippedItem(ConsumedItem))
		{
			return;
		}

		bBigHead = true;
		ApplyBigHeadVisual(true);

		// Timer para restablecer al tamaño original + efecto de mareo (#2).
		// CreateUObject en lugar de lambda: ClearAllTimersForObject lo cancela en EndPlay.
		FTimerDelegate BigHeadDel = FTimerDelegate::CreateUObject(this, &ATortugaCharacter::RemoveBigHeadEffect);
		GetWorldTimerManager().SetTimer(BigHeadTimerHandle, BigHeadDel, BigHeadDurationSeconds, false);

		return;
	}

	if ((EquippedItem.UseType == ETN_ItemUseType::Throwable)
		&& EquippedItem.ThrowableData.ActorClass)
	{
		const FVector SpawnLocation = GetItemSpawnLocation();

		// ── Dirección de lanzamiento: cámara + arco parabólico ────────────
		// Usar la dirección de cámara directamente (incluye pitch) para que
		// apuntar arriba/abajo cambie la trayectoria del lanzamiento.
		// ThrowUpAngleDeg se añade ENCIMA de la dirección de cámara como arco extra.
		const FVector CamDir     = GetItemForwardDirection(); // incluye pitch del controlador
		const FVector SafeCamDir = CamDir.IsNearlyZero() ? GetActorForwardVector() : CamDir.GetSafeNormal();

		// Eje de inclinación: perpendicular a la proyección horizontal de la cámara.
		const FVector HorizProj = FVector(SafeCamDir.X, SafeCamDir.Y, 0.f).GetSafeNormal();
		const FVector TiltAxis  = HorizProj.IsNearlyZero()
			? GetActorRightVector().GetSafeNormal()
			: FVector::CrossProduct(HorizProj, FVector::UpVector).GetSafeNormal();
		const FQuat   UpTilt(TiltAxis, FMath::DegreesToRadians(ThrowUpAngleDeg));
		const FVector ArcedDirection = UpTilt.RotateVector(SafeCamDir).GetSafeNormal();

		const FVector LaunchVelocity = ArcedDirection * FMath::Max(EquippedItem.ThrowableData.ThrowSpeed, 0.0f);

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		FTN_InventoryItem ConsumedItem;
		if (!InventoryComponent->TryConsumeEquippedItem(ConsumedItem))
		{
			return;
		}

		if (ATN_ThrowableItemActor* ThrowableActor = GetWorld()->SpawnActor<ATN_ThrowableItemActor>(EquippedItem.ThrowableData.ActorClass, SpawnLocation, ArcedDirection.Rotation(), SpawnParams))
		{
			// SourceItem lleva PickupActorClass para que el throwable sepa
			// qué pickup spawnear cuando aterrice o impacte (se convierte en recogible)
			ThrowableActor->SetSourceItem(ConsumedItem);
			ThrowableActor->InitializeThrow(SpawnLocation, LaunchVelocity);
		}
		else
		{
			InventoryComponent->TryAddOrReplaceEquipped(ConsumedItem, true);
		}
		return;
	}

	// ── #22 Concha trampa ────────────────────────────────────────────────────────
	if ((EquippedItem.UseType == ETN_ItemUseType::Conch)
		&& EquippedItem.ConchData.ActorClass)
	{
		FTN_InventoryItem ConsumedItem;
		if (!InventoryComponent->TryConsumeEquippedItem(ConsumedItem)) { return; }

		// Colocar la concha en el suelo justo debajo del jugador
		const FVector PlaceLoc = FindGroundBelow(GetActorLocation());

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner     = this;
		SpawnParams.Instigator = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		if (ATN_ConchPickup* Conch = GetWorld()->SpawnActor<ATN_ConchPickup>(
			ConsumedItem.ConchData.ActorClass, PlaceLoc, FRotator::ZeroRotator, SpawnParams))
		{
			Conch->PlaceAsTrap(PlaceLoc);
		}
		return;
	}

	// ── #13 Tinta de calamar ─────────────────────────────────────────────────────
	if ((EquippedItem.UseType == ETN_ItemUseType::InkThrower)
		&& EquippedItem.InkData.ProjectileClass)
	{
		FTN_InventoryItem ConsumedItem;
		if (!InventoryComponent->TryConsumeEquippedItem(ConsumedItem)) { return; }

		const FVector Origin    = GetItemSpawnLocation();
		const FVector Direction = GetItemForwardDirection();
		ATN_InkProjectile::Spawn(this, ConsumedItem.InkData.ProjectileClass,
			Origin, Direction, ConsumedItem.InkData.ThrowSpeed);
		return;
	}

	// ── #5 Tótem — uso manual: revivir a un jugador muerto aleatorio ──────────
	if (EquippedItem.UseType == ETN_ItemUseType::Totem)
	{
		// Buscar jugadores eliminados
		TArray<APlayerController*> DeadPlayers;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (!PC || PC == GetController()) { continue; }
			ATN_CoopPlayerState* PS = PC->GetPlayerState<ATN_CoopPlayerState>();
			if (PS && PS->bIsEliminated)
			{
				DeadPlayers.Add(PC);
			}
		}

		if (DeadPlayers.Num() == 0)
		{
			// Nadie a quien revivir — no consumir el ítem
			return;
		}

		FTN_InventoryItem ConsumedItem;
		if (!InventoryComponent->TryConsumeEquippedItem(ConsumedItem)) { return; }

		// Seleccionar y revivir
		const int32 Idx = FMath::RandRange(0, DeadPlayers.Num() - 1);
		APlayerController* TargetPC = DeadPlayers[Idx];

		if (ATN_RunGameMode* GM = GetWorld()->GetAuthGameMode<ATN_RunGameMode>())
		{
			GM->RevivePlayer(TargetPC);

			APawn* RevivedPawn = TargetPC->GetPawn();
			if (RevivedPawn)
			{
				const FVector RightOffset = GetActorRightVector() * 150.f;
				RevivedPawn->TeleportTo(GetActorLocation() + RightOffset, GetActorRotation());
			}
		}
		return;
	}
}

void ATortugaCharacter::ServerDropEquippedItem_Implementation()
{
	if (bIsKnockedDown || bIsDead) { return; }
	if (!InventoryComponent) { return; }

	FTN_InventoryItem DroppedItem;
	if (!InventoryComponent->TryExtractEquippedItem(DroppedItem) || !DroppedItem.IsValid() || !DroppedItem.PickupActorClass)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// Siempre spawnear en el suelo aunque el personaje esté en el aire
	const FVector DropPoint = FindGroundBelow(GetItemSpawnLocation());

	if (ATN_PickupInteractableBase* PickupActor = GetWorld()->SpawnActor<ATN_PickupInteractableBase>(
		DroppedItem.PickupActorClass, DropPoint, FRotator::ZeroRotator, SpawnParams))
	{
		PickupActor->InitializeFromInventoryItem(DroppedItem);
	}
}

FVector ATortugaCharacter::GetItemSpawnLocation() const
{
	return GetActorLocation() + (GetActorForwardVector() * 120.0f) + FVector(0.0f, 0.0f, 40.0f);
}

FVector ATortugaCharacter::GetItemForwardDirection() const
{
	if (Controller)
	{
		const FRotator ViewRotation = Controller->GetControlRotation();
		return ViewRotation.Vector().GetSafeNormal();
	}

	return GetActorForwardVector();
}

void ATortugaCharacter::GrantInfiniteStamina(float DurationSeconds)
{
	if (!StaminaComponent)
	{
		return;
	}

	StaminaComponent->GrantUnlimitedStamina(DurationSeconds);
}

void ATortugaCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// Re-apply helmet when PlayerState is replicated late (after BeginPlay timer already fired).
	// This covers the race condition where PlayerState arrives long after pawn possession.
	if (const ATN_CoopPlayerState* TNPS = GetPlayerState<ATN_CoopPlayerState>())
	{
		UpdateHelmetMesh(TNPS->EquippedHelmetId);
		UpdateSkinVisual(TNPS->EquippedSkinId);
	}
}

// ── Cosmetics ─────────────────────────────────────────────────────────────────

void ATortugaCharacter::UpdateHelmetMesh(FName HelmetId)
{
	if (!HelmetMeshComp)
	{
		return;
	}

	// NAME_None = desequipar
	if (HelmetId == NAME_None)
	{
		HelmetMeshComp->SetStaticMesh(nullptr);
		HelmetMeshComp->SetHiddenInGame(true);
		return;
	}

	// Obtener DataTable desde GameInstance
	const UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance());
	if (!GI)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TortugaCharacter] UpdateHelmetMesh: GameInstance no es UMP_GameInstance."));
		return;
	}

	const UDataTable* HelmDT = GI->GetHelmetDataTable();
	if (!HelmDT)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TortugaCharacter] UpdateHelmetMesh: HelmetDataTable no asignado en BP_GameInstance."));
		return;
	}

	const FTN_HelmetData* Row = HelmDT->FindRow<FTN_HelmetData>(HelmetId, TEXT("UpdateHelmetMesh"));
	if (!Row)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TortugaCharacter] UpdateHelmetMesh: HelmetId '%s' no encontrado en DT_Helmets."), *HelmetId.ToString());
		HelmetMeshComp->SetStaticMesh(nullptr);
		HelmetMeshComp->SetHiddenInGame(true);
		return;
	}

	if (!Row->DisplayMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TortugaCharacter] UpdateHelmetMesh: HelmetId '%s' sin DisplayMesh asignado."), *HelmetId.ToString());
		HelmetMeshComp->SetHiddenInGame(true);
		return;
	}

	HelmetMeshComp->SetStaticMesh(Row->DisplayMesh);
	HelmetMeshComp->SetRelativeScale3D(Row->MeshScale.IsNearlyZero() ? FVector::OneVector : Row->MeshScale);
	HelmetMeshComp->SetRelativeLocation(Row->MeshOffset);
	HelmetMeshComp->SetRelativeRotation(Row->MeshRotation);
	HelmetMeshComp->SetHiddenInGame(false);

	UE_LOG(LogTemp, Log, TEXT("[TortugaCharacter] '%s' equipa casco '%s'."), *GetName(), *HelmetId.ToString());
}

void ATortugaCharacter::UpdateSkinVisual(FName SkinId)
{
	USkeletalMeshComponent* SKM = GetMesh();
	if (!SKM) { return; }

	// Sin skin equipado → restaurar materiales originales cacheados en BeginPlay.
	if (SkinId == NAME_None)
	{
		for (int32 i = 0; i < DefaultSkelMeshMaterials.Num(); ++i)
		{
			SKM->SetMaterial(i, DefaultSkelMeshMaterials[i]);
		}
		return;
	}

	const UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance());
	const UDataTable* SkinDT = GI ? GI->GetSkinDataTable() : nullptr;
	if (!SkinDT)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TortugaCharacter] UpdateSkinVisual: SkinDataTable no asignado en BP_GameInstance."));
		return;
	}

	const FTN_SkinData* Row = SkinDT->FindRow<FTN_SkinData>(SkinId, TEXT("UpdateSkinVisual"));
	if (!Row)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TortugaCharacter] UpdateSkinVisual: SkinId '%s' no encontrado en DT_Skins."), *SkinId.ToString());
		return;
	}

	// Validar que el SkM unificado tiene los 5 slots esperados (Belly/EyeShine/
	// EyesMouth/Skin/Shell). Si tiene menos, SetMaterial(3/4,...) crea
	// OverrideMaterials fantasma sin warning y la skin parece aplicada pero no
	// se ve cambio en esos slots.
	const int32 NumMaterials = SKM->GetNumMaterials();
	if (NumMaterials < 5)
	{
		UE_LOG(LogTemp, Error, TEXT("[SKIN-CONFIG] '%s' SkM tiene solo %d slots (esperados 5: Belly/EyeShine/EyesMouth/Skin/Shell). Reordenar slots o reimportar mesh."),
			*GetName(), NumMaterials);
	}

	// Mapeo de slots del SkM unificado:
	//   Slot 0 → Barriga                         (BellyMaterial)
	//   Slot 1 → Brillo de los ojos              (EyeShineMaterial)
	//   Slot 2 → Ojos y boca                     (EyesMouthMaterial)
	//   Slot 3 → Cabeza, patas, brazos, cola     (SkinMaterial)
	//   Slot 4 → Caparazón principal             (ShellMaterial)
	//
	// Si la skin no aporta material para un slot, se mantiene el material por
	// defecto cacheado en BeginPlay → permite skins parciales (solo caparazón,
	// solo ojos, etc.) sin tener que repetir todos los materiales.
	auto AssignSlot = [&](int32 SlotIdx, UMaterialInterface* Mat)
	{
		if (Mat)
		{
			SKM->SetMaterial(SlotIdx, Mat);
		}
		else if (DefaultSkelMeshMaterials.IsValidIndex(SlotIdx))
		{
			SKM->SetMaterial(SlotIdx, DefaultSkelMeshMaterials[SlotIdx]);
		}
	};

	AssignSlot(0, Row->BellyMaterial);
	AssignSlot(1, Row->EyeShineMaterial);
	AssignSlot(2, Row->EyesMouthMaterial);
	AssignSlot(3, Row->SkinMaterial);
	AssignSlot(4, Row->ShellMaterial);

	// Forzar refresh del render state. Sin esto, SetMaterial puede no actualizar
	// visualmente con el SKM unificado en algunas configuraciones (overrideMaterials cache).
	SKM->MarkRenderStateDirty();

	UE_LOG(LogTemp, Log, TEXT("[SKIN-DEBUG] '%s' skin '%s' aplicado · slots=[0:%s 1:%s 2:%s 3:%s 4:%s]"),
		*GetName(), *SkinId.ToString(),
		*GetNameSafe(Row->BellyMaterial),
		*GetNameSafe(Row->EyeShineMaterial),
		*GetNameSafe(Row->EyesMouthMaterial),
		*GetNameSafe(Row->SkinMaterial),
		*GetNameSafe(Row->ShellMaterial));
}

void ATortugaCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	bCanAirDash = true;
}

// ── Replication ────────────────────────────────────────────────────────────────

void ATortugaCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// Replicar a todos los clientes para que el visual sea visible en todos
	DOREPLIFETIME(ATortugaCharacter, bIsKnockedDown);
	DOREPLIFETIME(ATortugaCharacter, bIsDead);
	// SkipOwner: el owner ya arranca el emote localmente en TriggerEmote/CancelEmote.
	DOREPLIFETIME_CONDITION(ATortugaCharacter, ReplicatedEmoteIndex, COND_SkipOwner);
	// DBNO revive state
	DOREPLIFETIME(ATortugaCharacter, bIsReviving);
	DOREPLIFETIME_CONDITION(ATortugaCharacter, ReviveProgress, COND_OwnerOnly);
	// BigHead consumable
	DOREPLIFETIME(ATortugaCharacter, bBigHead);
	// Dive
	DOREPLIFETIME(ATortugaCharacter, bIsDiving);
	DOREPLIFETIME(ATortugaCharacter, DiveTargetYaw);
	DOREPLIFETIME(ATortugaCharacter, bDiveYawInterpActive);
	// Umbrella protection (#29)
	DOREPLIFETIME(ATortugaCharacter, bHasUmbrellaProtection);
	// Head look — SkipOwner: el owner aplica la rotación localmente sin pasar por la red
	DOREPLIFETIME_CONDITION(ATortugaCharacter, ReplicatedHeadYaw,   COND_SkipOwner);
	DOREPLIFETIME_CONDITION(ATortugaCharacter, ReplicatedHeadPitch, COND_SkipOwner);
}

// ── Knockdown ─────────────────────────────────────────────────────────────────

void ATortugaCharacter::ApplyKnockdown(float Duration, FVector ImpulseOverride)
{
	if (!HasAuthority())
	{
		return;
	}

	// Evitar solapar knockdowns
	if (bIsKnockedDown)
	{
		// Si ya está en knockdown, reiniciar el timer con la nueva duración
		GetWorldTimerManager().SetTimer(KnockdownTimerHandle, this,
		                                &ATortugaCharacter::RecoverFromKnockdown, Duration, false);
		return;
	}

	// Cancelar dive si estaba activo — knockdown tiene prioridad
	if (bIsDiving)
	{
		EndDive();
	}

	bIsKnockedDown = true;

	// Impulso: usar override si se proporcionó, si no calcular del momentum actual
	if (UCharacterMovementComponent* MC = GetCharacterMovement())
	{
		FVector KnockImpulse;
		if (!ImpulseOverride.IsZero())
		{
			KnockImpulse = ImpulseOverride;
		}
		else
		{
			const FVector CurrentVel = MC->Velocity;
			KnockImpulse = FVector(
				CurrentVel.X * KnockdownHorizontalMultiplier,
				CurrentVel.Y * KnockdownHorizontalMultiplier,
				FMath::Min(CurrentVel.Z, 0.f) - KnockdownDownwardForce
			);
		}
		LaunchCharacter(KnockImpulse, /*bXYOverride=*/true, /*bZOverride=*/true);
	}

	// ── Knockdown visual via sistema de emotes ───────────────────────────────
	// ReplicatedEmoteIndex = KNOCKDOWN_EMOTE_ID replica la animación a todos los clientes
	// usando el mismo canal de replicación que los emotes normales (funciona perfectamente).
	ReplicatedEmoteIndex = KNOCKDOWN_EMOTE_ID;
	// Servidor: aplicar localmente (OnRep no dispara en quien posee la variable)
	StartEmoteLocally(KNOCKDOWN_EMOTE_ID);

	// Tilt del cuerpo (pitch -180°) — se ejecuta en servidor + todos los clientes.
	// Sin esto el emote solo agita brazos y el jugador se ve flotando, no tumbado.
	MulticastApplyKnockdownVisual(true);

	// ── DBNO heartbeat: solo el jugador local incapacitado oye el latido ──
	if (IsLocallyControlled())
	{
		PlayDBNOHeartbeatSound();
	}

	GetWorldTimerManager().SetTimer(KnockdownTimerHandle, this,
	                                &ATortugaCharacter::RecoverFromKnockdown, Duration, false);

	UE_LOG(LogTemp, Log, TEXT("[Knockdown] %s knocked down for %.1fs (momentum activo)"), *GetNameSafe(this), Duration);
}

void ATortugaCharacter::RecoverFromKnockdown()
{
	if (!HasAuthority())
	{
		return;
	}

	bIsKnockedDown = false;

	// Restaurar movimiento
	if (UCharacterMovementComponent* MC = GetCharacterMovement())
	{
		MC->SetMovementMode(MOVE_Walking);
	}

	// ── Cancelar el knockdown emote ───────────────────────────────────────────
	// Establecer -1 hace que OnRep_ReplicatedEmoteIndex cancele el emote en todos los clientes
	if (ReplicatedEmoteIndex == KNOCKDOWN_EMOTE_ID)
	{
		ReplicatedEmoteIndex = -1;
		// Servidor: cancelar emote localmente (OnRep no dispara en quien posee la variable)
		if (ActiveEmoteIndex >= 0 || bEmoteBlendingOut)
		{
			CancelEmoteLocalOnly();
		}
	}

	// Restaurar rotación del cuerpo en todas las máquinas
	MulticastApplyKnockdownVisual(false);

	// ── Audio feedback de revive ─────────────────────────────────────────
	StopDBNOHeartbeatSound();
	PlayReviveSuccessSound();

	UE_LOG(LogTemp, Log, TEXT("[Knockdown] %s recovered"), *GetNameSafe(this));
}

void ATortugaCharacter::OnRep_IsKnockedDown()
{
	// ReplicatedEmoteIndex usa COND_SkipOwner: el DUEÑO del pawn nunca recibe
	// OnRep_ReplicatedEmoteIndex cuando el servidor pone KNOCKDOWN_EMOTE_ID.
	// Por eso manejamos aquí TANTO el input COMO el visual del knockdown para
	// el cliente que es el dueño (IsLocallyControlled). Los otros clientes
	// (no dueños) reciben el emote via OnRep_ReplicatedEmoteIndex normalmente.
	if (IsLocallyControlled())
	{
		// El momentum viene del servidor vía replicación de movimiento.
		// Move() bloquea el input mientras bIsKnockedDown = true.
		// Al recuperar, restaurar Walking por si el timer llegó antes de aterrizar.
		if (!bIsKnockedDown)
		{
			if (UCharacterMovementComponent* MC = GetCharacterMovement())
			{
				MC->SetMovementMode(MOVE_Walking);
			}
		}

		// ── Visual knockdown para el dueño (ruta alternativa a OnRep_ReplicatedEmoteIndex) ─
		if (bIsKnockedDown)
		{
			// Arrancar el emote de knockdown localmente — igual que hace el servidor
			// en ApplyKnockdown() para el listen-server player.
			StartEmoteLocally(KNOCKDOWN_EMOTE_ID);
			PlayDBNOHeartbeatSound();
		}
		else
		{
			// Cancelar el emote de knockdown al recuperarse
			if (ActiveEmoteIndex == KNOCKDOWN_EMOTE_ID || bEmoteBlendingOut)
			{
				CancelEmoteLocalOnly();
			}
			StopDBNOHeartbeatSound();
			PlayReviveSuccessSound();
			bCanAirDash = true;  // Restore air dash after knockdown recovery
		}
	}
}

void ATortugaCharacter::MulticastApplyKnockdownVisual_Implementation(bool bKnocked)
{
	// La animación de agitar brazos la emite el sistema de emotes via
	// ReplicatedEmoteIndex = KNOCKDOWN_EMOTE_ID. Aquí aplicamos la rotación
	// simulada del cuerpo (pitch -180°) encima del emote — sin tilt el
	// jugador se ve "flotando" en vez de tumbado. Se ejecuta en TODAS las
	// máquinas para cubrir listen-server + todos los clientes (incluido el
	// dueño, que no recibe OnRep_ReplicatedEmoteIndex por COND_SkipOwner).
	ApplyKnockdownVisual(bKnocked);
}

void ATortugaCharacter::ApplyKnockdownVisual(bool bKnocked)
{
	// ── Ruta A: ragdoll físico (opción 2 del rediseño Q1-07) ───────────────────
	// Si el BP configuró bUsePhysicsRagdoll=true y el SkelMesh tiene PhysicsAsset,
	// activamos ragdoll completo. Sin PhysicsAsset no hay ragdoll posible → cae
	// silenciosamente al tilt manual (Ruta B más abajo) y no rompe nada.
	USkeletalMeshComponent* SkelMesh = GetMesh();
	const bool bCanRagdoll = bUsePhysicsRagdoll && SkelMesh && SkelMesh->GetPhysicsAsset() != nullptr;
	if (bCanRagdoll)
	{
		UCharacterMovementComponent* CMC_Ragdoll = GetCharacterMovement();

		if (bKnocked)
		{
			if (bKnockdownRagdollActive) { return; } // idempotente

			SnapshotSkelMeshRelTransform = SkelMesh->GetRelativeTransform();
			SnapshotSkelMeshCollisionProfile = SkelMesh->GetCollisionProfileName();

			if (HasAuthority())
			{
				SetReplicateMovement(false);
			}

			// KNOCKDOWN: preservar el momentum del LaunchCharacter (plátano, golpes).
			// Capturamos la velocity del CMC ANTES de pararlo para transferirla a los
			// bodies del ragdoll después — si no, el ragdoll arranca inerte y se cae
			// donde estabas sin "resbalar" por el impulso del plátano.
			const FVector KnockdownInitialVel = CMC_Ragdoll ? CMC_Ragdoll->Velocity : FVector::ZeroVector;

			if (CMC_Ragdoll)
			{
				CMC_Ragdoll->NetworkSmoothingMode = ENetworkSmoothingMode::Disabled;
				CMC_Ragdoll->DisableMovement();
				CMC_Ragdoll->StopMovementImmediately();
				CMC_Ragdoll->SetComponentTickEnabled(false);
			}

			if (UCapsuleComponent* Cap = GetCapsuleComponent())
			{
				Cap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}

			SkelMesh->SetCollisionProfileName(RagdollCollisionProfile);
			// Orden Epic: SIMULAR primero, pausar anims DESPUÉS. Evita el frame
			// "semitieso" (bPauseAnims=true congela pose antes de que física arranque).
			SkelMesh->SetAllBodiesSimulatePhysics(true);
			SkelMesh->SetAllBodiesPhysicsBlendWeight(1.f);
			SkelMesh->SetEnableGravity(true);
			SkelMesh->SetAllPhysicsLinearVelocity(KnockdownInitialVel);
			SkelMesh->SetAllPhysicsAngularVelocityInRadians(FVector::ZeroVector);
			SkelMesh->WakeAllRigidBodies();
			SkelMesh->bPauseAnims = true;  // después de simulación activa
			UE_LOG(LogTemp, Warning, TEXT("[Diagnostic] Ragdoll KNOCKDOWN ON (%s) IsSim=%s InitVel=(%.0f,%.0f,%.0f)"),
				*GetName(), SkelMesh->IsSimulatingPhysics()?TEXT("Y"):TEXT("N"),
				KnockdownInitialVel.X, KnockdownInitialVel.Y, KnockdownInitialVel.Z);

			bKnockdownRagdollActive = true;
		}
		else
		{
			if (!bKnockdownRagdollActive) { return; }

			// Mover el capsule a donde cayó el ragdoll antes de desactivar física,
			// para que no teleporte de vuelta a la pos pre-knockdown.
			{
				const FName  RootBone      = SkelMesh->GetBoneName(0);
				const FVector RagdollLoc   = SkelMesh->GetBoneLocation(RootBone, EBoneSpaces::WorldSpace);
				const float  HalfH         = GetCapsuleComponent()
				                             ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
				                             : 0.f;
				SetActorLocation(RagdollLoc + FVector(0.f, 0.f, HalfH),
				                 false, nullptr, ETeleportType::TeleportPhysics);
			}

			SkelMesh->SetAllBodiesPhysicsBlendWeight(0.f);
			SkelMesh->SetAllBodiesSimulatePhysics(false);
			SkelMesh->bPauseAnims = false;
			SkelMesh->SetCollisionProfileName(SnapshotSkelMeshCollisionProfile);

			// NO AttachToComponent: nunca hicimos detach. Solo restaurar la relative
			// transform al snapshot para devolver el mesh a su pose "vivo" sobre el capsule.
			SkelMesh->SetRelativeTransform(SnapshotSkelMeshRelTransform);

			if (UCapsuleComponent* Cap = GetCapsuleComponent())
			{
				Cap->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			}

			if (CMC_Ragdoll)
			{
				CMC_Ragdoll->SetComponentTickEnabled(true);
				CMC_Ragdoll->SetMovementMode(MOVE_Walking);
				CMC_Ragdoll->NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;
			}

			if (HasAuthority())
			{
				SetReplicateMovement(true);
			}

			bKnockdownRagdollActive = false;
		}
		return;
	}

	// ── Ruta B: fallback tilt manual (comportamiento pre-Q1-07) ────────────────
	USceneComponent* VisComp = KnockdownVisualComp.Get();

	// ── Fallback: si el multicast llegó antes de que BeginPlay encontrara el componente,
	// intentar resolverlo ahora con la misma lógica de búsqueda ──
	if (!VisComp)
	{
		// 1) Buscar por nombre configurable
		if (KnockdownComponentName != NAME_None)
		{
			if (USceneComponent* Named = FindChildByName(KnockdownComponentName))
			{
				KnockdownVisualComp = Named;
			}
		}
		// 2) Fallback: SkeletalMesh con asset
		if (!KnockdownVisualComp.IsValid())
		{
			if (USkeletalMeshComponent* FallbackSkelMesh = GetMesh())
			{
				if (FallbackSkelMesh->GetSkeletalMeshAsset())
				{
					KnockdownVisualComp = FallbackSkelMesh;
				}
			}
		}
		if (!KnockdownVisualComp.IsValid())
		{
			for (UActorComponent* Comp : GetComponents())
			{
				if (UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Comp))
				{
					if (SMC != GetRootComponent() && SMC != HelmetMeshComp && SMC->GetStaticMesh())
					{
						KnockdownVisualComp = SMC;
						break;
					}
				}
			}
		}
		if (!KnockdownVisualComp.IsValid())
		{
			TArray<USceneComponent*> AllChildren;
			if (GetRootComponent())
			{
				GetRootComponent()->GetChildrenComponents(true, AllChildren);
			}
			for (USceneComponent* Child : AllChildren)
			{
				if (UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Child))
				{
					if (SMC != HelmetMeshComp && SMC->GetStaticMesh())
					{
						KnockdownVisualComp = SMC;
						break;
					}
				}
			}
		}
		if (!KnockdownVisualComp.IsValid())
		{
			KnockdownVisualComp = GetMesh();
		}
		if (KnockdownVisualComp.IsValid())
		{
			MeshDefaultRelativeRotation = KnockdownVisualComp->GetRelativeRotation();
		}
		VisComp = KnockdownVisualComp.Get();
	}

	if (!VisComp)
	{
		UE_LOG(LogTemp, Error, TEXT("[Knockdown] ApplyKnockdownVisual(%s): No visual component on %s — knockdown tilt invisible! "
			"HasAuthority=%s IsLocal=%s Mesh=%s"),
			bKnocked ? TEXT("true") : TEXT("false"),
			*GetNameSafe(this),
			HasAuthority() ? TEXT("Y") : TEXT("N"),
			IsLocallyControlled() ? TEXT("Y") : TEXT("N"),
			GetMesh() ? *GetNameSafe(GetMesh()) : TEXT("NULL"));
		return;
	}

	UCharacterMovementComponent* CMC = GetCharacterMovement();

	if (bKnocked)
	{
		// ── Desactivar smoothing del CMC para que NO sobreescriba la rotación ──
		if (CMC)
		{
			CMC->NetworkSmoothingMode = ENetworkSmoothingMode::Disabled;
		}

		FRotator KnockedRot = MeshDefaultRelativeRotation;
		KnockedRot.Roll += 180.0f;
		VisComp->SetRelativeRotation(KnockedRot);
	}
	else
	{
		VisComp->SetRelativeRotation(MeshDefaultRelativeRotation);

		// ── Restaurar smoothing al salir del knockdown ─────────────────────
		if (CMC)
		{
			CMC->NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;
		}
	}

	UE_LOG(LogTemp, Verbose, TEXT("[Knockdown] ApplyKnockdownVisual(%s) on %s — comp=%s"),
		bKnocked ? TEXT("true") : TEXT("false"), *GetNameSafe(this), *VisComp->GetName());
}

// ─────────────────────────────────────────────────────────────────────────────
// ── Kill interface ────────────────────────────────────────────────────────────

void ATortugaCharacter::RequestKill(AActor* KillInstigator)
{
	if (!HasAuthority()) { return; }

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) { return; }

	ATN_RunGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ATN_RunGameMode>() : nullptr;
	if (!GM) { return; }

	UE_LOG(LogTemp, Log, TEXT("[Character] RequestKill on '%s' by '%s'"),
		*GetNameSafe(this), *GetNameSafe(KillInstigator));

	GM->MarkPlayerDead(PC);
}

// DEATH VISUAL SYSTEM
// ─────────────────────────────────────────────────────────────────────────────

void ATortugaCharacter::SetDeadVisual(bool bDead)
{
	if (!HasAuthority()) { return; }

	bIsDead = bDead;

	USkeletalMeshComponent* SkelMesh = GetMesh();
	if (bDead)
	{
		EnterRagdollState();

		if (RagdollFreezeAfterSeconds > 0.f)
		{
			GetWorldTimerManager().SetTimer(RagdollFreezeTimerHandle, this,
				&ATortugaCharacter::ServerFireFreezeRagdoll, RagdollFreezeAfterSeconds, false);
		}
	}
	else
	{
		bCanAirDash = true;
		GetWorldTimerManager().ClearTimer(RagdollFreezeTimerHandle);
		ExitRagdollState();
	}

	MulticastSetDeadVisual(bDead);

	UE_LOG(LogTemp, Log, TEXT("[Death] %s dead visual = %s"), *GetNameSafe(this), bDead ? TEXT("RAGDOLL") : TEXT("ALIVE"));
}

void ATortugaCharacter::ServerFireFreezeRagdoll()
{
	if (!HasAuthority()) { return; }

	// Server-auth puro: Tick override ya ha estado sincronizando ActorLocation
	// con root bone vía bReplicateMovement. Los clientes ya están donde toca.
	// El freeze solo necesita: detener SimulatePhysics en server (cliente no
	// tiene físicas activas) y multicastear para que cliente "selle" su SKM
	// en la pose actual interpolada.
	MulticastFreezeRagdoll();
}

void ATortugaCharacter::EnterRagdollState()
{
	USkeletalMeshComponent* SkelMesh = GetMesh();
	if (!SkelMesh || !SkelMesh->GetPhysicsAsset())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Ragdoll] EnterRagdollState abortado — SkM o PhysicsAsset null en %s"), *GetName());
		return;
	}

	// Idempotente: si ya simula no re-arranca.
	if (SkelMesh->IsSimulatingPhysics())
	{
		return;
	}

	// 1. CRÍTICO — detener AnimInstance ANTES de simular física. Si no, el AnimBP
	//    sigue tickeando bones y pelea con el solver físico → impulsos de torque
	//    enormes → "ragdoll explota / sale lanzado". Causa raíz documentada en
	//    foros Epic (search 'ragdoll launches character').
	if (UAnimInstance* AnimInst = SkelMesh->GetAnimInstance())
	{
		AnimInst->Montage_Stop(0.f);
		AnimInst->StopAllMontages(0.f);
	}
	SkelMesh->bPauseAnims = true;

	// 2. Snapshot del state actual para poder revivir limpiamente.
	SnapshotSkelMeshRelTransform    = SkelMesh->GetRelativeTransform();
	SnapshotSkelMeshCollisionProfile = SkelMesh->GetCollisionProfileName();

	// 3. Capsule no colisiona (evita interacción con bodies del SkM ragdoll).
	if (UCapsuleComponent* Cap = GetCapsuleComponent())
	{
		Cap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 4. CMC apagado.
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->StopMovementImmediately();
		CMC->DisableMovement();
		CMC->NetworkSmoothingMode = ENetworkSmoothingMode::Disabled;
		CMC->SetComponentTickEnabled(false);
	}

	// 5. Line trace al suelo: posicionar actor encima del suelo real para que
	//    el SkM ragdoll no arranque penetrando geometría → solver no aplica
	//    impulso de resolución de penetración (que era la causa del lanzamiento).
	if (UWorld* World = GetWorld())
	{
		const FVector ActorLoc = GetActorLocation();
		FHitResult GroundHit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(RagdollGround), false, this);
		Params.AddIgnoredActor(this);
		const FVector TraceStart = ActorLoc + FVector(0.f, 0.f, 100.f);
		const FVector TraceEnd   = ActorLoc - FVector(0.f, 0.f, 1000.f);
		bool bFound = World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_WorldStatic, Params);
		if (!bFound)
		{
			bFound = World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_WorldDynamic, Params);
		}
		if (bFound)
		{
			float HalfHeight = 88.f;
			if (UCapsuleComponent* Cap = GetCapsuleComponent())
			{
				HalfHeight = Cap->GetScaledCapsuleHalfHeight();
			}
			SetActorLocation(FVector(ActorLoc.X, ActorLoc.Y, GroundHit.ImpactPoint.Z + HalfHeight + 5.f),
				false, nullptr, ETeleportType::TeleportPhysics);
		}
	}

	// 6. Collision profile Ragdoll PRIMERO, luego override aislamiento.
	SkelMesh->SetCollisionProfileName(RagdollCollisionProfile);
	SkelMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	SkelMesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	SkelMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);

	// 7. CRÍTICO — bBlendPhysics=true ANTES de SimulatePhysics. Esto le dice al
	//    SkM que la simulación física DOMINE sobre la pose anim. Sin esto, los
	//    bones siguen mezclando con la pose anim cada frame → torque del solver
	//    por la diferencia → cuerpo sale despedido.
	SkelMesh->bBlendPhysics = true;

	// 8. Activar simulación física propiamente.
	SkelMesh->SetSimulatePhysics(true);
	SkelMesh->SetAllBodiesSimulatePhysics(true);
	SkelMesh->SetAllBodiesPhysicsBlendWeight(1.f);
	SkelMesh->SetEnableGravity(true);

	// 9. Vel inicial CERO defensivo (después de simulate, no antes — antes los
	//    bodies aún no existían en el simulator).
	SkelMesh->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
	SkelMesh->SetAllPhysicsAngularVelocityInRadians(FVector::ZeroVector);
	SkelMesh->WakeAllRigidBodies();

	UE_LOG(LogTemp, Warning, TEXT("[Ragdoll] ENTER (%s) authority=%d — bBlendPhysics=true · AnimInst stopped"),
		*GetName(), HasAuthority());
}

void ATortugaCharacter::ExitRagdollState()
{
	USkeletalMeshComponent* SkelMesh = GetMesh();
	if (!SkelMesh) { return; }

	if (SkelMesh->IsSimulatingPhysics())
	{
		SkelMesh->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
		SkelMesh->SetAllPhysicsAngularVelocityInRadians(FVector::ZeroVector);
		SkelMesh->SetAllBodiesSimulatePhysics(false);
		SkelMesh->SetSimulatePhysics(false);
	}
	SkelMesh->SetAllBodiesPhysicsBlendWeight(0.f);
	SkelMesh->bBlendPhysics = false;
	SkelMesh->bPauseAnims = false;
	SkelMesh->SetEnableGravity(false);
	if (SnapshotSkelMeshCollisionProfile != NAME_None)
	{
		SkelMesh->SetCollisionProfileName(SnapshotSkelMeshCollisionProfile);
	}
	SkelMesh->SetRelativeTransform(SnapshotSkelMeshRelTransform);

	if (UCapsuleComponent* Cap = GetCapsuleComponent())
	{
		Cap->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Cap->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		Cap->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		Cap->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	}
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->SetComponentTickEnabled(true);
		CMC->NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;
	}

	UE_LOG(LogTemp, Log, TEXT("[Ragdoll] EXIT (%s) authority=%d"), *GetName(), HasAuthority());
}

void ATortugaCharacter::OnRep_IsDead()
{
	// JIP late-join: cliente recibe bIsDead replicado y aplica el state
	// correspondiente vía las helpers compartidas (mismo patrón canónico).
	if (bIsDead) { EnterRagdollState(); }
	else         { ExitRagdollState();  }
}

void ATortugaCharacter::MulticastFreezeRagdoll_Implementation()
{
	// Detener simulación en TODAS las máquinas, mantener pose final como cadáver.
	USkeletalMeshComponent* SkelMesh = GetMesh();
	if (!SkelMesh) { return; }

	if (SkelMesh->IsSimulatingPhysics())
	{
		SkelMesh->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
		SkelMesh->SetAllPhysicsAngularVelocityInRadians(FVector::ZeroVector);
		SkelMesh->SetAllBodiesSimulatePhysics(false);
	}
	SkelMesh->SetEnableGravity(false);
	UE_LOG(LogTemp, Log, TEXT("[Ragdoll] FROZEN (%s) authority=%d"), *GetName(), HasAuthority());
}

void ATortugaCharacter::MulticastSetDeadVisual_Implementation(bool bDead)
{
	if (HasAuthority()) { return; }  // server ya lo hizo en SetDeadVisual
	if (bDead) { EnterRagdollState(); }
	else       { ExitRagdollState();  }
}

void ATortugaCharacter::HideLimbs()
{
	if (GetMesh()) { GetMesh()->SetVisibility(false, true); }
	if (HelmetMeshComp) { HelmetMeshComp->SetVisibility(false, true); }
}

void ATortugaCharacter::ShowLimbs()
{
	if (GetMesh()) { GetMesh()->SetVisibility(true, true); }
	if (HelmetMeshComp) { HelmetMeshComp->SetVisibility(true, true); }
}

void ATortugaCharacter::ServerTryReviveNearby_Implementation()
{
	if (bIsKnockedDown || bIsDead) { return; }

	// Solo busca jugadores en DBNO (knockdown). Los muertos se reviven vía TN_RescuePickup.
	const float ReviveSearchRadius = ReviveRadiusCm > 0.f ? ReviveRadiusCm : 300.f;
	const FVector MyLocation = GetActorLocation();

	APlayerController* ClosestDBNO_PC = nullptr;
	float ClosestDistSq = ReviveSearchRadius * ReviveSearchRadius;

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* OtherPC = It->Get();
		if (!OtherPC || OtherPC == GetController()) { continue; }

		const ATN_CoopPlayerState* OtherPS = OtherPC->GetPlayerState<ATN_CoopPlayerState>();
		if (!OtherPS || !OtherPS->bIsDBNO) { continue; }

		const APawn* OtherPawn = OtherPC->GetPawn();
		if (!OtherPawn) { continue; }

		const float DistSq = FVector::DistSquared(MyLocation, OtherPawn->GetActorLocation());
		if (DistSq < ClosestDistSq)
		{
			ClosestDistSq = DistSq;
			ClosestDBNO_PC = OtherPC;
		}
	}

	if (!ClosestDBNO_PC)
	{
		UE_LOG(LogTemp, Log, TEXT("[Revive] %s tried to revive but no DBNO player in range"), *GetNameSafe(this));
		return;
	}

	ATN_RunGameMode* RunGM = Cast<ATN_RunGameMode>(GetWorld()->GetAuthGameMode());
	if (RunGM)
	{
		RunGM->RevivePlayer(ClosestDBNO_PC);
		UE_LOG(LogTemp, Log, TEXT("[Revive] %s revived %s via interact"), *GetNameSafe(this), *GetNameSafe(ClosestDBNO_PC));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// REVIVE SYSTEM (DBNO)
// ─────────────────────────────────────────────────────────────────────────────

void ATortugaCharacter::TryStartReviveChannel()
{
	if (!HasAuthority()) { return; }
	if (bIsKnockedDown) { return; }  // Can't revive if you're knocked down yourself

	const FVector MyLoc = GetActorLocation();
	APlayerController* BestTargetPC = nullptr;
	float BestDistSq = ReviveRadiusCm * ReviveRadiusCm;

	// Buscar el jugador más cercano que esté en DBNO o noquedado (puffer-fish).
	// bIsDBNO: sistema de bleedout (actualmente desactivado).
	// bIsKnockedDown: knockdown por puffer-fish — también revivible por compañero.
	if (const UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* OtherPC = It->Get();
			if (!OtherPC || OtherPC == GetController()) { continue; }

			const ATN_CoopPlayerState* OtherPS = OtherPC->GetPlayerState<ATN_CoopPlayerState>();
			const APawn* OtherPawn = OtherPC->GetPawn();
			if (!OtherPawn) { continue; }

			const ATortugaCharacter* OtherTurtle = Cast<ATortugaCharacter>(OtherPawn);
			const bool bTargetNeedsRevive = (OtherPS && OtherPS->bIsDBNO)
				|| (OtherTurtle && OtherTurtle->IsKnockedDown());
			if (!bTargetNeedsRevive) { continue; }

			const float DistSq = FVector::DistSquared(MyLoc, OtherPawn->GetActorLocation());
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				BestTargetPC = OtherPC;
			}
		}
	}

	if (!BestTargetPC)
	{
		return;  // No DBNO player in range
	}

	// Start channeling
	ReviveTargetPC = BestTargetPC;
	ReviveChannelElapsed = 0.f;
	bIsReviving = true;
	ReviveProgress = 0.f;

	// ── Audio: canal de revive (spatialized, todos los cercanos lo oyen) ──
	PlayReviveChannelSound();

	GetWorldTimerManager().SetTimer(ReviveChannelTimerHandle, this,
		&ATortugaCharacter::TickReviveChannel, 0.1f, true);

	UE_LOG(LogTemp, Log, TEXT("[Revive] %s started reviving %s (%.0fcm)"),
		*GetNameSafe(this), *GetNameSafe(BestTargetPC),
		FMath::Sqrt(BestDistSq));
}

void ATortugaCharacter::CancelReviveChannel()
{
	if (!bIsReviving) { return; }

	bIsReviving = false;
	ReviveProgress = 0.f;
	ReviveTargetPC.Reset();
	ReviveChannelElapsed = 0.f;
	GetWorldTimerManager().ClearTimer(ReviveChannelTimerHandle);

	// ── Parar audio del canal de revive ──
	StopReviveChannelSound();

	UE_LOG(LogTemp, Log, TEXT("[Revive] %s cancelled revive channel"), *GetNameSafe(this));
}

void ATortugaCharacter::TickReviveChannel()
{
	if (!HasAuthority())
	{
		CancelReviveChannel();
		return;
	}

	// Validate: reviver is still emoting
	if (ReplicatedEmoteIndex < 0)
	{
		CancelReviveChannel();
		return;
	}

	// Validate: reviver is not knocked down
	if (bIsKnockedDown)
	{
		CancelReviveChannel();
		return;
	}

	// Validate: reviver hasn't finished the race or died
	if (APlayerController* MyPC = Cast<APlayerController>(GetController()))
	{
		if (const ATN_CoopPlayerState* MyPS = MyPC->GetPlayerState<ATN_CoopPlayerState>())
		{
			if (!MyPS->bIsAlive || MyPS->bHasFinishedRun)
			{
				CancelReviveChannel();
				return;
			}
		}
	}

	// Validate: target still valid and in DBNO
	APlayerController* TargetPC = ReviveTargetPC.Get();
	if (!TargetPC)
	{
		CancelReviveChannel();
		return;
	}

	const ATN_CoopPlayerState* TargetPS = TargetPC->GetPlayerState<ATN_CoopPlayerState>();

	// Validate: still in range
	const APawn* TargetPawn = TargetPC->GetPawn();
	if (!TargetPawn)
	{
		CancelReviveChannel();
		return;
	}

	// Validate: target still needs reviving (DBNO o noquedado por puffer-fish)
	const ATortugaCharacter* TargetTurtle = Cast<ATortugaCharacter>(TargetPawn);
	const bool bTargetStillRevivable = (TargetPS && TargetPS->bIsDBNO)
		|| (TargetTurtle && TargetTurtle->IsKnockedDown());
	if (!bTargetStillRevivable)
	{
		// Target ya se recuperó (auto-recover) — cancelar canal limpiamente
		CancelReviveChannel();
		return;
	}

	const float DistSq = FVector::DistSquared(GetActorLocation(), TargetPawn->GetActorLocation());
	if (DistSq > ReviveRadiusCm * ReviveRadiusCm)
	{
		CancelReviveChannel();
		return;
	}

	// Advance channel
	ReviveChannelElapsed += 0.1f;
	ReviveProgress = FMath::Clamp(ReviveChannelElapsed / ReviveDurationSeconds, 0.f, 1.f);

	// Check if complete
	if (ReviveChannelElapsed >= ReviveDurationSeconds)
	{
		UE_LOG(LogTemp, Log, TEXT("[Revive] %s successfully revived %s!"),
			*GetNameSafe(this), *GetNameSafe(TargetPC));

		// Complete the revive via GameMode
		if (ATN_RunGameMode* RunGM = GetWorld()->GetAuthGameMode<ATN_RunGameMode>())
		{
			RunGM->RevivePlayer(TargetPC);
		}

		// Clean up channel state
		CancelReviveChannel();

		// Cancel the reviver's emote (revive is done)
		if (IsLocallyControlled())
		{
			CancelEmote();
		}
		else
		{
			// Server-controlled pawn: force emote cancel
			ReplicatedEmoteIndex = -1;
			if (ActiveEmoteIndex >= 0 || bEmoteBlendingOut)
			{
				CancelEmote();
			}
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// EMOTE SYSTEM
// ─────────────────────────────────────────────────────────────────────────────

void ATortugaCharacter::TriggerEmote(int32 Index)
{
	if (!IsLocallyControlled() || !IsValidWheelEmoteId(Index)) { return; }

	// Start locally for immediate visual feedback
	StartEmoteLocally(Index);

	// Replicate to server → other clients
	ServerSetEmote(Index);
}

void ATortugaCharacter::RequestWheelEmote(uint8 EmoteID)
{
	TriggerEmote(static_cast<int32>(EmoteID));
}

bool ATortugaCharacter::IsValidWheelEmoteId(int32 EmoteID) const
{
	if (EmoteID < 0)
	{
		return false;
	}

	if (!EmoteWheelDataAsset)
	{
		return EmoteID >= 0 && EmoteID <= 9;
	}

	return EmoteWheelDataAsset->FindEntryById(static_cast<uint8>(EmoteID)) != nullptr;
}

const FTN_EmoteWheelEntry* ATortugaCharacter::ResolveWheelEmoteEntry(int32 EmoteID) const
{
	if (!EmoteWheelDataAsset || EmoteID < 0)
	{
		return nullptr;
	}

	return EmoteWheelDataAsset->FindEntryById(static_cast<uint8>(EmoteID));
}

float ATortugaCharacter::GetWheelEmoteCooldown(int32 EmoteID) const
{
	if (const FTN_EmoteWheelEntry* Entry = ResolveWheelEmoteEntry(EmoteID))
	{
		return Entry->Cooldown;
	}

	return 0.f;
}

void ATortugaCharacter::PlayWheelEmoteMontage(int32 EmoteID)
{
	const FTN_EmoteWheelEntry* Entry = ResolveWheelEmoteEntry(EmoteID);
	if (!Entry || !Entry->Montage)
	{
		return;
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}

	if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
	{
		AnimInstance->Montage_Stop(0.05f, Entry->Montage);
		AnimInstance->Montage_Play(Entry->Montage, 1.f);
	}
}

void ATortugaCharacter::StopWheelEmoteMontage(int32 EmoteID, float BlendOutTime)
{
	const FTN_EmoteWheelEntry* Entry = ResolveWheelEmoteEntry(EmoteID);
	if (!Entry || !Entry->Montage)
	{
		return;
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}

	if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
	{
		AnimInstance->Montage_Stop(FMath::Max(0.f, BlendOutTime), Entry->Montage);
	}
}

void ATortugaCharacter::StartEmoteLocally(int32 Index)
{
	const int32 PreviousEmoteIndex = ActiveEmoteIndex;
	StopWheelEmoteMontage(PreviousEmoteIndex, 0.05f);

	// Reset all bones to rest pose (clears stale location offsets)
	SetAnimBoneRot(Brazo1Bone, Brazo1RestRot); SetAnimBoneLoc(Brazo1Bone, Brazo1RestLoc);
	SetAnimBoneRot(Brazo2Bone, Brazo2RestRot); SetAnimBoneLoc(Brazo2Bone, Brazo2RestLoc);
	SetAnimBoneRot(Pata1Bone,  Pata1RestRot);  SetAnimBoneLoc(Pata1Bone,  Pata1RestLoc);
	SetAnimBoneRot(Pata2Bone,  Pata2RestRot);  SetAnimBoneLoc(Pata2Bone,  Pata2RestLoc);
	SetAnimBoneRot(ColaBone,   ColaRestRot);   SetAnimBoneLoc(ColaBone,   ColaRestLoc);
	SetAnimBoneRot(CabezaBone, CabezaRestRot); SetAnimBoneLoc(CabezaBone, CabezaRestLoc);

	// (Re)start — pressing the same emote while it plays restarts from the top.
	ActiveEmoteIndex   = Index;
	EmoteTime          = 0.f;
	bEmoteBlendingOut  = false;
	EmoteBlendOutTimer = 0.f;

	// El knockdown emote (ID=100) es interno — no tiene audio ni montaje.
	if (Index != KNOCKDOWN_EMOTE_ID)
	{
		PlayEmoteSound(Index);
		PlayWheelEmoteMontage(Index);
	}
}

void ATortugaCharacter::ServerSetEmote_Implementation(int32 Index)
{
	if (Index >= 0)
	{
		if (!IsValidWheelEmoteId(Index))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Emote] ID inválido %d en %s"), Index, *GetNameSafe(this));
			ClientRejectEmote(Index);
			return;
		}

		if (ATN_CoopPlayerState* TNPS = GetPlayerState<ATN_CoopPlayerState>())
		{
			if (!TNPS->bIsAlive || TNPS->bIsDBNO || bIsKnockedDown)
			{
				ClientRejectEmote(Index);
				return;
			}

			const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
			const float Cooldown = GetWheelEmoteCooldown(Index);
			if (!TNPS->CanServerPlayEmote(static_cast<uint8>(Index), Now, Cooldown))
			{
				ClientRejectEmote(Index);
				return;
			}

			TNPS->MarkServerEmotePlayed(static_cast<uint8>(Index), Now);
		}
	}

	// Siempre actualizar el valor replicado, incluso si el emote ya acabó localmente.
	// Esto garantiza que TODOS los clientes remotos reciban OnRep.
	ReplicatedEmoteIndex = Index;

	// Listen server: OnRep no dispara en la máquina que posee la variable,
	// así que arrancamos/cancelamos el emote directamente para pawns remotos.
	if (!IsLocallyControlled())
	{
		if (Index >= 0)
		{
			StartEmoteLocally(Index);
		}
		else
		{
			// Solo cancelar si hay emote activo o blend pendiente
			if (ActiveEmoteIndex >= 0 || bEmoteBlendingOut)
			{
				CancelEmote();
			}
		}
	}

	// ── DBNO Revive: emote triggers revive channel if near a downed teammate ──
	if (Index >= 0)
	{
		TryStartReviveChannel();
	}
	else
	{
		CancelReviveChannel();
	}

	UE_LOG(LogTemp, Verbose, TEXT("[Emote] ServerSetEmote(%d) on %s  CabezaBone=%s"),
		Index, *GetNameSafe(this),
		CabezaBone != NAME_None ? TEXT("OK") : TEXT("NULL"));
}

void ATortugaCharacter::OnRep_ReplicatedEmoteIndex()
{
	if (ReplicatedEmoteIndex < 0)
	{
		// Cancelación forzada por el servidor (ej: recuperación de knockdown).
		// Cancelar en TODOS los clientes, independientemente del control local.
		// CancelEmoteLocalOnly es un no-op si ya no hay emote activo.
		if (ActiveEmoteIndex >= 0 || bEmoteBlendingOut)
		{
			CancelEmoteLocalOnly();  // NO llama ServerSetEmote(-1) — evita bucle
		}
	}
	else if (!IsLocallyControlled())
	{
		// Emote arrancado remotamente: aplicar en clientes que no controlan este pawn
		StartEmoteLocally(ReplicatedEmoteIndex);
	}
	// Si IsLocallyControlled() y el índice >= 0: el jugador ya arrancó el emote
	// localmente en TriggerEmote() antes de enviar el RPC, no hay que hacer nada.

	UE_LOG(LogTemp, Verbose, TEXT("[Emote] OnRep_ReplicatedEmoteIndex(%d) on %s  IsLocal=%s  CabezaBone=%s"),
		ReplicatedEmoteIndex, *GetNameSafe(this),
		IsLocallyControlled() ? TEXT("YES") : TEXT("NO"),
		CabezaBone != NAME_None ? TEXT("OK") : TEXT("NULL"));
}

void ATortugaCharacter::ClientRejectEmote_Implementation(int32 Index)
{
	if (ActiveEmoteIndex == Index || bEmoteBlendingOut)
	{
		CancelEmoteLocalOnly();
	}
}

void ATortugaCharacter::CancelEmoteLocalOnly()
{
	if (ActiveEmoteIndex < 0 && !bEmoteBlendingOut) { return; }

	const int32 EmoteIdToStop = ActiveEmoteIndex;

	SnapshotBrazo1    = GetAnimBoneRot(Brazo1Bone); SnapshotBrazo1Loc = GetAnimBoneLoc(Brazo1Bone);
	SnapshotBrazo2    = GetAnimBoneRot(Brazo2Bone); SnapshotBrazo2Loc = GetAnimBoneLoc(Brazo2Bone);
	SnapshotPata1     = GetAnimBoneRot(Pata1Bone);  SnapshotPata1Loc  = GetAnimBoneLoc(Pata1Bone);
	SnapshotPata2     = GetAnimBoneRot(Pata2Bone);  SnapshotPata2Loc  = GetAnimBoneLoc(Pata2Bone);
	SnapshotCola      = GetAnimBoneRot(ColaBone);   SnapshotColaLoc   = GetAnimBoneLoc(ColaBone);
	SnapshotCabeza    = GetAnimBoneRot(CabezaBone); SnapshotCabezaLoc = GetAnimBoneLoc(CabezaBone);

	// Si cancela el emote de knockdown, capturar también la rotación del cuerpo para restaurarla.
	bKnockdownCompSnapshotValid = (EmoteIdToStop == KNOCKDOWN_EMOTE_ID) && KnockdownVisualComp.IsValid();
	if (bKnockdownCompSnapshotValid)
	{
		SnapshotKnockdownComp = KnockdownVisualComp->GetRelativeRotation();
	}

	ActiveEmoteIndex   = -1;
	bEmoteBlendingOut  = true;
	EmoteBlendOutTimer = 0.f;
	StopEmoteSound();
	StopWheelEmoteMontage(EmoteIdToStop, EmoteBlendOutDuration);
}

void ATortugaCharacter::CancelEmote()
{
	if (ActiveEmoteIndex < 0 && !bEmoteBlendingOut)
	{
		return;
	}

	CancelEmoteLocalOnly();

	// Replicar cancelación a otros clientes (solo desde el jugador que controla)
	if (IsLocallyControlled())
	{
		ServerSetEmote(-1);
	}
}

void ATortugaCharacter::SetAnimBoneRot(FName BoneName, const FRotator& Rot) const
{
	if (BoneName == NAME_None) { return; }
	if (UTN_ProcAnimInstance* Inst = Cast<UTN_ProcAnimInstance>(GetMesh() ? GetMesh()->GetAnimInstance() : nullptr))
		Inst->BoneQuat.Add(BoneName, FQuat(Rot));
}

FRotator ATortugaCharacter::GetAnimBoneRot(FName BoneName) const
{
	if (BoneName == NAME_None || !GetMesh()) { return FRotator::ZeroRotator; }
	if (const UTN_ProcAnimInstance* Inst = Cast<UTN_ProcAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		if (const FQuat* Q = Inst->BoneQuat.Find(BoneName)) { return Q->Rotator(); }
	}
	return GetMesh()->GetBoneQuaternion(BoneName, EBoneSpaces::ComponentSpace).Rotator();
}

void ATortugaCharacter::SetAnimBoneLoc(FName BoneName, const FVector& Loc) const
{
	if (BoneName == NAME_None) { return; }
	if (UTN_ProcAnimInstance* Inst = Cast<UTN_ProcAnimInstance>(GetMesh() ? GetMesh()->GetAnimInstance() : nullptr))
		Inst->BoneLoc.Add(BoneName, Loc);
}

void ATortugaCharacter::SetAnimBoneScale(FName BoneName, const FVector& Scale) const
{
	if (BoneName == NAME_None) { return; }
	if (UTN_ProcAnimInstance* Inst = Cast<UTN_ProcAnimInstance>(GetMesh() ? GetMesh()->GetAnimInstance() : nullptr))
	{
		if (Scale.Equals(FVector::OneVector)) { Inst->BoneScale.Remove(BoneName); }
		else                                  { Inst->BoneScale.Add(BoneName, Scale); }
	}
}

FVector ATortugaCharacter::GetAnimBoneLoc(FName BoneName) const
{
	if (BoneName == NAME_None || !GetMesh()) { return FVector::ZeroVector; }
	if (const UTN_ProcAnimInstance* Inst = Cast<UTN_ProcAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		if (const FVector* L = Inst->BoneLoc.Find(BoneName)) { return *L; }
	}
	return GetMesh()->GetBoneLocation(BoneName, EBoneSpaces::ComponentSpace);
}

void ATortugaCharacter::ApplyEmoteAngle(FName BoneName, const FRotator& Rest,
                                        float AngleDeg, const FVector& Axis) const
{
	const FQuat SwingQuat(Axis.GetSafeNormal(), FMath::DegreesToRadians(AngleDeg));
	SetAnimBoneRot(BoneName, (SwingQuat * FQuat(Rest)).Rotator());
}

void ATortugaCharacter::ApplyEmoteAngles2(FName BoneName, const FRotator& Rest,
                                          float A1, const FVector& Ax1,
                                          float A2, const FVector& Ax2) const
{
	const FQuat Q1(Ax1.GetSafeNormal(), FMath::DegreesToRadians(A1));
	const FQuat Q2(Ax2.GetSafeNormal(), FMath::DegreesToRadians(A2));
	SetAnimBoneRot(BoneName, (Q2 * Q1 * FQuat(Rest)).Rotator());
}

void ATortugaCharacter::ApplyEmoteAngles3(FName BoneName, const FRotator& Rest,
                                          float A1, const FVector& Ax1,
                                          float A2, const FVector& Ax2,
                                          float A3, const FVector& Ax3) const
{
	const FQuat Q1(Ax1.GetSafeNormal(), FMath::DegreesToRadians(A1));
	const FQuat Q2(Ax2.GetSafeNormal(), FMath::DegreesToRadians(A2));
	const FQuat Q3(Ax3.GetSafeNormal(), FMath::DegreesToRadians(A3));
	SetAnimBoneRot(BoneName, (Q3 * Q2 * Q1 * FQuat(Rest)).Rotator());
}

void ATortugaCharacter::TickEmote(float DeltaTime)
{
	// Dive owns all limb components while active — skip emote updates entirely
	if (bIsDiving || DiveTiltAlpha > 0.f) { return; }

	// ── Blend-out: lerp all components back to rest rotations & locations ─────
	if (bEmoteBlendingOut)
	{
		EmoteBlendOutTimer += DeltaTime;
		const float Alpha = FMath::Min(EmoteBlendOutTimer / FMath::Max(EmoteBlendOutDuration, KINDA_SMALL_NUMBER), 1.f);

		auto Blend = [&](FName Bone, const FRotator& SnapR, const FRotator& RestR,
		                 const FVector& SnapL, const FVector& RestL)
		{
			SetAnimBoneRot(Bone, FMath::Lerp(SnapR, RestR, Alpha));
			SetAnimBoneLoc(Bone, FMath::Lerp(SnapL, RestL, Alpha));
		};

		Blend(Brazo1Bone, SnapshotBrazo1, Brazo1RestRot, SnapshotBrazo1Loc, Brazo1RestLoc);
		Blend(Brazo2Bone, SnapshotBrazo2, Brazo2RestRot, SnapshotBrazo2Loc, Brazo2RestLoc);
		Blend(Pata1Bone,  SnapshotPata1,  Pata1RestRot,  SnapshotPata1Loc,  Pata1RestLoc);
		Blend(Pata2Bone,  SnapshotPata2,  Pata2RestRot,  SnapshotPata2Loc,  Pata2RestLoc);
		Blend(ColaBone,   SnapshotCola,   ColaRestRot,   SnapshotColaLoc,   ColaRestLoc);
		Blend(CabezaBone, SnapshotCabeza, CabezaRestRot, SnapshotCabezaLoc, CabezaRestLoc);

		// Restaurar la rotación del cuerpo si era un knockdown emote
		if (bKnockdownCompSnapshotValid && KnockdownVisualComp.IsValid())
		{
			KnockdownVisualComp->SetRelativeRotation(
				FMath::Lerp(SnapshotKnockdownComp, MeshDefaultRelativeRotation, Alpha));
		}

		if (Alpha >= 1.f)
		{
			bEmoteBlendingOut = false;
			bKnockdownCompSnapshotValid = false;
			// Asegurar restauración exacta al finalizar blend-out
			if (KnockdownVisualComp.IsValid())
			{
				KnockdownVisualComp->SetRelativeRotation(MeshDefaultRelativeRotation);
			}
		}
		return;
	}

	if (ActiveEmoteIndex < 0) { return; }

	EmoteTime += DeltaTime;
	const float T = EmoteTime;

	// Trig helpers (frequency in Hz)
	auto S    = [](float Hz, float t) { return FMath::Sin(2.f * PI * Hz * t); };
	auto Cos  = [](float Hz, float t) { return FMath::Cos(2.f * PI * Hz * t); };
	auto Sat  = [](float v)           { return FMath::Clamp(v, 0.f, 1.f); };

	// ── Ejes de referencia (espacio de componente — SKM unificado) ──────────
	// El nuevo SKM unificado tiene los huesos rotados ~90° Z respecto al blockout.
	// Todos los ejes se han rotado R_z(-90°): (1,0,0)→(0,-1,0), (0,1,0)→(1,0,0).
	// AZ y LZ permanecen iguales (Z es invariante bajo rotaciones Z).
	//
	// BRAZOS: AX sube/baja (+AX arriba), AY roll eje largo, AZ adelante/atrás
	// CABEZA: AX = roll, AY = cabeceo nod, AZ = shake izq/dcha
	// PATAS:  LX = splay lateral (abrir/cerrar), LY = stride (configurable), LZ = twist
	// COLA:   TY = arriba/abajo, TZ = lado a lado, TX = roll
	const FVector AX(0.f, -1.f, 0.f);
	const FVector AY(1.f,  0.f, 0.f);
	const FVector AZ(0.f,  0.f, 1.f);

	// Patas
	const FVector LX(0.f, -1.f, 0.f);       // splay lateral
	const FVector LY = LegSwingAxis;          // stride adelante/atrás (configurable)
	const FVector LZ(0.f,  0.f, 1.f);       // giro pata

	// Cola
	const FVector TY = TailUpDownAxis;
	const FVector TZ = TailSideAxis;
	const FVector TX(0.f, -1.f, 0.f);       // roll cola

	// ── Helpers de aplicación (rotación) ──────────────────────────────────────
	auto Ap = [&](FName Bone, const FRotator& Rest, float Angle, const FVector& Axis)
	{
		ApplyEmoteAngle(Bone, Rest, Angle, Axis);
	};

	auto Ap2 = [&](FName Bone, const FRotator& Rest,
	               float A1, const FVector& Ax1, float A2, const FVector& Ax2)
	{
		ApplyEmoteAngles2(Bone, Rest, A1, Ax1, A2, Ax2);
	};

	auto Ap3 = [&](FName Bone, const FRotator& Rest,
	               float A1, const FVector& Ax1,
	               float A2, const FVector& Ax2,
	               float A3, const FVector& Ax3)
	{
		ApplyEmoteAngles3(Bone, Rest, A1, Ax1, A2, Ax2, A3, Ax3);
	};

	// ── Helper de aplicación (traslación) — para Explosivo y Modo Loco 2 ────
	auto SetLoc = [&](FName Bone, const FVector& RestLoc, const FVector& Offset)
	{
		SetAnimBoneLoc(Bone, RestLoc + Offset);
	};

	bool bEnded = false;

	switch (ActiveEmoteIndex)
	{
	// ──────────────────────────────────────────────────────────────────────────
	// 0  SALUDAR — Brazo1 (dcha): AY 90° posiciona + AX ±75° aleteo.
	//              Brazo2 (izq) baja.  2.5 s
	// ──────────────────────────────────────────────────────────────────────────
	case 0:
	{
		const float setup   = Sat(T / 0.3f);
		const float fadeOut = (T > 2.0f) ? Sat((T - 2.0f) / 0.5f) : 0.f;
		const float env     = setup * (1.f - fadeOut);
		const float wave    = (T > 0.3f) ? S(5.f, T) : 0.f;

		// Brazo1 (dcha): AY 90° posiciona brazo, AX ±75° oscila (aleteo)
		Ap2(Brazo1Bone, Brazo1RestRot,
		    env * 90.f,              AY,
		    env * 75.f * wave,       AX);

		// Brazo2 (izq): baja relajado. EMOTE-01: signo negado — con ANIM-01 el eje AX
		// apunta a (0,-1,0), así que para que la mano quede ABAJO hace falta -80°.
		Ap(Brazo2Bone, Brazo2RestRot, env * -80.f, AX);

		// Cabeza — cabeceo visible mientras saluda
		Ap(CabezaBone, CabezaRestRot, env * 20.f * S(2.f, T), AY);

		// Cola
		Ap(ColaBone, ColaRestRot, env * 10.f * S(1.5f, T), TZ);

		bEnded = (T >= 2.5f);
		break;
	}

	// ──────────────────────────────────────────────────────────────────────────
	// 1  APLAUSO — Brazos en V cerrada, palmaditas rápidas.             3 s
	// ──────────────────────────────────────────────────────────────────────────
	case 1:
	{
		const float rise    = Sat(T / 0.4f);
		const float fadeOut = (T > 2.5f) ? Sat((T - 2.5f) / 0.5f) : 0.f;
		const float env     = rise * (1.f - fadeOut);
		const float bob     = (T > 0.4f) ? 12.f * S(4.f, T) : 0.f;   // 4 Hz, ±12°

		// Brazo1 (dcha): arriba +AX 50° (más cerrado) + palma al frente AY 90°
		Ap2(Brazo1Bone, Brazo1RestRot,
		    env * 110.f + bob * env,   AX,
		    env * 90.f,               AY);
		// Brazo2 (izq): arriba −AX 50° + palma al frente AY +90°
		Ap2(Brazo2Bone, Brazo2RestRot,
		    -(env * 110.f + bob * env), AX,
		    env * 90.f,                AY);

		// Cabeza — mira hacia las manos que aplauden
		Ap(CabezaBone, CabezaRestRot, env * (-25.f), AY);

		// Patas
		Ap(Pata1Bone, Pata1RestRot,  10.f * S(2.f, T) * env, LY);
		Ap(Pata2Bone, Pata2RestRot, -10.f * S(2.f, T) * env, LY);

		// Cola
		Ap(ColaBone, ColaRestRot, env * 20.f * S(3.f, T), TZ);

		bEnded = (T >= 3.f);
		break;
	}

	// ──────────────────────────────────────────────────────────────────────────
	// 2  HELICÓPTERO — Brazos giran en AZ. Patas: ±90° en eje frontal (LX)
	//                  y luego giran en LZ igual que los brazos.      LOOP ∞
	// ──────────────────────────────────────────────────────────────────────────
	case 2:
	{
		// Spin continuo: 8 vueltas/seg
		const float spin = 360.f * T * 8.f;

		// Brazos: spin clásico en AZ (propulsor horizontal)
		Ap(Brazo1Bone, Brazo1RestRot,  spin, AZ);
		Ap(Brazo2Bone, Brazo2RestRot, -spin, AZ);

		// Patas: en los primeros 0.4s se posicionan ±90° en el eje frontal (LX)
		// y acto seguido giran en LZ igual que los brazos — efecto hélice
		const float legSetup = Sat(T / 0.4f);
		Ap2(Pata1Bone, Pata1RestRot,
		     legSetup *  90.f,  LX,   // eje frontal +90°
		     spin,              LZ);  // giro continuo
		Ap2(Pata2Bone, Pata2RestRot,
		     legSetup * -90.f,  LX,   // eje frontal -90° (opuesto)
		     spin,              LZ);  // giro continuo

		// Cabeza — cabeceo al ritmo del spin
		Ap(CabezaBone, CabezaRestRot, 20.f * S(2.f, T), AY);


		// Cola
		Ap(ColaBone, ColaRestRot, 12.f * S(1.f, T), TZ);

		break; // LOOP ∞
	}

	// ──────────────────────────────────────────────────────────────────────────
	// 3  PALMADA POTENTE — Ambos brazos: AX 90° + AZ 90° posición,
	//     luego AY oscila −90° rápido abajo, lento arriba.            LOOP ∞
	// ──────────────────────────────────────────────────────────────────────────
	case 3:
	{
		// 0.3s para que los brazos lleguen a la posición de partida
		const float setup = Sat(T / 0.3f);

		// Palmada asimétrica: baja rápido (25% ciclo), sube normal (50% ciclo), pausa (25%)
		const float clapT     = FMath::Max(T - 0.3f, 0.f);
		const float cycleTime = 0.5f;   // 2 palmadas/segundo
		float palmada = 0.f;
		if (clapT > 0.f)
		{
			const float phase = FMath::Fmod(clapT, cycleTime) / cycleTime;
			if (phase < 0.25f)
			{
				// Fast down: 0 → −90° en 25% del ciclo
				palmada = -90.f * (phase / 0.25f);
			}
			else if (phase < 0.75f)
			{
				// Normal up: −90° → 0 en 50% del ciclo
				palmada = -90.f * (1.f - (phase - 0.25f) / 0.50f);
			}
			// else: 25% pausa en reposo (brazos arriba antes de la siguiente palmada)
		}

		// Brazo1 (dcha): AX 90° sube + AZ 90° centra + AY palmada
		Ap3(Brazo1Bone, Brazo1RestRot,
		    setup *  -90.f,  AX,
		    setup *  -90.f,  AZ,
		    palmada,        AY);
		// Brazo2 (izq): espejo (−AX, −AZ, misma AY)
		Ap3(Brazo2Bone, Brazo2RestRot,
		    setup * 90.f,  AX,
		    setup * 90.f,  AZ,
		    palmada,        AY);

		// Cabeza asiente con cada palmada
		Ap(CabezaBone, CabezaRestRot, 18.f * (palmada / -90.f), AY);

		// Cola menea
		Ap(ColaBone, ColaRestRot, 15.f * S(2.f, T), TZ);

		break; // LOOP ∞
	}

	// ──────────────────────────────────────────────────────────────────────────
	// 4  APLAUDIR — Brazo1 (dcha): AX 120° + AY (20° + ±10° clap).
	//              Brazo2 (izq): reposo abajo. Cola: TY 180° rápido.   2 s
	// ──────────────────────────────────────────────────────────────────────────
	case 4:
	{
		const float setup   = Sat(T / 0.3f);
		const float clap    = S(6.f, T);
		const float colSnap = Sat(T / 0.2f); // cola rápido a posición

		// Brazo1 (dcha): AX 120° sube + AY 20° orienta + AY ±10° palmada rápida
		Ap2(Brazo1Bone, Brazo1RestRot,
		    setup * -120.f,                      AX,
		    setup * (-20.f - 10.f * clap),       AY);

		// Brazo2 (izq): reposo abajo
		Ap(Brazo2Bone, Brazo2RestRot, setup * 80.f, AX);

		// Cabeza — cabeceo al ritmo del aplauso
		Ap(CabezaBone, CabezaRestRot, 15.f * S(6.f, T), AY);

		// Cola: entra rápido por −TY (0.2s), sale por el mismo lado (0.4s a partir de T=1.5)
		// — la vuelta es explícita para que no tome el arco opuesto en el blend-out.
		const float colOut = T > 2.5f ? Sat((T - 2.5f) / 0.1f) : 0.f;
		const float colEnv = colSnap * (1.f - colOut);
		Ap(ColaBone, ColaRestRot, colEnv * -175.f, TY);

		bEnded = (T >= 2.f);
		break;
	}

	// ──────────────────────────────────────────────────────────────────────────
	// 5  BAILE IRLANDÉS — Ambos brazos se juntan por DETRÁS.         LOOP ∞
	// ──────────────────────────────────────────────────────────────────────────
	case 5:
	{
		const float setup = Sat(T / 0.4f);

		// Brazo1 (dcha): atrás (+AZ), baja un poco (−AX)
		Ap2(Brazo1Bone, Brazo1RestRot,
		    setup * (-10.f),    AX,
		    setup *  80.f,      AZ);
		// Brazo2 (izq):  atrás (−AZ), baja un poco (+AX = abajo para izq)
		Ap2(Brazo2Bone, Brazo2RestRot,
		    setup *  10.f,      AX,
		    setup * (-80.f),    AZ);

		// Cabeza — gira al ritmo del baile
		Ap(CabezaBone, CabezaRestRot, 15.f * S(2.5f, T), AZ);

		// Patas
		Ap2(Pata1Bone, Pata1RestRot,
		     30.f * S(2.5f, T),   LY,
		     15.f * S(1.25f, T),  LX);
		Ap2(Pata2Bone, Pata2RestRot,
		    -30.f * S(2.5f, T),   LY,
		    -15.f * S(1.25f, T),  LX);

		// Cola
		Ap(ColaBone, ColaRestRot, 10.f * S(2.5f, T), TZ);

		break; // LOOP ∞
	}

	// ──────────────────────────────────────────────────────────────────────────
	// 6  FLOTAR (SUPERMAN) — bien                                       5 s
	// ──────────────────────────────────────────────────────────────────────────
	case 6:
	{
		const float fadeIn  = Sat(T / 1.f);
		const float fadeOut = (T > 4.f) ? Sat((T - 4.f) / 1.f) : 0.f;
		const float env     = fadeIn * (1.f - fadeOut);
		const float drift   = 3.f * S(0.3f, T);

		Ap2(Brazo1Bone, Brazo1RestRot,
		    env * 50.f + drift,    AX,
		    env * (-80.f),         AZ);
		Ap2(Brazo2Bone, Brazo2RestRot,
		    -(env * 50.f) + drift, AX,
		    env * 80.f,            AZ);

		Ap(CabezaBone, CabezaRestRot, env * (-20.f) + drift, AY);

		Ap(Pata1Bone, Pata1RestRot, (env * 80.f) + drift, LY);
		Ap(Pata2Bone, Pata2RestRot, (env * 80.f) + drift, LY);

		Ap2(ColaBone, ColaRestRot,
		    env * 20.f + drift,  TY,
		    drift * 2.f,         TZ);

		bEnded = (T >= 5.f);
		break;
	}

	// ──────────────────────────────────────────────────────────────────────────
	// 7  SEÑALAR — Brazo1 apunta, Brazo2 baja reposo.                  1.2 s
	// ──────────────────────────────────────────────────────────────────────────
	case 7:
	{
		const float rise    = Sat(T / 0.1f);
		const float fadeOut = (T > 0.8f) ? Sat((T - 0.8f) / 0.4f) : 0.f;
		const float env     = rise * (1.f - fadeOut);

		// Brazo1 (dcha): apunta adelante
		Ap2(Brazo1Bone, Brazo1RestRot,
		    env * 25.f,        AX,
		    env * (-80.f),     AZ);

		// Brazo2 (izq): reposo abajo
		Ap(Brazo2Bone, Brazo2RestRot, env * 80.f, AX);

		Ap(CabezaBone, CabezaRestRot, env * 20.f, AY);
		Ap(ColaBone, ColaRestRot, env * 8.f * S(1.5f, T), TZ);

		bEnded = (T >= 1.2f);
		break;
	}

	// ──────────────────────────────────────────────────────────────────────────
	// 8  MODO LOCO 2 — Caos TOTAL: todos los componentes rotan y se
	//     trasladan a lo bestia (incluye Brazo2).                     LOOP ∞
	// ──────────────────────────────────────────────────────────────────────────
	case 8:
	{
		// Rotación salvaje en TODOS los componentes
		Ap3(Brazo1Bone, Brazo1RestRot,
		     50.f * S(3.0f, T),          AX,
		     40.f * S(2.1f, T),          AY,
		     20.f * S(4.7f, T),          AZ);
		Ap3(Brazo2Bone, Brazo2RestRot,
		    -55.f * S(2.8f, T + 0.1f),   AX,
		    -40.f * S(3.3f, T + 0.2f),   AY,
		     25.f * S(4.1f, T),           AZ);
		Ap3(Pata1Bone, Pata1RestRot,
		     35.f * S(4.0f, T),          LY,
		     20.f * S(2.7f, T),          LX,
		     10.f * S(5.0f, T),          LZ);
		Ap3(Pata2Bone, Pata2RestRot,
		    -35.f * S(4.0f, T + 0.08f),  LY,
		    -20.f * S(2.7f, T + 0.15f),  LX,
		    -10.f * S(5.0f, T + 0.05f),  LZ);
		Ap3(ColaBone, ColaRestRot,
		    15.f * Cos(5.f, T),           TY,
		    25.f * S(7.f, T),             TZ,
		    10.f * S(3.f, T + 0.1f),      TX);
		Ap3(CabezaBone, CabezaRestRot,
		    25.f * S(3.7f, T),            AY,
		    30.f * S(2.3f, T + 0.07f),    AZ,
		    15.f * S(5.5f, T),            AX);

		// Traslación: HIPER-EXAGERADA tipo explosión — ±250 cm
		const float locAmp = 250.f;
		SetLoc(Brazo1Bone, Brazo1RestLoc, FVector(
		    locAmp * S(1.7f, T),  locAmp * S(2.3f, T + 0.1f),  locAmp * S(3.1f, T)));
		SetLoc(Brazo2Bone, Brazo2RestLoc, FVector(
		    locAmp * S(2.1f, T + 0.05f), -locAmp * S(1.9f, T),  locAmp * S(3.5f, T + 0.1f)));
		SetLoc(Pata1Bone, Pata1RestLoc, FVector(
		    locAmp * S(2.9f, T), -locAmp * S(1.3f, T + 0.15f), locAmp * S(3.7f, T)));
		SetLoc(Pata2Bone, Pata2RestLoc, FVector(
		   -locAmp * S(2.3f, T + 0.1f),  locAmp * S(1.7f, T),  locAmp * Cos(3.3f, T)));
		SetLoc(ColaBone, ColaRestLoc, FVector(
		   -locAmp * S(1.1f, T), locAmp * S(2.9f, T + 0.05f), locAmp * S(4.1f, T)));
		SetLoc(CabezaBone, CabezaRestLoc, FVector(
		    locAmp * 0.5f * S(2.5f, T), locAmp * 0.3f * S(3.1f, T + 0.12f), locAmp * 0.4f * S(4.3f, T)));

		break; // LOOP ∞
	}

	// ──────────────────────────────────────────────────────────────────────────
	// 9  FIESTA (MODO LOCO) — Caos puro solo rotación, sin separar.     LOOP ∞
	// ──────────────────────────────────────────────────────────────────────────
	case 9:
		Ap3(Brazo1Bone, Brazo1RestRot,
		     50.f * S(3.0f, T),          AX,
		     40.f * S(2.1f, T),          AY,
		     20.f * S(4.7f, T),          AZ);
		Ap3(Brazo2Bone, Brazo2RestRot,
		    -55.f * S(2.3f, T + 0.13f),  AX,
		    -35.f * S(3.1f, T),          AY,
		     25.f * S(1.7f, T + 0.08f),  AZ);
		Ap3(Pata1Bone, Pata1RestRot,
		     35.f * S(4.0f, T),          LY,
		     20.f * S(2.7f, T),          LX,
		     10.f * S(5.0f, T),          LZ);
		Ap3(Pata2Bone, Pata2RestRot,
		    -35.f * S(4.0f, T + 0.08f),  LY,
		    -20.f * S(2.7f, T + 0.15f),  LX,
		    -10.f * S(5.0f, T + 0.05f),  LZ);
		Ap3(ColaBone, ColaRestRot,
		    15.f * Cos(5.f, T),           TY,
		    25.f * S(7.f, T),             TZ,
		    10.f * S(3.f, T + 0.1f),      TX);
		Ap3(CabezaBone, CabezaRestRot,
		    25.f * S(3.7f, T),            AY,
		    30.f * S(2.3f, T + 0.07f),    AZ,
		    15.f * S(5.5f, T),            AX);
		break; // LOOP ∞

	default:
		// ── KNOCKDOWN EMOTE (ID = 100) ─────────────────────────────────────
		// Tortuga boca arriba: el CUERPO completo rota 90° hacia atrás,
		// patas/brazos apuntan hacia arriba con pequeña oscilación.
		if (ActiveEmoteIndex == KNOCKDOWN_EMOTE_ID)
		{
			// CRÍTICO: si el ragdoll físico está activo, la física dicta el visual.
			// Cualquier SetRelativeRotation del KnockdownVisualComp o BoneQuat
			// override aquí es:
			//   (a) inútil — el guard IsSimulatingPhysics del ProcAnimInstance los suprime
			//   (b) peligroso — el SetRelativeRotation sobre el mesh simulado dispara
			//       el warning "Attempting to move a fully simulated skeletal mesh"
			// Early-exit: la física ragdoll es el visual de knockdown cuando bUsePhysicsRagdoll.
			if (bKnockdownRagdollActive)
			{
				break;
			}
			// DIAGNOSTIC: confirmar que entramos al case (si Kirk sigue mal aunque
			// modifique valores, puede que el emote nunca entre aquí).
			static bool bLoggedKirk = false;
			if (!bLoggedKirk)
			{
				bLoggedKirk = true;
				UE_LOG(LogTemp, Warning,
					TEXT("[Diagnostic] KNOCKDOWN case ENTRADA · IsSimulating=%s (si true, BoneQuat suppressed by ProcAnim guard)"),
					(GetMesh() && GetMesh()->IsSimulatingPhysics()) ? TEXT("Y") : TEXT("N"));
			}
			const float Setup    = FMath::Min(T / 0.2f, 1.f);  // Pose completada en 0.2s
			const float EasedSetup = FMath::InterpEaseOut(0.f, 1.f, Setup, 2.f); // Deceleración natural
			const float Struggle = FMath::Sin(T * 4.f) * 5.f;  // ±5° oscilación suave de "lucha"

			// ── Rotar el cuerpo entero (componente "Cuerpo" / KnockdownVisualComp) ──
			// KIRK-FIX: post-ANIM-01 los signos quedaron invertidos. User: "todos hacia
			// abajo en vez de arriba; en vez de -30 → +30, salvo patas que en eje vertical
			// -30 → 30". Flipeo en eje HORIZONTAL para cuerpo/brazos/cabeza/cola; patas
			// cambian a eje VERTICAL (LZ) con sus signos re-orientados.
			if (KnockdownVisualComp.IsValid())
			{
				FRotator TargetRot = MeshDefaultRelativeRotation;
				TargetRot.Pitch += 90.f;  // flipeo: -=90 → +=90 (eje horizontal)
				KnockdownVisualComp->SetRelativeRotation(
					FMath::Lerp(MeshDefaultRelativeRotation, TargetRot, EasedSetup));
			}

			// Patas: cambio de eje LY (horizontal stride) → LZ (vertical twist) + signo invertido
			Ap(Pata1Bone, Pata1RestRot,  EasedSetup * (-80.f - Struggle), LZ);
			Ap(Pata2Bone, Pata2RestRot,  EasedSetup * ( 80.f - Struggle), LZ);

			// Brazos: mismo eje AX, signo negado
			Ap(Brazo1Bone, Brazo1RestRot,  EasedSetup * (-70.f - Struggle * 0.7f), AX);
			Ap(Brazo2Bone, Brazo2RestRot,  EasedSetup * ( 70.f - Struggle * 0.7f), AX);

			// Cabeza: signo negado
			Ap(CabezaBone, CabezaRestRot, EasedSetup * -25.f, TY);

			// Cola: signo negado (expresión original -(−35 + S·0.5) = 35 − S·0.5)
			Ap(ColaBone, ColaRestRot, EasedSetup * (35.f - Struggle * 0.5f), TY);

			// NO termina (bEnded queda false) — se mantiene hasta RecoverFromKnockdown
		}
		else
		{
			bEnded = true;
		}
		break;
	}

	if (bEnded) { CancelEmote(); }
}

// ── Emote Audio ───────────────────────────────────────────────────────────────

void ATortugaCharacter::PlayEmoteSound(int32 Index)
{
	// Stop any previous emote sound first.
	StopEmoteSound();

	if (!EmoteSounds.IsValidIndex(Index) || !EmoteSounds[Index])
	{
		return;
	}

	// Lazily create the audio component on first use (attached to root, spatialized).
	if (!EmoteAudioComponent)
	{
		EmoteAudioComponent = NewObject<UAudioComponent>(this, TEXT("EmoteAudio"));
		if (!EmoteAudioComponent)
		{
			return;
		}

		EmoteAudioComponent->SetupAttachment(GetRootComponent());
		EmoteAudioComponent->bAutoActivate = false;
		EmoteAudioComponent->bAlwaysPlay = false;

		// Proximity attenuation matching voice chat range.
		EmoteAudioComponent->bAllowSpatialization = true;
		EmoteAudioComponent->bOverrideAttenuation = true;
		EmoteAudioComponent->AttenuationOverrides.bAttenuate = true;
		EmoteAudioComponent->AttenuationOverrides.bSpatialize = true;
		EmoteAudioComponent->AttenuationOverrides.FalloffDistance = FMath::Max(EmoteAudioOuterRadius - EmoteAudioInnerRadius, 100.f);
		EmoteAudioComponent->AttenuationOverrides.AttenuationShape = EAttenuationShape::Sphere;
		EmoteAudioComponent->AttenuationOverrides.AttenuationShapeExtents = FVector(EmoteAudioInnerRadius);
		EmoteAudioComponent->AttenuationOverrides.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;

		// Auto-restart when sound finishes → forced loop while emote is active.
		EmoteAudioComponent->OnAudioFinished.AddDynamic(this, &ATortugaCharacter::OnEmoteAudioFinished);

		EmoteAudioComponent->RegisterComponent();
	}

	EmoteAudioComponent->SetSound(EmoteSounds[Index]);
	EmoteAudioComponent->Play();
}

void ATortugaCharacter::StopEmoteSound()
{
	if (EmoteAudioComponent && EmoteAudioComponent->IsPlaying())
	{
		EmoteAudioComponent->Stop();
	}
}

void ATortugaCharacter::OnEmoteAudioFinished()
{
	// If an emote is still active, restart the sound for seamless looping.
	if (ActiveEmoteIndex >= 0 && EmoteAudioComponent && EmoteAudioComponent->Sound)
	{
		EmoteAudioComponent->Play();
	}
}

// ── Per-emote input handlers ──────────────────────────────────────────────────
void ATortugaCharacter::OnEmote0() { TriggerEmote(0); }
void ATortugaCharacter::OnEmote1() { TriggerEmote(1); }
void ATortugaCharacter::OnEmote2() { TriggerEmote(2); }
void ATortugaCharacter::OnEmote3() { TriggerEmote(3); }
void ATortugaCharacter::OnEmote4() { TriggerEmote(4); }
void ATortugaCharacter::OnEmote5() { TriggerEmote(5); }
void ATortugaCharacter::OnEmote6() { TriggerEmote(6); }
void ATortugaCharacter::OnEmote7() { TriggerEmote(7); }
void ATortugaCharacter::OnEmote8() { TriggerEmote(8); }
void ATortugaCharacter::OnEmote9() { TriggerEmote(9); }

// ─────────────────────────────────────────────────────────────────────────────
// DBNO / REVIVE AUDIO
// ─────────────────────────────────────────────────────────────────────────────

UAudioComponent* ATortugaCharacter::EnsureReviveAudioComponent()
{
	if (ReviveAudioComponent)
	{
		return ReviveAudioComponent;
	}

	ReviveAudioComponent = NewObject<UAudioComponent>(this, TEXT("ReviveAudio"));
	if (!ReviveAudioComponent)
	{
		return nullptr;
	}

	ReviveAudioComponent->SetupAttachment(GetRootComponent());
	ReviveAudioComponent->bAutoActivate = false;
	ReviveAudioComponent->bAlwaysPlay = false;

	// Proximity attenuation matching voice chat / emote range.
	ReviveAudioComponent->bAllowSpatialization = true;
	ReviveAudioComponent->bOverrideAttenuation = true;
	ReviveAudioComponent->AttenuationOverrides.bAttenuate = true;
	ReviveAudioComponent->AttenuationOverrides.bSpatialize = true;
	ReviveAudioComponent->AttenuationOverrides.FalloffDistance = FMath::Max(ReviveAudioOuterRadius - ReviveAudioInnerRadius, 100.f);
	ReviveAudioComponent->AttenuationOverrides.AttenuationShape = EAttenuationShape::Sphere;
	ReviveAudioComponent->AttenuationOverrides.AttenuationShapeExtents = FVector(ReviveAudioInnerRadius);
	ReviveAudioComponent->AttenuationOverrides.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;

	// Auto-restart loop while revive is channeling.
	ReviveAudioComponent->OnAudioFinished.AddDynamic(this, &ATortugaCharacter::OnReviveAudioFinished);

	ReviveAudioComponent->RegisterComponent();
	return ReviveAudioComponent;
}

UAudioComponent* ATortugaCharacter::EnsureDBNOAudioComponent()
{
	if (DBNOAudioComponent)
	{
		return DBNOAudioComponent;
	}

	DBNOAudioComponent = NewObject<UAudioComponent>(this, TEXT("DBNOAudio"));
	if (!DBNOAudioComponent)
	{
		return nullptr;
	}

	DBNOAudioComponent->SetupAttachment(GetRootComponent());
	DBNOAudioComponent->bAutoActivate = false;
	DBNOAudioComponent->bAlwaysPlay = false;

	// Non-spatialized: only the local DBNO player hears the heartbeat.
	DBNOAudioComponent->bAllowSpatialization = false;
	DBNOAudioComponent->bIsUISound = true;  // Bypass distance culling — always audible for the local player.

	// Auto-restart loop while DBNO.
	DBNOAudioComponent->OnAudioFinished.AddDynamic(this, &ATortugaCharacter::OnDBNOAudioFinished);

	DBNOAudioComponent->RegisterComponent();
	return DBNOAudioComponent;
}

void ATortugaCharacter::PlayReviveChannelSound()
{
	if (!ReviveChannelSound)
	{
		return;
	}

	UAudioComponent* AC = EnsureReviveAudioComponent();
	if (!AC)
	{
		return;
	}

	AC->SetSound(ReviveChannelSound);
	AC->Play();
}

void ATortugaCharacter::StopReviveChannelSound()
{
	if (ReviveAudioComponent && ReviveAudioComponent->IsPlaying())
	{
		ReviveAudioComponent->Stop();
	}
}

void ATortugaCharacter::PlayReviveSuccessSound()
{
	if (!ReviveSuccessSound)
	{
		return;
	}

	// Success sound: spatialized one-shot (reuse the ReviveAudioComponent).
	// If channel sound was playing, Stop first so OnFinished doesn't re-loop.
	StopReviveChannelSound();

	UAudioComponent* AC = EnsureReviveAudioComponent();
	if (!AC)
	{
		return;
	}

	AC->SetSound(ReviveSuccessSound);
	AC->Play();
	// Note: OnReviveAudioFinished will NOT re-loop because bIsReviving == false at this point.
}

void ATortugaCharacter::PlayDBNOHeartbeatSound()
{
	if (!DBNOHeartbeatSound)
	{
		return;
	}

	UAudioComponent* AC = EnsureDBNOAudioComponent();
	if (!AC)
	{
		return;
	}

	AC->SetSound(DBNOHeartbeatSound);
	AC->Play();
}

void ATortugaCharacter::StopDBNOHeartbeatSound()
{
	if (DBNOAudioComponent && DBNOAudioComponent->IsPlaying())
	{
		DBNOAudioComponent->Stop();
	}
}

void ATortugaCharacter::OnReviveAudioFinished()
{
	// Re-loop revive channel sound while actively channeling.
	if (bIsReviving && ReviveAudioComponent && ReviveAudioComponent->Sound)
	{
		ReviveAudioComponent->Play();
	}
	// If not reviving (e.g., success sound just finished), do nothing — one-shot.
}

void ATortugaCharacter::OnDBNOAudioFinished()
{
	// Re-loop heartbeat while knocked down (DBNO).
	if (bIsKnockedDown && IsLocallyControlled() && DBNOAudioComponent && DBNOAudioComponent->Sound)
	{
		DBNOAudioComponent->Play();
	}
}

// ── Big Head Consumable ───────────────────────────────────────────────────────

void ATortugaCharacter::OnRep_bBigHead()
{
	ApplyBigHeadVisual(bBigHead);
}

void ATortugaCharacter::ApplyBigHeadVisual(bool bBig)
{
	if (CabezaBone == NAME_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BigHead] CabezaBone not resolved on %s"), *GetNameSafe(this));
		return;
	}
	const float S = bBig ? BigHeadScale : 1.f;
	SetAnimBoneScale(CabezaBone, FVector(S));
}

// ── Dive System ───────────────────────────────────────────────────────────────
//
// Flow:
//   Input (client)  → TryDive()
//   → Server_StartDive(DiveDir)      — validates, applies physics, sets bIsDiving
//   → Multicast_OnDiveVisual(true)   — all clients apply tilt + capsule resize
//   TickDive (server) watches speed  → EndDive() when stopped
//   → bIsDiving = false              — OnRep_IsDiving fires on clients → restore
//
// Visual pattern mirrors knockdown: same KnockdownVisualComp, same NetworkSmoothingMode
// disable, but pitch is FORWARD (-85°) instead of backward.
// Capsule HalfHeight is reduced to simulate a horizontal hitbox.
// ─────────────────────────────────────────────────────────────────────────────

void ATortugaCharacter::TryDive()
{
	// Only locally controlled, not already diving, not knocked down
	if (!IsLocallyControlled() || bIsDiving || bIsKnockedDown || bIsDead)
	{
		return;
	}

	// DASH-03 (2026-04-26): el dash siempre va EN LA DIRECCIÓN DE LA CÁMARA.
	// Antes usaba CMC->Velocity (DASH-02) lo que ataba el dash al movimiento
	// actual del char. Ahora la cámara es la fuente de verdad — feel más
	// directo (al estilo shooter): donde miras es donde dasheas.
	FVector DiveDir = FVector::ZeroVector;
	const FRotator ControlRot = GetControlRotation();
	FVector CamForward = FRotationMatrix(FRotator(0.f, ControlRot.Yaw, 0.f)).GetUnitAxis(EAxis::X);
	CamForward.Z = 0.f;
	if (CamForward.Normalize())
	{
		DiveDir = CamForward;
	}
	else
	{
		// Fallback: control rotation degenerada → frente del actor
		DiveDir = GetActorForwardVector();
		DiveDir.Z = 0.f;
		DiveDir.Normalize();
	}

	UE_LOG(LogTemp, Log,
		TEXT("[Dive] DASH-03 · DiveDir(camera)=(%.2f,%.2f,%.2f) ControlYaw=%.1f"),
		DiveDir.X, DiveDir.Y, DiveDir.Z, ControlRot.Yaw);

	Server_StartDive(DiveDir);
}

void ATortugaCharacter::Server_StartDive_Implementation(FVector DiveDir)
{
	// Server-side guards
	if (bIsDiving || bIsKnockedDown || bIsDead)
	{
		return;
	}

	// Sanitize direction
	DiveDir.Z = 0.f;
	if (!DiveDir.Normalize())
	{
		DiveDir = GetActorForwardVector();
		DiveDir.Z = 0.f;
		DiveDir.Normalize();
	}

	// Cancel any active emote
	if (ReplicatedEmoteIndex >= 0)
	{
		ReplicatedEmoteIndex = -1;
		CancelEmoteLocalOnly();
	}

	// DASH-05: rotación fluida hacia DiveDir. En lugar de snap instantáneo,
	// se setea DiveTargetYaw + flag bDiveYawInterpActive que TickDive (corre en
	// owner + server) consume cada frame para interpolar el actor hacia el
	// target con velocidad DiveYawInterpSpeed (deg/seg).
	//
	// Replicado: clientes remotos también interpolan tras recibir el target —
	// feel suave en todos los puntos de vista.
	DiveTargetYaw         = DiveDir.Rotation().Yaw;
	bDiveYawInterpActive  = true;

	// ── Momentum preservation: cámara ACTUAL vs salto ORIGINAL ─────────────────
	// La velocity horizontal AL SALTAR (capturada en OnJumped) marca la dirección
	// del salto. Comparamos la cámara ACTUAL (donde apunta el jugador al dashear)
	// contra esa dirección original.
	//
	//   alignment = +1 → cámara apunta donde estaba yendo al saltar  → Forward factor (bonus máx)
	//   alignment =  0 → cámara apunta lateral al salto              → Lateral factor (bonus medio)
	//   alignment = -1 → cámara apunta opuesta al salto              → Backward factor (0 default)
	//
	// Si rotaste la cámara para mirar atrás del salto, el momentum se anula —
	// el jugador "decide" no preservar el momentum cambiando hacia donde mira.
	float MomentumBonus = 0.f;
	const float JumpStartSpeed = JumpStartHorizontalVelocity.Size();
	if (JumpStartSpeed > 1.f)
	{
		const FVector JumpStartDir = JumpStartHorizontalVelocity / JumpStartSpeed;

		// Forward cámara (control rotation Yaw, plano horizontal)
		const FRotator ControlRot = GetControlRotation();
		FVector CamForward = FRotationMatrix(FRotator(0.f, ControlRot.Yaw, 0.f)).GetUnitAxis(EAxis::X);
		CamForward.Z = 0.f;
		if (!CamForward.Normalize())
		{
			CamForward = GetActorForwardVector();
			CamForward.Z = 0.f;
			CamForward.Normalize();
		}

		const float Alignment = FVector::DotProduct(CamForward, JumpStartDir); // [-1, 1]

		float Factor;
		if (Alignment >= 0.f)
		{
			Factor = FMath::Lerp(DiveMomentumLateralFactor, DiveMomentumForwardFactor, Alignment);
		}
		else
		{
			Factor = FMath::Lerp(DiveMomentumLateralFactor, DiveMomentumBackwardFactor, -Alignment);
		}
		MomentumBonus = JumpStartSpeed * Factor;

		UE_LOG(LogTemp, Log,
			TEXT("[Dive] Momentum · JumpStartSpeed=%.0f CamVsJumpAlign=%.2f Factor=%.2f Bonus=%.0f → Total=%.0f"),
			JumpStartSpeed, Alignment, Factor, MomentumBonus, DiveForwardSpeed + MomentumBonus);
	}
	else
	{
		UE_LOG(LogTemp, Log,
			TEXT("[Dive] Momentum · sin salto registrado (JumpStartSpeed=0) → bonus 0, dash a velocidad base %.0f"),
			DiveForwardSpeed);
	}

	// Cap simétrico: permite valores negativos (dash hacia atrás cuando la cámara
	// está opuesta al salto y BackwardFactor es lo bastante negativo). El char
	// invierte la DiveDir naturalmente porque DiveDir * (-Speed) = -DiveDir * Speed.
	const float CombinedForwardSpeed = FMath::Clamp(DiveForwardSpeed + MomentumBonus,
		-DiveMaxTotalSpeed, DiveMaxTotalSpeed);
	const FVector DiveVelocity = DiveDir * CombinedForwardSpeed + FVector(0.f, 0.f, -DiveDownwardSpeed);
	LaunchCharacter(DiveVelocity, /*bXYOverride=*/true, /*bZOverride=*/true);

	// Activate dive state — triggers OnRep on clients
	bIsDiving     = true;
	DiveLockTimer = 0.f;

	// Apply visual on all machines (server runs Multicast locally too)
	Multicast_OnDiveVisual(true);

	UE_LOG(LogTemp, Log, TEXT("[Dive] %s — dive started, dir=%s"), *GetNameSafe(this), *DiveDir.ToString());
}

void ATortugaCharacter::Multicast_OnDiveVisual_Implementation(bool bEnter)
{
	UCharacterMovementComponent* CMC = GetCharacterMovement();

	if (bEnter)
	{
		// Disable CMC smoothing so it won't fight our manual rotation
		if (CMC)
		{
			CMC->NetworkSmoothingMode = ENetworkSmoothingMode::Disabled;
		}

		// Shrink capsule to represent horizontal hitbox
		GetCapsuleComponent()->SetCapsuleHalfHeight(DiveCapsuleHalfHeight);

		// Abort any active emote or blend-out — dive takes full control of limbs.
		// Snap all limb components to rest immediately so the dive pose starts clean.
		if (ActiveEmoteIndex >= 0 || bEmoteBlendingOut)
		{
			SetAnimBoneRot(Brazo1Bone, Brazo1RestRot);
			SetAnimBoneRot(Brazo2Bone, Brazo2RestRot);
			SetAnimBoneRot(Pata1Bone,  Pata1RestRot);
			SetAnimBoneRot(Pata2Bone,  Pata2RestRot);
			SetAnimBoneRot(ColaBone,   ColaRestRot);
			SetAnimBoneRot(CabezaBone, CabezaRestRot);
			bEmoteBlendingOut           = false;
			bKnockdownCompSnapshotValid = false;
			EmoteBlendOutTimer          = 0.f;
			ActiveEmoteIndex            = -1;
			EmoteTime                   = 0.f;
		}
		// Abort jump animation if it was running
		bJumpAnimActive = false;
		JumpAnimTime    = 0.f;

		// DiveTiltAlpha will be driven smoothly by TickDive (starts lerping to 1)
	}
	else
	{
		// Restore capsule size
		GetCapsuleComponent()->SetCapsuleHalfHeight(DiveCapsuleOrigHalfHeight);

		// Restore CMC smoothing
		if (CMC)
		{
			CMC->NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;
		}

		// TickDive lerps DiveTiltAlpha back to 0 and restores rotations at that point.
		// No immediate snap needed here — the lerp handles the smooth return.
	}
}

void ATortugaCharacter::OnRep_IsDiving()
{
	// Called on REMOTE clients when bIsDiving changes.
	// The server already ran Multicast_OnDiveVisual, but OnRep covers clients
	// that joined late or missed the multicast.
	Multicast_OnDiveVisual_Implementation(bIsDiving);
}

void ATortugaCharacter::EndDive()
{
	if (!HasAuthority()) { return; }

	bIsDiving             = false;
	bDiveYawInterpActive  = false;
	DiveLockTimer         = 0.f;

	// Multicast restore visual
	Multicast_OnDiveVisual(false);

	UE_LOG(LogTemp, Log, TEXT("[Dive] %s — dive ended."), *GetNameSafe(this));
}

void ATortugaCharacter::TickDive(float DeltaTime)
{
	// ── Guard: si el mesh ya está en ragdoll (knockdown post-dash a plátano,
	//    o muerte durante dive), NO tocar SetRelativeRotation. Hacerlo dispara
	//    "Attempting to move a fully simulated skeletal mesh" y desincroniza
	//    los bodies del ragdoll. La interpolación visual del dive deja de
	//    importar — el ragdoll es el visual canonico. ──────────────────────
	{
		USkeletalMeshComponent* SkelMeshGuard = GetMesh();
		if (SkelMeshGuard && SkelMeshGuard->IsSimulatingPhysics())
		{
			DiveTiltAlpha = 0.f;
			bDiveYawInterpActive = false;
			return;
		}
		if (bIsKnockedDown || bIsDead)
		{
			// Cancelar interpolación pendiente sin tocar el mesh — ya lo
			// gestiona ApplyKnockdownVisual / SetDeadVisual.
			DiveTiltAlpha = 0.f;
			bDiveYawInterpActive = false;
			return;
		}
	}

	// ── DASH-05: interpolación fluida del actor Yaw hacia DiveTargetYaw ────────
	// Owner + server interpolan localmente para feel inmediato. Clientes remotos
	// también: bDiveYawInterpActive y DiveTargetYaw replican, así que el cliente
	// remoto ejecuta el mismo path con valores autoritativos.
	if (bDiveYawInterpActive)
	{
		const FRotator CurrentRot = GetActorRotation();
		const float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentRot.Yaw, DiveTargetYaw);
		const float MaxStep  = DiveYawInterpSpeed * DeltaTime;
		const float StepYaw  = FMath::Clamp(DeltaYaw, -MaxStep, MaxStep);
		const float NewYaw   = CurrentRot.Yaw + StepYaw;

		// Solo aplicamos en owner (cliente local) y server. Clientes remotos no-owner
		// reciben la rotation por bReplicateMovement con smoothing — si llamamos
		// SetActorRotation en ellos, peleamos contra la replicación.
		if (IsLocallyControlled() || HasAuthority())
		{
			SetActorRotation(FRotator(0.f, NewYaw, 0.f));
		}

		if (FMath::Abs(DeltaYaw) <= 1.f)
		{
			bDiveYawInterpActive = false; // alcanzado el target → desactivar
		}
	}

	// ── Whole-character forward tilt (all machines, cosmetic) ─────────────────
	// Rotate GetMesh() (carries all re-attached limbs) AND KnockdownVisualComp
	// (Cuerpo) by the same pitch so the entire character tilts as one solid unit.
	// If Cuerpo is a child of GetMesh() the runtime ancestor check skips the
	// separate rotation to prevent double-rotation.
	if (bIsDiving || DiveTiltAlpha > 0.f)
	{
		// DASH-01 v3 DIAGNOSTIC: imprime DiveMeshDefaultRot la primera vez para ver
		// qué Yaw/Pitch/Roll tiene realmente el mesh post-ANIM-01 (hipótesis: si
		// DiveMeshDefaultRot tiene un eje distinto al asumido, el quaternion fix
		// del round 2 rota por el eje incorrecto).
		static bool bLoggedDiveRot = false;
		if (!bLoggedDiveRot && bIsDiving)
		{
			bLoggedDiveRot = true;
			UE_LOG(LogTemp, Warning,
				TEXT("[Diagnostic] DiveMeshDefaultRot = Rotator(P=%.2f Y=%.2f R=%.2f) · MeshRel = Rotator(P=%.2f Y=%.2f R=%.2f)"),
				DiveMeshDefaultRot.Pitch, DiveMeshDefaultRot.Yaw, DiveMeshDefaultRot.Roll,
				GetMesh() ? GetMesh()->GetRelativeRotation().Pitch : 0.f,
				GetMesh() ? GetMesh()->GetRelativeRotation().Yaw   : 0.f,
				GetMesh() ? GetMesh()->GetRelativeRotation().Roll  : 0.f);
		}
		const float TargetAlpha = bIsDiving ? 1.f : 0.f;
		DiveTiltAlpha = FMath::FInterpTo(DiveTiltAlpha, TargetAlpha, DeltaTime, DiveTiltSpeed);
		if (FMath::Abs(DiveTiltAlpha - TargetAlpha) < 0.005f)
		{
			DiveTiltAlpha = TargetAlpha;
		}

		constexpr float DivePitch = -80.f;
		const float     env       = DiveTiltAlpha;

		// 1) SkeletalMesh — all re-attached limbs follow.
		// DASH-01 v2: DiveMeshDefaultRot post-ANIM-01 incluye Yaw=90 (SKM unificado).
		// Sumar al componente Pitch del rotator producía tilt lateral (orden ZYX:
		// Yaw→Pitch→Roll hacía que el Pitch rote alrededor del eje ya rotado por Yaw).
		// Composición con quaterniones: aplicar BaseQuat primero (orientación del mesh)
		// y después un TiltQuat en espacio PADRE (capsule), cuyo eje Y = actor-right →
		// Pitch puro del actor → tilt hacia adelante real.
		if (USkeletalMeshComponent* SkelMesh = GetMesh())
		{
			const FQuat BaseQuat = DiveMeshDefaultRot.Quaternion();
			// Axis configurable en BP para ajustar sin recompilar: user puede probar
			// (1,0,0) vs (0,1,0) vs (-1,0,0) etc según la orientación default del
			// mesh post-ANIM-01. Default (1,0,0) asume Yaw=90 en rest.
			const FVector TiltAxisNorm = DiveTiltAxis.IsNearlyZero() ? FVector(1.f, 0.f, 0.f) : DiveTiltAxis.GetSafeNormal();
			const FQuat TiltQuat(TiltAxisNorm, FMath::DegreesToRadians(DivePitch * env));
			SkelMesh->SetRelativeRotation(TiltQuat * BaseQuat);
		}

		// 2) KnockdownVisualComp (Cuerpo) — only if NOT already a descendant of GetMesh()
		//    AND not GetMesh() itself. Sin este segundo check, cuando
		//    KnockdownVisualComp == GetMesh() (fallback del BeginPlay), este bloque
		//    SOBREESCRIBE la rotación quaternion-correcta del bloque 1 con una suma
		//    de FRotator (incorrecta) → dash rotaba sobre eje equivocado.
		if (KnockdownVisualComp.IsValid() && KnockdownVisualComp.Get() != GetMesh())
		{
			bool bIsChildOfMesh = false;
			for (USceneComponent* Cur = KnockdownVisualComp->GetAttachParent(); Cur; Cur = Cur->GetAttachParent())
			{
				if (Cur == GetMesh()) { bIsChildOfMesh = true; break; }
			}
			if (!bIsChildOfMesh)
			{
				FRotator Rot = MeshDefaultRelativeRotation;
				Rot.Pitch   += DivePitch * env;
				KnockdownVisualComp->SetRelativeRotation(Rot);
			}
		}

		// Snap everything back to exact rest once fade-out completes
		if (!bIsDiving && DiveTiltAlpha == 0.f)
		{
			GetCapsuleComponent()->SetCapsuleHalfHeight(DiveCapsuleOrigHalfHeight);
			if (USkeletalMeshComponent* SkelMesh = GetMesh())
			{
				SkelMesh->SetRelativeRotation(DiveMeshDefaultRot);
			}
			if (KnockdownVisualComp.IsValid())
			{
				KnockdownVisualComp->SetRelativeRotation(MeshDefaultRelativeRotation);
			}
		}
	}

	// ── Recovery check (server only) ─────────────────────────────────────────
	if (!HasAuthority() || !bIsDiving) { return; }

	DiveLockTimer += DeltaTime;

	// Wait for minimum lock duration first, then check if character has stopped
	if (DiveLockTimer < DiveMinLockDuration) { return; }

	const float Speed2D = GetVelocity().Size2D();
	if (Speed2D <= DiveStopSpeedThreshold)
	{
		EndDive();
	}
}

// ── Jump Procedural Animation ─────────────────────────────────────────────────
//
// Triggered locally when the character jumps from the ground.
// Cosmetic-only; runs only on the local machine (no replication needed).
// Arms shoot up, legs kick back, head tilts back, tail rises.
// Duration: ~0.8s total (fast rise 0.12s → hold → blend back 0.35s).
// ─────────────────────────────────────────────────────────────────────────────

void ATortugaCharacter::TickJumpAnim(float DeltaTime)
{
	if (!bJumpAnimActive) { return; }

	// Cancelled by dive or incapacitation — snap back to rest
	if (bIsDiving || bIsKnockedDown || bIsDead)
	{
		SetAnimBoneRot(Brazo1Bone, Brazo1RestRot);
		SetAnimBoneRot(Brazo2Bone, Brazo2RestRot);
		SetAnimBoneRot(Pata1Bone,  Pata1RestRot);
		SetAnimBoneRot(Pata2Bone,  Pata2RestRot);
		SetAnimBoneRot(ColaBone,   ColaRestRot);
		SetAnimBoneRot(CabezaBone, CabezaRestRot);
		bJumpAnimActive = false;
		JumpAnimTime    = 0.f;
		return;
	}

	JumpAnimTime += DeltaTime;
	const float T = JumpAnimTime;

	const auto Sat = [](float v) { return FMath::Clamp(v, 0.f, 1.f); };

	const FVector AX = ArmSwingAxis;               // arm up/down (R_z(-90°) corrected via UPROPERTY)
	const FVector AY = FVector(1.f, 0.f, 0.f);    // arm roll / head nod (R_z(-90°) corrected)
	const FVector AZ = FVector(0.f, 0.f, 1.f);    // arm forward/back
	const FVector LY = LegSwingAxis;              // (0,1,0)  leg swing
	const FVector TY = TailUpDownAxis;

	auto Ap  = [this](FName Bone, const FRotator& Rest, float Angle, const FVector& Axis)
	{
		ApplyEmoteAngle(Bone, Rest, Angle, Axis);
	};
	auto Ap2 = [this](FName Bone, const FRotator& Rest,
	                  float A1, const FVector& Ax1, float A2, const FVector& Ax2)
	{
		ApplyEmoteAngles2(Bone, Rest, A1, Ax1, A2, Ax2);
	};

	// Envelope: fast rise (0.12s), hold, then blend back to rest (0.35s from t=0.45)
	const float rise    = Sat(T / 0.12f);
	const float fadeOut = T > 0.45f ? Sat((T - 0.45f) / 0.35f) : 0.f;
	const float env     = FMath::InterpEaseOut(0.f, 1.f, rise, 2.f) * (1.f - fadeOut);

	// Both arms shoot up wide — "weeee!" jump pose
	Ap2(Brazo1Bone, Brazo1RestRot,  env *  90.f,  AX,  env * (-25.f), AZ);
	Ap2(Brazo2Bone, Brazo2RestRot, -env *  90.f,  AX,  env *   25.f,  AZ);

	// Legs kick back (both same direction — frog jump)
	Ap(Pata1Bone, Pata1RestRot, env * 70.f, LY);
	Ap(Pata2Bone, Pata2RestRot, env * 70.f, LY);

	// Head tilts back slightly (looking up with excitement)
	Ap(CabezaBone, CabezaRestRot, env * (-18.f), AY);

	// Tail rises
	Ap(ColaBone, ColaRestRot, env * 22.f, TY);

	if (T >= 0.8f)
	{
		// Snap exactly to rest at end (env ≈ 0 already via fadeOut, but be exact)
		SetAnimBoneRot(Brazo1Bone, Brazo1RestRot);
		SetAnimBoneRot(Brazo2Bone, Brazo2RestRot);
		SetAnimBoneRot(Pata1Bone,  Pata1RestRot);
		SetAnimBoneRot(Pata2Bone,  Pata2RestRot);
		SetAnimBoneRot(ColaBone,   ColaRestRot);
		SetAnimBoneRot(CabezaBone, CabezaRestRot);
		bJumpAnimActive = false;
		JumpAnimTime    = 0.f;
	}
}

// ── Head Look ─────────────────────────────────────────────────────────────────

void ATortugaCharacter::TickHeadLook(float DeltaTime)
{
	// No animar la cabeza durante emotes, blend-out, knockdown, muerte, dive o jump anim.
	// Esos sistemas son dueños de Cabeza durante su ciclo de vida.
	if (ActiveEmoteIndex >= 0 || bEmoteBlendingOut) { return; }
	if (bIsKnockedDown || bIsDead)                  { return; }
	if (bIsDiving || DiveTiltAlpha > 0.f)           { return; }
	if (bJumpAnimActive)                             { return; }
	if (CabezaBone == NAME_None)                     { return; }

	if (IsLocallyControlled())
	{
		// Calcular offset de la cámara respecto al cuerpo
		const APlayerController* PC = Cast<APlayerController>(GetController());
		if (!PC) { return; }

		const FRotator ControlRot = PC->GetControlRotation();
		const float ActorYaw      = GetActorRotation().Yaw;

		// Yaw relativo del control respecto al cuerpo, siempre normalizado a [-180, 180].
		// NOTA: antes manteníamos un acumulador (LastHeadRawYaw) para "continuidad",
		// pero si el usuario giraba la cámara continuamente en la misma dirección, el
		// acumulador crecía sin tope y la cabeza quedaba encallada en ±90°: había que
		// rotar el mismo número de vueltas en sentido opuesto para desbloquearla.
		// NormalizeAxis + Clamp es suficiente: la cabeza simplemente se queda en el
		// tope visual cuando la cámara está >90° detrás del cuerpo. Sin latch.
		const float RawYaw   = FRotator::NormalizeAxis(ControlRot.Yaw - ActorYaw);
		LocalHeadRelativeYaw = FMath::Clamp(RawYaw, -90.f, 90.f);

		// Pitch: en UE el pitch es negativo al mirar arriba; lo invertimos para nuestra convención
		const float RawPitch = FRotator::NormalizeAxis(ControlRot.Pitch);
		LocalHeadPitch       = FMath::Clamp(-RawPitch, -80.f, 80.f);

		// Interpolación local: suaviza snaps cuando el cuerpo rota hacia la cámara.
		// Velocidad alta (20/s) → no se nota para input directo de cámara,
		// pero evita el snap de 90° cuando el character body rota.
		SmoothedHeadYaw   = FMath::FInterpTo(SmoothedHeadYaw,   LocalHeadRelativeYaw, DeltaTime, 20.f);
		SmoothedHeadPitch = FMath::FInterpTo(SmoothedHeadPitch, LocalHeadPitch,        DeltaTime, 20.f);
		ApplyHeadLookToCabeza(SmoothedHeadYaw, SmoothedHeadPitch);

		// Enviar al servidor (listen-server escribe directo; cliente dedicado usa RPC unreliable)
		if (HasAuthority())
		{
			ReplicatedHeadYaw   = LocalHeadRelativeYaw;
			ReplicatedHeadPitch = LocalHeadPitch;
		}
		else
		{
			ServerUpdateHeadRotation(LocalHeadRelativeYaw, LocalHeadPitch);
		}
	}
	else
	{
		// Cliente remoto: interpolar hacia los valores replicados para suavidad
		SmoothedHeadYaw   = FMath::FInterpTo(SmoothedHeadYaw,   ReplicatedHeadYaw,   DeltaTime, 15.f);
		SmoothedHeadPitch = FMath::FInterpTo(SmoothedHeadPitch, ReplicatedHeadPitch, DeltaTime, 15.f);
		ApplyHeadLookToCabeza(SmoothedHeadYaw, SmoothedHeadPitch);
	}
}

void ATortugaCharacter::ApplyHeadLookToCabeza(float Yaw, float Pitch)
{
	if (CabezaBone == NAME_None) { return; }

	const FQuat RestQ (FVector(0.f, 0.f, 1.f), FMath::DegreesToRadians(HeadRestYawDeg));
	const FQuat YawQ  (FVector(0.f, 0.f, 1.f), FMath::DegreesToRadians(Yaw));
	const FQuat PitchQ(FVector(-1.f, 0.f, 0.f), FMath::DegreesToRadians(Pitch));
	SetAnimBoneRot(CabezaBone, (RestQ * YawQ * PitchQ * FQuat(CabezaRestRot)).Rotator());

	// Head-neck follow workaround: rotar el hueso del cuello/tronco por una fracción
	// del yaw/pitch para arrastrar la piel del cuello (que está pesada a ese hueso).
	// Configurar NeckFollowBone + NeckFollowRatio en BP si la cara del cuello se queda fija.
	if (NeckFollowBone != NAME_None && NeckFollowRatio > 0.f)
	{
		const FQuat NeckYawQ  (FVector(0.f, 0.f, 1.f),  FMath::DegreesToRadians(Yaw   * NeckFollowRatio));
		const FQuat NeckPitchQ(FVector(-1.f, 0.f, 0.f), FMath::DegreesToRadians(Pitch * NeckFollowRatio));
		SetAnimBoneRot(NeckFollowBone, (NeckYawQ * NeckPitchQ * FQuat(NeckFollowRestRot)).Rotator());
	}
}

void ATortugaCharacter::ServerUpdateHeadRotation_Implementation(float Yaw, float Pitch)
{
	// Validar rangos en el servidor para prevenir manipulación del cliente
	ReplicatedHeadYaw   = FMath::Clamp(Yaw,   -90.f,  90.f);
	ReplicatedHeadPitch = FMath::Clamp(Pitch,  -80.f,  80.f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tótem auto-revive — feedback visual/sonoro en todas las máquinas
// ─────────────────────────────────────────────────────────────────────────────

void ATortugaCharacter::Multicast_OnTotemAutoRevive_Implementation()
{
	if (TotemSelfReviveSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, TotemSelfReviveSound, GetActorLocation());
	}
	if (TotemSelfReviveVFX)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), TotemSelfReviveVFX, GetActorLocation());
	}
	OnTotemAutoRevive();
}
