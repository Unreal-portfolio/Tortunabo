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

// ── SFX helpers ──────────────────────────────────────────────────────────────

void ATortugaCharacter::PlaySfxAtSelf(USoundBase* Sound) const
{
	if (!Sound || !GetWorld()) { return; }
	UGameplayStatics::SpawnSoundAtLocation(this, Sound, GetActorLocation());
}

void ATortugaCharacter::MulticastPlaySfx_Implementation(USoundBase* Sound)
{
	PlaySfxAtSelf(Sound);
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

	// ── Footsteps (cosmetic, local-only en cada máquina) ─────────────────────
	// Cada máquina tickea TickLegAnimation para todos los pawns; Velocity está
	// replicada. No hace falta RPC: cada cliente decide si el pawn X debe
	// sonar paso, y todos convergen al mismo ritmo (±1 frame).
	if (FootstepSound && !bIsDead)
	{
		const UCharacterMovementComponent* CMC = GetCharacterMovement();
		const bool bGrounded = CMC && !CMC->IsFalling();
		if (bGrounded && Speed > FootstepMinGroundSpeed)
		{
			FootstepCooldown -= DeltaTime;
			if (FootstepCooldown <= 0.f)
			{
				PlaySfxAtSelf(FootstepSound);
				FootstepCooldown = bIsSprinting ? FootstepSprintInterval : FootstepWalkInterval;
			}
		}
		else
		{
			FootstepCooldown = 0.f;
		}
	}
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

	if (HasAuthority() && JumpSound)
	{
		MulticastPlaySfx(JumpSound);
	}
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

			if (ThrowSound) { MulticastPlaySfx(ThrowSound); }
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
	if (!InventoryComponent || !InventoryComponent->HasEquippedItem()) { return; }

	// Validar ANTES de consumir. La versión anterior extraía el ítem del inventario
	// primero y solo después comprobaba PickupActorClass / el spawn: si la clase era
	// null o SpawnActor fallaba, el ítem quedaba consumido pero sin pickup en el mundo
	// (item lost). Ahora spawnamos primero y solo consumimos si el pickup existe.
	const FTN_InventoryItem& Equipped = InventoryComponent->GetEquippedItem();
	if (!Equipped.IsValid() || !Equipped.PickupActorClass)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// Siempre spawnear en el suelo aunque el personaje esté en el aire
	const FVector DropPoint = FindGroundBelow(GetItemSpawnLocation());

	ATN_PickupInteractableBase* PickupActor = GetWorld()->SpawnActor<ATN_PickupInteractableBase>(
		Equipped.PickupActorClass, DropPoint, FRotator::ZeroRotator, SpawnParams);
	if (!PickupActor)
	{
		return; // Spawn falló → NO consumir el ítem (evita pérdida)
	}

	FTN_InventoryItem DroppedItem;
	if (!InventoryComponent->TryExtractEquippedItem(DroppedItem))
	{
		PickupActor->Destroy();
		return;
	}

	PickupActor->InitializeFromInventoryItem(DroppedItem);
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
