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
#include "Player/TN_InventoryComponent.h"
#include "Player/TN_StaminaComponent.h"
#include "World/TN_InteractableBase.h"
#include "World/TN_PickupInteractableBase.h"
#include "World/TN_ThrowableItemActor.h"
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

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 350.f;
	CameraBoom->bUsePawnControlRotation = true;

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

	// Cache leg components (added in Blueprint as child SceneComponents).
	// The cube mesh origin must be at the TOP of each cube (the hip pivot).
	// If GetFName() doesn't match, check the component name in the BP Details panel.
	Pata1 = FindChildByName(TEXT("Pata1"));
	Pata2 = FindChildByName(TEXT("Pata2"));

	if (Pata1.IsValid()) { Pata1RestRot = Pata1->GetRelativeRotation(); Pata1RestLoc = Pata1->GetRelativeLocation(); }
	else { UE_LOG(LogTemp, Warning, TEXT("[TortugaCharacter] 'Pata1' component not found — add a SceneComponent named exactly 'Pata1' in the Blueprint.")); }

	if (Pata2.IsValid()) { Pata2RestRot = Pata2->GetRelativeRotation(); Pata2RestLoc = Pata2->GetRelativeLocation(); }
	else { UE_LOG(LogTemp, Warning, TEXT("[TortugaCharacter] 'Pata2' component not found — add a SceneComponent named exactly 'Pata2' in the Blueprint.")); }

	// Cache arm, tail and head components for the emote system (same pattern as legs).
	Brazo1 = FindChildByName(TEXT("Brazo1"));
	Brazo2 = FindChildByName(TEXT("Brazo2"));
	Cola   = FindChildByName(TEXT("Cola"));
	Cabeza = FindChildByName(TEXT("Cabeza"));

	if (Brazo1.IsValid()) { Brazo1RestRot = Brazo1->GetRelativeRotation(); Brazo1RestLoc = Brazo1->GetRelativeLocation(); }
	else { UE_LOG(LogTemp, Warning, TEXT("[TortugaCharacter] 'Brazo1' not found — emotes will be partial. Add a SceneComponent named exactly 'Brazo1' in the Blueprint.")); }

	if (Brazo2.IsValid()) { Brazo2RestRot = Brazo2->GetRelativeRotation(); Brazo2RestLoc = Brazo2->GetRelativeLocation(); }
	else { UE_LOG(LogTemp, Warning, TEXT("[TortugaCharacter] 'Brazo2' not found — emotes will be partial.")); }

	if (Cola.IsValid()) { ColaRestRot = Cola->GetRelativeRotation(); ColaRestLoc = Cola->GetRelativeLocation(); }
	else { UE_LOG(LogTemp, Warning, TEXT("[TortugaCharacter] 'Cola' not found — emotes will be partial.")); }

	// Disable pawn collision on all child meshes of Cola so the tail doesn't push other players.
	if (Cola.IsValid())
	{
		TArray<USceneComponent*> ColaChildren;
		Cola->GetChildrenComponents(true, ColaChildren);
		for (USceneComponent* Child : ColaChildren)
		{
			if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Child))
			{
				Prim->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
			}
		}
	}

	if (Cabeza.IsValid()) { CabezaRestRot = Cabeza->GetRelativeRotation(); CabezaRestLoc = Cabeza->GetRelativeLocation(); CabezaRestScale = Cabeza->GetRelativeScale3D(); }
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[TortugaCharacter] ═══════════════════════════════════════════════════════════"));
		UE_LOG(LogTemp, Error, TEXT("[TortugaCharacter] 'Cabeza' NOT FOUND — head will NOT animate during emotes!"));
		UE_LOG(LogTemp, Error, TEXT("[TortugaCharacter] Add a SceneComponent named EXACTLY 'Cabeza' in BP_TortugaCharacter."));
		UE_LOG(LogTemp, Error, TEXT("[TortugaCharacter] Available SceneComponents on '%s':"), *GetName());

		int32 SceneCompCount = 0;
		for (UActorComponent* Comp : GetComponents())
		{
			if (USceneComponent* SC = Cast<USceneComponent>(Comp))
			{
				++SceneCompCount;
				UE_LOG(LogTemp, Error, TEXT("[TortugaCharacter]   [%d] '%s'  (Class: %s)"),
					SceneCompCount, *SC->GetName(), *SC->GetClass()->GetName());
			}
		}
		if (SceneCompCount == 0)
		{
			UE_LOG(LogTemp, Error, TEXT("[TortugaCharacter]   (none found — BP components may not be set up)"));
		}
		UE_LOG(LogTemp, Error, TEXT("[TortugaCharacter] ═══════════════════════════════════════════════════════════"));
	}

	// Log de diagnóstico: estado final de todos los componentes de emote
	UE_LOG(LogTemp, Log, TEXT("[TortugaCharacter] Emote components: Brazo1=%s  Brazo2=%s  Pata1=%s  Pata2=%s  Cola=%s  Cabeza=%s"),
		Brazo1.IsValid() ? TEXT("OK") : TEXT("MISSING"),
		Brazo2.IsValid() ? TEXT("OK") : TEXT("MISSING"),
		Pata1.IsValid()  ? TEXT("OK") : TEXT("MISSING"),
		Pata2.IsValid()  ? TEXT("OK") : TEXT("MISSING"),
		Cola.IsValid()   ? TEXT("OK") : TEXT("MISSING"),
		Cabeza.IsValid() ? TEXT("OK") : TEXT("MISSING"));

	// ── Fix de network smoothing para skins en clientes remotos ──────────────
	// El CMC sólo aplica smoothing al SkeletalMesh (GetMesh()) y sus hijos.
	// Si los componentes de skin están en CapsuleComponent (root), se mueven con
	// la posición raw (no suavizada) → lag visual en clientes remotos.
	// Solución: si están en el root o en el capsule, re-adjuntarlos a GetMesh().
	// Se preserva el transform relativo para no cambiar posiciones configuradas en BP.
	{
		USkeletalMeshComponent* MainMesh = GetMesh();
		USceneComponent* RootComp = GetRootComponent();

		// Lista de componentes cosméticos que deben ser hijos de GetMesh()
		TWeakObjectPtr<USceneComponent> CosmeticComps[] = {
			Pata1, Pata2, Brazo1, Brazo2, Cola, Cabeza
		};

		for (TWeakObjectPtr<USceneComponent>& WeakComp : CosmeticComps)
		{
			if (!WeakComp.IsValid() || !MainMesh)
			{
				continue;
			}

			USceneComponent* Comp = WeakComp.Get();
			USceneComponent* AttachParent = Comp->GetAttachParent();

			// Si ya está en GetMesh() o en un descendiente suyo, no hacer nada.
			// Sólo reatamos si está en el root/capsule directamente.
			if (AttachParent == RootComp || AttachParent == MainMesh->GetAttachParent())
			{
				// Guardar el transform relativo antes de re-adjuntar
				const FTransform SavedRelTransform = Comp->GetRelativeTransform();

				Comp->AttachToComponent(MainMesh,
					FAttachmentTransformRules::KeepRelativeTransform);
				Comp->SetRelativeTransform(SavedRelTransform);

				UE_LOG(LogTemp, Log, TEXT("[TortugaCharacter] '%s' re-adjuntado a GetMesh() "
					"para network smoothing (era hijo de '%s')."),
					*Comp->GetName(), AttachParent ? *AttachParent->GetName() : TEXT("null"));
			}
		}
	}

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
	// HelmetMeshComp ya está adjunto a GetMesh() desde el constructor → hereda el
	// network smoothing del CMC automáticamente (sin lag visual en clientes remotos).
	// Si existe el SceneComponent "Sombrero" (p.ej. hijo de Cabeza en el BP),
	// re-adjuntar a él para respetar el offset configurado en el Blueprint.
	SombreroSocket = FindChildByName(TEXT("Sombrero"));
	if (SombreroSocket.IsValid() && HelmetMeshComp)
	{
		HelmetMeshComp->AttachToComponent(SombreroSocket.Get(),
			FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		UE_LOG(LogTemp, Log, TEXT("[TortugaCharacter] Socket 'Sombrero' encontrado → HelmetMeshComp adjunto."));
	}
	else
	{
		// Permanece adjunto a GetMesh() desde el constructor — sin lag, sin warning.
		UE_LOG(LogTemp, Log, TEXT("[TortugaCharacter] Socket 'Sombrero' no encontrado → HelmetMeshComp en GetMesh()."));
	}

	// Cachear materiales originales de todos los StaticMeshComponents del cuerpo.
	// Deben guardarse ANTES del timer para que UpdateSkinVisual(NAME_None) pueda restaurarlos.
	DefaultBodyMaterials.Reset();
	for (UActorComponent* Comp : GetComponents())
	{
		UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Comp);
		if (!SMC || SMC == HelmetMeshComp) { continue; }

		TArray<TObjectPtr<UMaterialInterface>>& Mats = DefaultBodyMaterials.Add(SMC);
		for (int32 i = 0; i < SMC->GetNumMaterials(); ++i)
		{
			Mats.Add(SMC->GetMaterial(i));
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

	// ── Knockdown ground lock ─────────────────────────────────────────────
	// Si el personaje fue lanzado por una bomba (PufferFish) durante knockdown,
	// HandlePendingLaunch pone MOVE_Falling para que vuele por los aires.
	// Cuando aterriza, CMC pone MOVE_Walking. Re-desactivamos movimiento aquí
	// para que siga inmovilizado mientras dure el knockdown.
	if (HasAuthority() && bIsKnockedDown)
	{
		if (UCharacterMovementComponent* MC = GetCharacterMovement())
		{
			if (MC->IsMovingOnGround())
			{
				MC->DisableMovement();
			}
		}
	}

	TickEmote(DeltaTime);          // emote system (overrides leg anim when active)
	TickLegAnimation(DeltaTime);   // normal locomotion (suppressed during emotes)
	TickCameraInterp(DeltaTime);   // cinematic camera zoom/FOV interpolation
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

	// Bail out early if neither leg is available.
	if (!Pata1.IsValid() && !Pata2.IsValid())
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
	if (Pata1.IsValid()) { ApplyLegAngle(Pata1.Get(), Pata1RestRot,  Angle); }
	if (Pata2.IsValid()) { ApplyLegAngle(Pata2.Get(), Pata2RestRot, -Angle); }
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
}

void ATortugaCharacter::ApplyLegAngle(USceneComponent* Comp, const FRotator& RestRot, float AngleDeg) const
{
	// Build an incremental rotation around the configured local axis.
	const FQuat SwingQuat(LegSwingAxis.GetSafeNormal(), FMath::DegreesToRadians(AngleDeg));

	// Compose with the rest rotation so the animation is additive.
	const FRotator FinalRot = (FQuat(RestRot) * SwingQuat).Rotator();
	Comp->SetRelativeRotation(FinalRot);
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
	FocusedInteractable = nullptr;
	ActiveEmoteIndex   = -1;
	bEmoteBlendingOut  = false;

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

	// ── Re-añadir HUD widgets al viewport (cliente tras seamless travel) ─────
	// OnPossess solo se ejecuta en el SERVIDOR. En el cliente, PawnClientRestart
	// es el hook equivalente (disparado por ClientRestart RPC).
	// Los widgets del PC persisten entre mapas pero son eliminados del viewport
	// por UWorld::CleanupWorld durante la transición. Aquí los volvemos a añadir.
	if (AMP_GamePlayerController* PC = Cast<AMP_GamePlayerController>(GetController()))
	{
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

void ATortugaCharacter::Move(const FInputActionValue& Value)
{
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
	if (!GetWorld()) { FocusedInteractable = nullptr; return; }

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

		GrantInfiniteStamina(EquippedItem.StaminaUnlimitedDurationSeconds);
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

		// Timer para restablecer al tamaño original
		TWeakObjectPtr<ATortugaCharacter> WeakSelf(this);
		GetWorldTimerManager().SetTimer(BigHeadTimerHandle,
			[WeakSelf]()
			{
				if (ATortugaCharacter* C = WeakSelf.Get())
				{
					C->bBigHead = false;
					C->ApplyBigHeadVisual(false);
				}
			}, BigHeadDurationSeconds, false);

		return;
	}

	if ((EquippedItem.UseType == ETN_ItemUseType::Throwable)
		&& EquippedItem.ThrowableActorClass)
	{
		const FVector SpawnLocation = GetItemSpawnLocation();

		// ── Dirección de lanzamiento con arco parabólico ─────────────────
		// Aplanar la dirección de la cámara al plano horizontal (pitch=0),
		// luego tiltar hacia arriba por ThrowUpAngleDeg.
		// Así, mirar arriba/abajo NO anula el arco — siempre hay parábola.
		const FVector RawDirection = GetItemForwardDirection();
		const FVector FlatDirection = FVector(RawDirection.X, RawDirection.Y, 0.f).GetSafeNormal();
		
		// Si el jugador mira recto al suelo, fallback al forward del actor
		const FVector SafeFlatDir = FlatDirection.IsNearlyZero() ? GetActorForwardVector().GetSafeNormal2D() : FlatDirection;
		
		// Tiltar hacia arriba por el ángulo configurado.
		// FQuat usa right-hand rule: para que +angle eleve la dirección en UE (Z-up),
		// el eje debe ser Forward × Up (= Left), no Up × Forward (= Right).
		const FVector LeftVec = FVector::CrossProduct(SafeFlatDir, FVector::UpVector).GetSafeNormal();
		const FQuat UpTilt(LeftVec, FMath::DegreesToRadians(ThrowUpAngleDeg));
		const FVector ArcedDirection = UpTilt.RotateVector(SafeFlatDir).GetSafeNormal();

		const FVector LaunchVelocity = ArcedDirection * FMath::Max(EquippedItem.ThrowSpeed, 0.0f);

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		FTN_InventoryItem ConsumedItem;
		if (!InventoryComponent->TryConsumeEquippedItem(ConsumedItem))
		{
			return;
		}

		if (ATN_ThrowableItemActor* ThrowableActor = GetWorld()->SpawnActor<ATN_ThrowableItemActor>(EquippedItem.ThrowableActorClass, SpawnLocation, ArcedDirection.Rotation(), SpawnParams))
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
}

void ATortugaCharacter::ServerDropEquippedItem_Implementation()
{
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
	// Sin skin equipado → restaurar materiales originales cacheados en BeginPlay.
	if (SkinId == NAME_None)
	{
		for (auto& Pair : DefaultBodyMaterials)
		{
			if (UStaticMeshComponent* SMC = Pair.Key.Get())
			{
				for (int32 i = 0; i < Pair.Value.Num(); ++i)
				{
					SMC->SetMaterial(i, Pair.Value[i]);
				}
			}
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
	if (!Row || !Row->BodyMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TortugaCharacter] UpdateSkinVisual: SkinId '%s' no encontrado o sin material."), *SkinId.ToString());
		return;
	}

	// Componentes que reciben BodyMaterial (cuerpo principal).
	static const TArray<FName> BodyNames = { TEXT("Body"), TEXT("Body1") };

	// Componentes que reciben SkinMaterial (extremidades / detalles).
	static const TArray<FName> SkinNames = {
		TEXT("Mesh1"), TEXT("Mesh2"), TEXT("Mesh3"),
		TEXT("Mesh4"), TEXT("Mesh5"), TEXT("Mesh13")
	};

	int32 NumApplied = 0;
	for (UActorComponent* Comp : GetComponents())
	{
		UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Comp);
		if (!SMC || SMC == HelmetMeshComp) { continue; }

		const FName CompName = SMC->GetFName();

		UMaterialInterface* MatToApply = nullptr;
		if (BodyNames.Contains(CompName) && Row->BodyMaterial)
		{
			MatToApply = Row->BodyMaterial;
		}
		else if (SkinNames.Contains(CompName) && Row->SkinMaterial)
		{
			MatToApply = Row->SkinMaterial;
		}

		if (!MatToApply) { continue; }

		const int32 NumMats = SMC->GetNumMaterials();
		for (int32 i = 0; i < NumMats; ++i)
		{
			SMC->SetMaterial(i, MatToApply);
		}
		++NumApplied;
	}

	UE_LOG(LogTemp, Log, TEXT("[TortugaCharacter] '%s' skin '%s' aplicado a %d componentes."),
		*GetName(), *SkinId.ToString(), NumApplied);
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
}

// ── Knockdown ─────────────────────────────────────────────────────────────────

void ATortugaCharacter::ApplyKnockdown(float Duration)
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

	bIsKnockedDown = true;

	// Bloquear movimiento en servidor
	if (UCharacterMovementComponent* MC = GetCharacterMovement())
	{
		MC->DisableMovement();
	}

	// ── Knockdown visual via sistema de emotes ───────────────────────────────
	// ReplicatedEmoteIndex = KNOCKDOWN_EMOTE_ID replica la animación a todos los clientes
	// usando el mismo canal de replicación que los emotes normales (funciona perfectamente).
	ReplicatedEmoteIndex = KNOCKDOWN_EMOTE_ID;
	// Servidor: aplicar localmente (OnRep no dispara en quien posee la variable)
	StartEmoteLocally(KNOCKDOWN_EMOTE_ID);

	// ── DBNO heartbeat: solo el jugador local incapacitado oye el latido ──
	if (IsLocallyControlled())
	{
		PlayDBNOHeartbeatSound();
	}

	GetWorldTimerManager().SetTimer(KnockdownTimerHandle, this,
	                                &ATortugaCharacter::RecoverFromKnockdown, Duration, false);

	UE_LOG(LogTemp, Log, TEXT("[Knockdown] %s knocked down for %.1fs"), *GetNameSafe(this), Duration);
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
		if (UCharacterMovementComponent* MC = GetCharacterMovement())
		{
			if (bIsKnockedDown) { MC->DisableMovement(); }
			else                { MC->SetMovementMode(MOVE_Walking); }
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
		}
	}
}

void ATortugaCharacter::MulticastApplyKnockdownVisual_Implementation(bool bKnocked)
{
	// El visual del knockdown ahora es manejado por el sistema de emotes
	// (ReplicatedEmoteIndex = KNOCKDOWN_EMOTE_ID = 100).
	// Esta función se mantiene por compatibilidad de API pero es un no-op.
}

void ATortugaCharacter::ApplyKnockdownVisual(bool bKnocked)
{
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
		KnockedRot.Pitch -= 180.0f;
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
// DEATH VISUAL SYSTEM
// ─────────────────────────────────────────────────────────────────────────────

void ATortugaCharacter::SetDeadVisual(bool bDead)
{
	if (!HasAuthority()) { return; }

	bIsDead = bDead;

	// Listen-server: OnRep no se dispara localmente
	if (bDead) { HideLimbs(); } else { ShowLimbs(); }

	// Multicast fiable para todos los clientes
	MulticastSetDeadVisual(bDead);

	UE_LOG(LogTemp, Log, TEXT("[Death] %s dead visual = %s"), *GetNameSafe(this), bDead ? TEXT("HIDDEN") : TEXT("VISIBLE"));
}

void ATortugaCharacter::OnRep_IsDead()
{
	if (bIsDead) { HideLimbs(); } else { ShowLimbs(); }
}

void ATortugaCharacter::MulticastSetDeadVisual_Implementation(bool bDead)
{
	// El servidor ya lo aplicó en SetDeadVisual
	if (HasAuthority()) { return; }
	if (bDead) { HideLimbs(); } else { ShowLimbs(); }
}

void ATortugaCharacter::HideLimbs()
{
	auto HideComp = [](TWeakObjectPtr<USceneComponent>& Comp)
	{
		if (Comp.IsValid())
		{
			Comp->SetVisibility(false, true);
		}
	};
	HideComp(Brazo1);
	HideComp(Brazo2);
	HideComp(Pata1);
	HideComp(Pata2);
	HideComp(Cola);
	HideComp(Cabeza);

	if (HelmetMeshComp)
	{
		HelmetMeshComp->SetVisibility(false, true);
	}
}

void ATortugaCharacter::ShowLimbs()
{
	auto ShowComp = [](TWeakObjectPtr<USceneComponent>& Comp)
	{
		if (Comp.IsValid())
		{
			Comp->SetVisibility(true, true);
		}
	};
	ShowComp(Brazo1);
	ShowComp(Brazo2);
	ShowComp(Pata1);
	ShowComp(Pata2);
	ShowComp(Cola);
	ShowComp(Cabeza);

	if (HelmetMeshComp)
	{
		HelmetMeshComp->SetVisibility(true, true);
	}
}

void ATortugaCharacter::ServerTryReviveNearby_Implementation()
{
	if (!HasAuthority()) { return; }

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

	// Search for the nearest DBNO player in range
	if (const UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* OtherPC = It->Get();
			if (!OtherPC || OtherPC == GetController()) { continue; }

			const ATN_CoopPlayerState* OtherPS = OtherPC->GetPlayerState<ATN_CoopPlayerState>();
			if (!OtherPS || !OtherPS->bIsDBNO) { continue; }

			const APawn* OtherPawn = OtherPC->GetPawn();
			if (!OtherPawn) { continue; }

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

	// Validate: target still valid and in DBNO
	APlayerController* TargetPC = ReviveTargetPC.Get();
	if (!TargetPC)
	{
		CancelReviveChannel();
		return;
	}

	const ATN_CoopPlayerState* TargetPS = TargetPC->GetPlayerState<ATN_CoopPlayerState>();
	if (!TargetPS || !TargetPS->bIsDBNO)
	{
		CancelReviveChannel();
		return;
	}

	// Validate: still in range
	const APawn* TargetPawn = TargetPC->GetPawn();
	if (!TargetPawn)
	{
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

	// Reset all components to rest pose (clears stale location offsets)
	if (Brazo1.IsValid()) { Brazo1->SetRelativeRotation(Brazo1RestRot); Brazo1->SetRelativeLocation(Brazo1RestLoc); }
	if (Brazo2.IsValid()) { Brazo2->SetRelativeRotation(Brazo2RestRot); Brazo2->SetRelativeLocation(Brazo2RestLoc); }
	if (Pata1.IsValid())  { Pata1->SetRelativeRotation(Pata1RestRot);  Pata1->SetRelativeLocation(Pata1RestLoc);  }
	if (Pata2.IsValid())  { Pata2->SetRelativeRotation(Pata2RestRot);  Pata2->SetRelativeLocation(Pata2RestLoc);  }
	if (Cola.IsValid())   { Cola->SetRelativeRotation(ColaRestRot);    Cola->SetRelativeLocation(ColaRestLoc);    }
	if (Cabeza.IsValid()) { Cabeza->SetRelativeRotation(CabezaRestRot); Cabeza->SetRelativeLocation(CabezaRestLoc); }

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

	UE_LOG(LogTemp, Verbose, TEXT("[Emote] ServerSetEmote(%d) on %s  Cabeza=%s"),
		Index, *GetNameSafe(this),
		Cabeza.IsValid() ? TEXT("OK") : TEXT("NULL"));
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

	UE_LOG(LogTemp, Verbose, TEXT("[Emote] OnRep_ReplicatedEmoteIndex(%d) on %s  IsLocal=%s  Cabeza=%s"),
		ReplicatedEmoteIndex, *GetNameSafe(this),
		IsLocallyControlled() ? TEXT("YES") : TEXT("NO"),
		Cabeza.IsValid() ? TEXT("OK") : TEXT("NULL"));
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

	if (Brazo1.IsValid()) { SnapshotBrazo1 = Brazo1->GetRelativeRotation(); SnapshotBrazo1Loc = Brazo1->GetRelativeLocation(); }
	if (Brazo2.IsValid()) { SnapshotBrazo2 = Brazo2->GetRelativeRotation(); SnapshotBrazo2Loc = Brazo2->GetRelativeLocation(); }
	if (Pata1.IsValid())  { SnapshotPata1  = Pata1->GetRelativeRotation();  SnapshotPata1Loc  = Pata1->GetRelativeLocation();  }
	if (Pata2.IsValid())  { SnapshotPata2  = Pata2->GetRelativeRotation();  SnapshotPata2Loc  = Pata2->GetRelativeLocation();  }
	if (Cola.IsValid())   { SnapshotCola   = Cola->GetRelativeRotation();   SnapshotColaLoc   = Cola->GetRelativeLocation();   }
	if (Cabeza.IsValid()) { SnapshotCabeza = Cabeza->GetRelativeRotation(); SnapshotCabezaLoc = Cabeza->GetRelativeLocation(); }

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

void ATortugaCharacter::ApplyEmoteAngle(USceneComponent* Comp, const FRotator& Rest,
                                        float AngleDeg, const FVector& Axis) const
{
	// Swing en ESPACIO DEL PADRE: Swing * Rest.
	// Los ejes AX/AY/AZ son del padre (raíz del personaje), no del componente.
	// Así AX produce el mismo efecto visual en Brazo1 y Brazo2 aunque tengan rests espejados.
	const FQuat SwingQuat(Axis.GetSafeNormal(), FMath::DegreesToRadians(AngleDeg));
	Comp->SetRelativeRotation((SwingQuat * FQuat(Rest)).Rotator());
}

void ATortugaCharacter::ApplyEmoteAngles2(USceneComponent* Comp, const FRotator& Rest,
                                          float A1, const FVector& Ax1,
                                          float A2, const FVector& Ax2) const
{
	const FQuat Q1(Ax1.GetSafeNormal(), FMath::DegreesToRadians(A1));
	const FQuat Q2(Ax2.GetSafeNormal(), FMath::DegreesToRadians(A2));
	Comp->SetRelativeRotation((Q2 * Q1 * FQuat(Rest)).Rotator());
}

void ATortugaCharacter::ApplyEmoteAngles3(USceneComponent* Comp, const FRotator& Rest,
                                          float A1, const FVector& Ax1,
                                          float A2, const FVector& Ax2,
                                          float A3, const FVector& Ax3) const
{
	const FQuat Q1(Ax1.GetSafeNormal(), FMath::DegreesToRadians(A1));
	const FQuat Q2(Ax2.GetSafeNormal(), FMath::DegreesToRadians(A2));
	const FQuat Q3(Ax3.GetSafeNormal(), FMath::DegreesToRadians(A3));
	Comp->SetRelativeRotation((Q3 * Q2 * Q1 * FQuat(Rest)).Rotator());
}

void ATortugaCharacter::TickEmote(float DeltaTime)
{
	// ── Blend-out: lerp all components back to rest rotations & locations ─────
	if (bEmoteBlendingOut)
	{
		EmoteBlendOutTimer += DeltaTime;
		const float Alpha = FMath::Min(EmoteBlendOutTimer / FMath::Max(EmoteBlendOutDuration, KINDA_SMALL_NUMBER), 1.f);

		auto Blend = [Alpha](TWeakObjectPtr<USceneComponent>& Comp, const FRotator& SnapR, const FRotator& RestR,
		                     const FVector& SnapL, const FVector& RestL)
		{
			if (Comp.IsValid())
			{
				Comp->SetRelativeRotation(FMath::Lerp(SnapR, RestR, Alpha));
				Comp->SetRelativeLocation(FMath::Lerp(SnapL, RestL, Alpha));
			}
		};

		Blend(Brazo1, SnapshotBrazo1, Brazo1RestRot, SnapshotBrazo1Loc, Brazo1RestLoc);
		Blend(Brazo2, SnapshotBrazo2, Brazo2RestRot, SnapshotBrazo2Loc, Brazo2RestLoc);
		Blend(Pata1,  SnapshotPata1,  Pata1RestRot,  SnapshotPata1Loc,  Pata1RestLoc);
		Blend(Pata2,  SnapshotPata2,  Pata2RestRot,  SnapshotPata2Loc,  Pata2RestLoc);
		Blend(Cola,   SnapshotCola,   ColaRestRot,   SnapshotColaLoc,   ColaRestLoc);
		Blend(Cabeza, SnapshotCabeza, CabezaRestRot, SnapshotCabezaLoc, CabezaRestLoc);

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

	// ── Ejes de referencia (espacio del PADRE — T-Pose) ─────────────────────
	// Setup: SceneComponents a rot (0,0,0), brazos extendidos por ±Y (T-Pose).
	//
	// BRAZOS (mesh por ±Y desde el joint):
	//   AX (1,0,0) adelante  → sube/baja visto de frente  (+AX = arriba)
	//   AY (0,1,0) lateral   → roll sobre eje largo del brazo (casi invisible en cubos)
	//   AZ (0,0,1) arriba    → adelante/atrás             (+AZ = backward, −AZ = forward)
	//
	// CABEZA (mesh por +X desde el joint):
	//   AX = roll (rara vez útil)
	//   AY = cabeceo arriba/abajo (nod)
	//   AZ = girar izquierda/derecha (shake)
	//
	// PATAS (depende de orientación del mesh — si cuelgan por −Z):
	//   LX (1,0,0) = splay lateral (abrir/cerrar piernas)
	//   LY (0,1,0) = stride adelante/atrás  ← LegSwingAxis configurable
	//   LZ (0,0,1) = twist/giro
	//   Si las patas están en T-Pose (±Y), cambiar LegSwingAxis a (0,0,1).
	//
	// COLA (mesh por −X desde el joint):
	//   TY = arriba/abajo   TZ = lado a lado   TX = roll cola
	const FVector AX(1.f, 0.f, 0.f);
	const FVector AY(0.f, 1.f, 0.f);
	const FVector AZ(0.f, 0.f, 1.f);

	// Patas
	const FVector LX(1.f, 0.f, 0.f);       // splay lateral
	const FVector LY = LegSwingAxis;         // stride adelante/atrás (configurable)
	const FVector LZ(0.f, 0.f, 1.f);       // giro pata

	// Cola
	const FVector TY = TailUpDownAxis;
	const FVector TZ = TailSideAxis;
	const FVector TX(1.f, 0.f, 0.f);       // roll cola

	// ── Helpers de aplicación (rotación) ──────────────────────────────────────
	auto Ap = [&](TWeakObjectPtr<USceneComponent>& Comp, const FRotator& Rest,
	              float Angle, const FVector& Axis)
	{
		if (Comp.IsValid()) { ApplyEmoteAngle(Comp.Get(), Rest, Angle, Axis); }
	};

	auto Ap2 = [&](TWeakObjectPtr<USceneComponent>& Comp, const FRotator& Rest,
	               float A1, const FVector& Ax1, float A2, const FVector& Ax2)
	{
		if (Comp.IsValid()) { ApplyEmoteAngles2(Comp.Get(), Rest, A1, Ax1, A2, Ax2); }
	};

	auto Ap3 = [&](TWeakObjectPtr<USceneComponent>& Comp, const FRotator& Rest,
	               float A1, const FVector& Ax1,
	               float A2, const FVector& Ax2,
	               float A3, const FVector& Ax3)
	{
		if (Comp.IsValid()) { ApplyEmoteAngles3(Comp.Get(), Rest, A1, Ax1, A2, Ax2, A3, Ax3); }
	};

	// ── Helper de aplicación (traslación) — para Explosivo y Modo Loco 2 ────
	auto SetLoc = [&](TWeakObjectPtr<USceneComponent>& Comp, const FVector& RestLoc, const FVector& Offset)
	{
		if (Comp.IsValid()) { Comp->SetRelativeLocation(RestLoc + Offset); }
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
		Ap2(Brazo1, Brazo1RestRot,
		    env * 90.f,              AY,
		    env * 75.f * wave,       AX);

		// Brazo2 (izq): baja relajado
		Ap(Brazo2, Brazo2RestRot, env * 80.f, AX);

		// Cabeza — cabeceo visible mientras saluda
		Ap(Cabeza, CabezaRestRot, env * 20.f * S(2.f, T), AY);

		// Cola
		Ap(Cola, ColaRestRot, env * 10.f * S(1.5f, T), TZ);

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
		Ap2(Brazo1, Brazo1RestRot,
		    env * 110.f + bob * env,   AX,
		    env * 90.f,               AY);
		// Brazo2 (izq): arriba −AX 50° + palma al frente AY +90°
		Ap2(Brazo2, Brazo2RestRot,
		    -(env * 110.f + bob * env), AX,
		    env * 90.f,                AY);

		// Cabeza — mira hacia las manos que aplauden
		Ap(Cabeza, CabezaRestRot, env * (-25.f), AY);

		// Patas
		Ap(Pata1, Pata1RestRot,  10.f * S(2.f, T) * env, LY);
		Ap(Pata2, Pata2RestRot, -10.f * S(2.f, T) * env, LY);

		// Cola
		Ap(Cola, ColaRestRot, env * 20.f * S(3.f, T), TZ);

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
		Ap(Brazo1, Brazo1RestRot,  spin, AZ);
		Ap(Brazo2, Brazo2RestRot, -spin, AZ);

		// Patas: en los primeros 0.4s se posicionan ±90° en el eje frontal (LX)
		// y acto seguido giran en LZ igual que los brazos — efecto hélice
		const float legSetup = Sat(T / 0.4f);
		Ap2(Pata1, Pata1RestRot,
		     legSetup *  90.f,  LX,   // eje frontal +90°
		     spin,              LZ);  // giro continuo
		Ap2(Pata2, Pata2RestRot,
		     legSetup * -90.f,  LX,   // eje frontal -90° (opuesto)
		     spin,              LZ);  // giro continuo

		// Cabeza — cabeceo al ritmo del spin
		Ap(Cabeza, CabezaRestRot, 20.f * S(2.f, T), AY);


		// Cola
		Ap(Cola, ColaRestRot, 12.f * S(1.f, T), TZ);

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
		Ap3(Brazo1, Brazo1RestRot,
		    setup *  -90.f,  AX,
		    setup *  -90.f,  AZ,
		    palmada,        AY);
		// Brazo2 (izq): espejo (−AX, −AZ, misma AY)
		Ap3(Brazo2, Brazo2RestRot,
		    setup * 90.f,  AX,
		    setup * 90.f,  AZ,
		    palmada,        AY);

		// Cabeza asiente con cada palmada
		Ap(Cabeza, CabezaRestRot, 18.f * (palmada / -90.f), AY);

		// Cola menea
		Ap(Cola, ColaRestRot, 15.f * S(2.f, T), TZ);

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
		Ap2(Brazo1, Brazo1RestRot,
		    setup * -120.f,                      AX,
		    setup * (-20.f - 10.f * clap),       AY);

		// Brazo2 (izq): reposo abajo
		Ap(Brazo2, Brazo2RestRot, setup * 80.f, AX);

		// Cabeza — cabeceo al ritmo del aplauso
		Ap(Cabeza, CabezaRestRot, 15.f * S(6.f, T), AY);

		// Cola: entra rápido por −TY (0.2s), sale por el mismo lado (0.4s a partir de T=1.5)
		// — la vuelta es explícita para que no tome el arco opuesto en el blend-out.
		const float colOut = T > 2.5f ? Sat((T - 2.5f) / 0.1f) : 0.f;
		const float colEnv = colSnap * (1.f - colOut);
		Ap(Cola, ColaRestRot, colEnv * -175.f, TY);

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
		Ap2(Brazo1, Brazo1RestRot,
		    setup * (-10.f),    AX,
		    setup *  80.f,      AZ);
		// Brazo2 (izq):  atrás (−AZ), baja un poco (+AX = abajo para izq)
		Ap2(Brazo2, Brazo2RestRot,
		    setup *  10.f,      AX,
		    setup * (-80.f),    AZ);

		// Cabeza — gira al ritmo del baile
		Ap(Cabeza, CabezaRestRot, 15.f * S(2.5f, T), AZ);

		// Patas
		Ap2(Pata1, Pata1RestRot,
		     30.f * S(2.5f, T),   LY,
		     15.f * S(1.25f, T),  LX);
		Ap2(Pata2, Pata2RestRot,
		    -30.f * S(2.5f, T),   LY,
		    -15.f * S(1.25f, T),  LX);

		// Cola
		Ap(Cola, ColaRestRot, 10.f * S(2.5f, T), TZ);

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

		Ap2(Brazo1, Brazo1RestRot,
		    env * 50.f + drift,    AX,
		    env * (-80.f),         AZ);
		Ap2(Brazo2, Brazo2RestRot,
		    -(env * 50.f) + drift, AX,
		    env * 80.f,            AZ);

		Ap(Cabeza, CabezaRestRot, env * (-20.f) + drift, AY);

		Ap(Pata1, Pata1RestRot, (env * 80.f) + drift, LY);
		Ap(Pata2, Pata2RestRot, (env * 80.f) + drift, LY);

		Ap2(Cola, ColaRestRot,
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
		Ap2(Brazo1, Brazo1RestRot,
		    env * 25.f,        AX,
		    env * (-80.f),     AZ);

		// Brazo2 (izq): reposo abajo
		Ap(Brazo2, Brazo2RestRot, env * 80.f, AX);

		Ap(Cabeza, CabezaRestRot, env * 20.f, AY);
		Ap(Cola, ColaRestRot, env * 8.f * S(1.5f, T), TZ);

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
		Ap3(Brazo1, Brazo1RestRot,
		     50.f * S(3.0f, T),          AX,
		     40.f * S(2.1f, T),          AY,
		     20.f * S(4.7f, T),          AZ);
		Ap3(Brazo2, Brazo2RestRot,
		    -55.f * S(2.8f, T + 0.1f),   AX,
		    -40.f * S(3.3f, T + 0.2f),   AY,
		     25.f * S(4.1f, T),           AZ);
		Ap3(Pata1, Pata1RestRot,
		     35.f * S(4.0f, T),          LY,
		     20.f * S(2.7f, T),          LX,
		     10.f * S(5.0f, T),          LZ);
		Ap3(Pata2, Pata2RestRot,
		    -35.f * S(4.0f, T + 0.08f),  LY,
		    -20.f * S(2.7f, T + 0.15f),  LX,
		    -10.f * S(5.0f, T + 0.05f),  LZ);
		Ap3(Cola, ColaRestRot,
		    15.f * Cos(5.f, T),           TY,
		    25.f * S(7.f, T),             TZ,
		    10.f * S(3.f, T + 0.1f),      TX);
		Ap3(Cabeza, CabezaRestRot,
		    25.f * S(3.7f, T),            AY,
		    30.f * S(2.3f, T + 0.07f),    AZ,
		    15.f * S(5.5f, T),            AX);

		// Traslación: HIPER-EXAGERADA tipo explosión — ±250 cm
		const float locAmp = 250.f;
		SetLoc(Brazo1, Brazo1RestLoc, FVector(
		    locAmp * S(1.7f, T),  locAmp * S(2.3f, T + 0.1f),  locAmp * S(3.1f, T)));
		SetLoc(Brazo2, Brazo2RestLoc, FVector(
		    locAmp * S(2.1f, T + 0.05f), -locAmp * S(1.9f, T),  locAmp * S(3.5f, T + 0.1f)));
		SetLoc(Pata1, Pata1RestLoc, FVector(
		    locAmp * S(2.9f, T), -locAmp * S(1.3f, T + 0.15f), locAmp * S(3.7f, T)));
		SetLoc(Pata2, Pata2RestLoc, FVector(
		   -locAmp * S(2.3f, T + 0.1f),  locAmp * S(1.7f, T),  locAmp * Cos(3.3f, T)));
		SetLoc(Cola, ColaRestLoc, FVector(
		   -locAmp * S(1.1f, T), locAmp * S(2.9f, T + 0.05f), locAmp * S(4.1f, T)));
		SetLoc(Cabeza, CabezaRestLoc, FVector(
		    locAmp * 0.5f * S(2.5f, T), locAmp * 0.3f * S(3.1f, T + 0.12f), locAmp * 0.4f * S(4.3f, T)));

		break; // LOOP ∞
	}

	// ──────────────────────────────────────────────────────────────────────────
	// 9  FIESTA (MODO LOCO) — Caos puro solo rotación, sin separar.     LOOP ∞
	// ──────────────────────────────────────────────────────────────────────────
	case 9:
		Ap3(Brazo1, Brazo1RestRot,
		     50.f * S(3.0f, T),          AX,
		     40.f * S(2.1f, T),          AY,
		     20.f * S(4.7f, T),          AZ);
		Ap3(Brazo2, Brazo2RestRot,
		    -55.f * S(2.3f, T + 0.13f),  AX,
		    -35.f * S(3.1f, T),          AY,
		     25.f * S(1.7f, T + 0.08f),  AZ);
		Ap3(Pata1, Pata1RestRot,
		     35.f * S(4.0f, T),          LY,
		     20.f * S(2.7f, T),          LX,
		     10.f * S(5.0f, T),          LZ);
		Ap3(Pata2, Pata2RestRot,
		    -35.f * S(4.0f, T + 0.08f),  LY,
		    -20.f * S(2.7f, T + 0.15f),  LX,
		    -10.f * S(5.0f, T + 0.05f),  LZ);
		Ap3(Cola, ColaRestRot,
		    15.f * Cos(5.f, T),           TY,
		    25.f * S(7.f, T),             TZ,
		    10.f * S(3.f, T + 0.1f),      TX);
		Ap3(Cabeza, CabezaRestRot,
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
			const float Setup    = FMath::Min(T / 0.2f, 1.f);  // Pose completada en 0.2s
			const float EasedSetup = FMath::InterpEaseOut(0.f, 1.f, Setup, 2.f); // Deceleración natural
			const float Struggle = FMath::Sin(T * 4.f) * 5.f;  // ±5° oscilación suave de "lucha"

			// ── Rotar el cuerpo entero hacia atrás (componente "Cuerpo" / KnockdownVisualComp) ──
			// Este es el efecto principal: la tortuga cae de espaldas (como una tortuga real).
			if (KnockdownVisualComp.IsValid())
			{
				FRotator TargetRot = MeshDefaultRelativeRotation;
				TargetRot.Pitch -= 90.f;  // 90° hacia atrás = patas arriba
				KnockdownVisualComp->SetRelativeRotation(
					FMath::Lerp(MeshDefaultRelativeRotation, TargetRot, EasedSetup));
			}

			// Patas hacia arriba (eje de swing normal)
			Ap(Pata1, Pata1RestRot,  EasedSetup * (80.f + Struggle), LY);
			Ap(Pata2, Pata2RestRot,  EasedSetup * (-80.f + Struggle), LY);

			// Brazos extendidos hacia arriba
			Ap(Brazo1, Brazo1RestRot,  EasedSetup * (70.f + Struggle * 0.7f), AX);
			Ap(Brazo2, Brazo2RestRot,  EasedSetup * (-70.f + Struggle * 0.7f), AX);

			// Cabeza caída hacia atrás por gravedad
			Ap(Cabeza, CabezaRestRot, EasedSetup * 25.f, TY);

			// Cola ligeramente levantada
			Ap(Cola, ColaRestRot, EasedSetup * (-35.f + Struggle * 0.5f), TY);

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
	if (!Cabeza.IsValid())
	{
		return;
	}

	if (bBig)
	{
		const FVector NewScale = CabezaRestScale * BigHeadScale;
		Cabeza->SetRelativeScale3D(NewScale);
		UE_LOG(LogTemp, Log, TEXT("[BigHead] %s — cabeza escalada x%.1f"), *GetNameSafe(this), BigHeadScale);
	}
	else
	{
		Cabeza->SetRelativeScale3D(CabezaRestScale);
		UE_LOG(LogTemp, Log, TEXT("[BigHead] %s — cabeza restaurada"), *GetNameSafe(this));
	}
}

