// ─────────────────────────────────────────────────────────────────────────────
// TortugaCharacter — Knockdown, muerte y ragdoll.
//
// Definiciones extraídas de TortugaCharacter.cpp para mejorar la legibilidad.
// Misma clase ATortugaCharacter en otra unidad de traducción: sin cambios de
// lógica ni de replicación, solo organización.
// ─────────────────────────────────────────────────────────────────────────────

#include "Player/TortugaCharacter.h"
#include "Core/TN_Log.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Core/TN_CoopPlayerState.h"
#include "Game/TN_RunGameMode.h"

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

	UE_LOG(LogTortunabo, Log, TEXT("[Knockdown] %s knocked down for %.1fs (momentum activo)"), *GetNameSafe(this), Duration);
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

	UE_LOG(LogTortunabo, Log, TEXT("[Knockdown] %s recovered"), *GetNameSafe(this));
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

	if (bKnocked && KnockdownSound)
	{
		PlaySfxAtSelf(KnockdownSound);
	}
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
			// Chaos: Body->SetEnableGravity(true) propaga internamente al solver
			// (FPhysicsInterface::SetGravityEnabled_AssumesLocked). Mismo patrón
			// que EnterRagdollState (death) → consistencia entre rutas.
			// (Triple round 2 sugirió bEnableGravity + UpdatePhysicsProperties,
			// pero esa función no existe en FBodyInstance; SetEnableGravity es
			// la API canónica.)
			for (FBodyInstance* Body : SkelMesh->Bodies)
			{
				if (Body) { Body->SetEnableGravity(true); }
			}
			SkelMesh->SetAllPhysicsLinearVelocity(KnockdownInitialVel);
			SkelMesh->SetAllPhysicsAngularVelocityInRadians(FVector::ZeroVector);
			SkelMesh->WakeAllRigidBodies();
			SkelMesh->bPauseAnims = true;  // después de simulación activa
			UE_LOG(LogTortunabo, Warning, TEXT("[Diagnostic] Ragdoll KNOCKDOWN ON (%s) IsSim=%s InitVel=(%.0f,%.0f,%.0f)"),
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
		UE_LOG(LogTortunabo, Error, TEXT("[Knockdown] ApplyKnockdownVisual(%s): No visual component on %s — knockdown tilt invisible! "
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

	UE_LOG(LogTortunabo, Verbose, TEXT("[Knockdown] ApplyKnockdownVisual(%s) on %s — comp=%s"),
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

	UE_LOG(LogTortunabo, Log, TEXT("[Character] RequestKill on '%s' by '%s'"),
		*GetNameSafe(this), *GetNameSafe(KillInstigator));

	GM->MarkPlayerDead(PC);
}

// DEATH VISUAL SYSTEM
// ─────────────────────────────────────────────────────────────────────────────

void ATortugaCharacter::SetDeadVisual(bool bDead)
{
	if (!HasAuthority()) { return; }

	bIsDead = bDead;
	// DualMax round 2 — Codex CRITICAL: garantizar entrega del UPROPERTY replicado
	// a clientes especteadores. ForceNetUpdate fuerza al actor a entrar en next
	// replication pass; FlushNetDormancy despierta el actor si estaba dormant
	// (caso reportado: tras revive previo, segunda muerte no disparaba OnRep_IsDead
	// en cliente especteador porque cliente creía bIsDead=true).
	FlushNetDormancy();
	ForceNetUpdate();

	UE_LOG(LogTortunabo, Warning, TEXT("[DeathState][SERVER] %s SetDeadVisual(%d) bIsDead=%d RepMove=%d Dormant=%d Role=%d"),
		*GetName(), bDead, bIsDead, IsReplicatingMovement() ? 1 : 0, (int32)NetDormancy, (int32)GetLocalRole());

	USkeletalMeshComponent* SkelMesh = GetMesh();
	FVector GroundLocation = GetActorLocation();
	if (bDead)
	{
		// Do not snap death ragdoll down to the floor. Knockdown works because it
		// starts from the current pose/location; snapping to capsule half-height can
		// spawn low leg bodies already intersecting the ground.
		float RequiredLift = DeathRagdollSpawnLift;
		if (SkelMesh && GetWorld())
		{
			FHitResult GroundHit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(DeathRagdollFloorClearance), false, this);
			Params.AddIgnoredActor(this);
			const FVector ActorLoc = GetActorLocation();
			const FVector TraceStart = ActorLoc + FVector(0.f, 0.f, 150.f);
			const FVector TraceEnd = ActorLoc - FVector(0.f, 0.f, 1000.f);
			bool bFoundGround = GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_WorldStatic, Params);
			if (!bFoundGround)
			{
				bFoundGround = GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_WorldDynamic, Params);
			}
			if (bFoundGround)
			{
				const float MeshBottomZ = SkelMesh->Bounds.GetBox().Min.Z;
				const float DesiredBottomZ = GroundHit.ImpactPoint.Z + DeathRagdollFloorClearance;
				RequiredLift = FMath::Max(RequiredLift, DesiredBottomZ - MeshBottomZ);
			}
		}

		GroundLocation = GetActorLocation() + FVector(0.f, 0.f, RequiredLift);
		SetActorLocation(GroundLocation, false, nullptr, ETeleportType::TeleportPhysics);
		UE_LOG(LogTortunabo, Log, TEXT("[Death] Ragdoll spawn lift %.1fcm (base=%.1f clearance=%.1f)"),
			RequiredLift, DeathRagdollSpawnLift, DeathRagdollFloorClearance);

		EnterRagdollState();
		SetReplicateMovement(false);

		// R2: freeze diferido. Tras RagdollFreezeDelay de simulación local, el
		// servidor captura la posición autoritativa del cadáver y todas las
		// máquinas congelan y snapean (bRagdollFrozen + RagdollFrozenLoc).
		GetWorldTimerManager().SetTimer(RagdollFreezeTimerHandle,
			FTimerDelegate::CreateUObject(this, &ATortugaCharacter::ServerFreezeRagdoll),
			RagdollFreezeDelay, false);
	}
	else
	{
		bCanAirDash = true;

		// Cancelar el freeze pendiente y limpiar el flag replicado antes de
		// revivir (en clientes, OnRep_RagdollFrozen(false) es un no-op).
		GetWorldTimerManager().ClearTimer(RagdollFreezeTimerHandle);
		bRagdollFrozen = false;

		ExitRagdollState();

		// Restaurar movement replication para el pawn revivido.
		SetReplicateMovement(true);
	}

	MulticastSetDeadVisual(bDead, GroundLocation);

	// Servidor / listen-server: disparar el evento BP aquí (los clientes lo reciben
	// dentro de MulticastSetDeadVisual_Implementation).
	OnDeathVisualSet(bDead);

	UE_LOG(LogTortunabo, Log, TEXT("[Death] %s dead visual = %s"), *GetNameSafe(this), bDead ? TEXT("RAGDOLL") : TEXT("ALIVE"));
}

void ATortugaCharacter::EnterRagdollState()
{
	USkeletalMeshComponent* SkelMesh = GetMesh();
	UE_LOG(LogTortunabo, Warning, TEXT("[Ragdoll][ENTER_BEGIN] %s NetMode=%d Role=%d MeshSim=%d Bodies=%d PA=%s"),
		*GetName(), (int32)GetNetMode(), (int32)GetLocalRole(),
		SkelMesh && SkelMesh->IsSimulatingPhysics() ? 1 : 0,
		SkelMesh ? SkelMesh->Bodies.Num() : -1,
		SkelMesh && SkelMesh->GetPhysicsAsset() ? TEXT("YES") : TEXT("NO"));

	if (!SkelMesh || !SkelMesh->GetPhysicsAsset())
	{
		UE_LOG(LogTortunabo, Warning, TEXT("[Ragdoll] EnterRagdollState abortado — SkM o PhysicsAsset null en %s"), *GetName());
		return;
	}

	// Idempotente: si ya simula no re-arranca.
	if (SkelMesh->IsSimulatingPhysics())
	{
		UE_LOG(LogTortunabo, Warning, TEXT("[Ragdoll][EARLY_OUT_ALREADY_SIM] %s Bodies=%d"),
			*GetName(), SkelMesh->Bodies.Num());
		return;
	}

	// 1. Stop montages, but keep the same startup order as knockdown ragdoll:
	//    simulate first, pause animation after bodies are awake. This avoids a
	//    death-only path where a frozen/custom pose starts partially interpenetrating
	//    the floor and Chaos keeps resolving it forever.
	if (UAnimInstance* AnimInst = SkelMesh->GetAnimInstance())
	{
		AnimInst->Montage_Stop(0.f);
		AnimInst->StopAllMontages(0.f);
	}

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

	// 5. (movido al caller — DualMax round 3): el LineTrace + SetActorLocation
	//    se hacen ANTES de EnterRagdollState. Server lo calcula en SetDeadVisual
	//    y lo envía como param del Multicast; cliente lo aplica en
	//    MulticastSetDeadVisual_Implementation antes de llamar EnterRagdollState.
	//    Esto elimina la divergencia client/server por LineTrace local
	//    inconsistente y la race condition entre RPC y bReplicateMovement.

	// 6. Use the same collision setup as knockdown. The Ragdoll profile should be
	//    authored in the PhysicsAsset/project settings; do not override it here.
	SkelMesh->SetCollisionProfileName(RagdollCollisionProfile);

	// 7. (movido al paso 11 — bBlendPhysics se setea AL FINAL, después de
	//    SetSimulate + Wake. Triple Mode round 2 — Gemini 8/10: setear
	//    bBlendPhysics ANTES de simulate hacía que los bodies pelearan con la
	//    kinematic anim pose; el solver aplicaba constraint resolution suave
	//    que se veía como "vuela lentamente sin gravedad". Bodies arrancan sim
	//    primero dictando pose; blend se activa después con bodies ya en control.)

	// 8. Damping: dejar valores del PhysicsAsset (NO forzar 0). DualMax round 2
	//    — Codex CRITICAL: damping=0 era mala idea para cadáveres porque Chaos
	//    tenía menos disipación de energía → micro-vibración persistente al
	//    asentarse (jitter reportado). El PhysicsAsset del mannequin ya tiene
	//    damping moderado calibrado. Si en playtests el ragdoll sigue
	//    micro-vibrando, aplicar uniforme: LinearDamping=0.8, AngularDamping=1.5.

	// 9. Activar simulación en TODOS los bodies. Llamar SOLO SetAllBodies; añadir
	//    SetSimulatePhysics(true) puede sobrescribir el state del root body que
	//    SetAllBodies acaba de configurar.
	SkelMesh->SetAllBodiesSimulatePhysics(true);
	SkelMesh->SetAllBodiesPhysicsBlendWeight(1.f);
	SkelMesh->SetEnableGravity(true);
	// Chaos: Body->SetEnableGravity(true) — esta API propaga al solver vía
	// FPhysicsInterface::SetGravityEnabled_AssumesLocked cuando IsSimulatingPhysics
	// es true. Como llegamos aquí DESPUÉS de SetAllBodiesSimulatePhysics(true),
	// los bodies sí están simulando → propagación correcta.
	// (Triple round 2 sugirió bEnableGravity + UpdatePhysicsProperties, pero esa
	// función no existe en FBodyInstance; SetEnableGravity es la API canónica.)
	for (FBodyInstance* Body : SkelMesh->Bodies)
	{
		if (Body) { Body->SetEnableGravity(true); }
	}

	// 10. Vel inicial CERO defensivo (después de simulate, no antes — antes los
	//     bodies aún no existían en el simulator).
	SkelMesh->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
	SkelMesh->SetAllPhysicsAngularVelocityInRadians(FVector::ZeroVector);
	SkelMesh->WakeAllRigidBodies();

	// 11. bBlendPhysics AL FINAL — bodies ya simulan + están awake; ahora el
	//     blend permite que la sim física domine sobre cualquier pose anim
	//     residual sin que los bodies tengan que pelear con kinematic targets.
	SkelMesh->bBlendPhysics = true;
	SkelMesh->bPauseAnims = true;

	// Marca local del arranque de la simulación — ApplyRagdollFreeze la usa para
	// diferir el freeze si la sim acaba de empezar (caso JIP).
	if (const UWorld* World = GetWorld())
	{
		LocalRagdollStartTime = World->GetTimeSeconds();
	}

	UE_LOG(LogTortunabo, Warning, TEXT("[Ragdoll] ENTER (%s) auth=%d · AnimPaused=true(post-wake) · BlendPhysics=true(post-wake) · bodies=%d"),
		*GetName(), HasAuthority(), SkelMesh->Bodies.Num());
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
	}
	SkelMesh->SetAllBodiesPhysicsBlendWeight(0.f);
	SkelMesh->bBlendPhysics = false;
	SkelMesh->bPauseAnims = false;
	SkelMesh->SetEnableGravity(false);

	// Restaurar AnimBP — EnterRagdollState lo apagó con AnimationCustomMode.
	SkelMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
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

	UE_LOG(LogTortunabo, Log, TEXT("[Ragdoll] EXIT (%s) authority=%d"), *GetName(), HasAuthority());
}

void ATortugaCharacter::OnRep_IsDead()
{
	USkeletalMeshComponent* SkM = GetMesh();
	UE_LOG(LogTortunabo, Warning, TEXT("[DeathState][ONREP] %s bIsDead=%d NetMode=%d Role=%d RemoteRole=%d MeshSim=%d Bodies=%d PA=%s"),
		*GetName(), bIsDead, (int32)GetNetMode(), (int32)GetLocalRole(), (int32)GetRemoteRole(),
		SkM && SkM->IsSimulatingPhysics() ? 1 : 0,
		SkM ? SkM->Bodies.Num() : -1,
		SkM && SkM->GetPhysicsAsset() ? TEXT("YES") : TEXT("NO"));
	// JIP late-join: cliente recibe bIsDead replicado y aplica el state
	// correspondiente vía las helpers compartidas (mismo patrón canónico).
	if (bIsDead) { EnterRagdollState(); }
	else         { ExitRagdollState();  }
}

void ATortugaCharacter::MulticastSetDeadVisual_Implementation(bool bDead, FVector GroundLocation)
{
	UE_LOG(LogTortunabo, Warning, TEXT("[DeathState][MC] %s bDead=%d NetMode=%d HasAuthority=%d Role=%d RemoteRole=%d Ground=(%.0f,%.0f,%.0f)"),
		*GetName(), bDead, (int32)GetNetMode(), HasAuthority() ? 1 : 0, (int32)GetLocalRole(), (int32)GetRemoteRole(),
		GroundLocation.X, GroundLocation.Y, GroundLocation.Z);

	// Sonido de muerte: se reproduce en TODAS las máquinas (incluido listen-server)
	// antes del early-return de autoridad. Anclado a la pos actual del actor — el
	// teleport a GroundLocation aún no ha ocurrido, así que el sonido sale en la
	// pos visible donde murió.
	if (bDead && KillSound)
	{
		PlaySfxAtSelf(KillSound);
	}

	if (HasAuthority()) { return; }  // server ya lo hizo en SetDeadVisual
	if (bDead)
	{
		// DualMax round 3: cliente teleporta a la pos GROUND-SNAP autoritativa
		// recibida con el RPC (no espera replicación de bReplicateMovement, que
		// llega después del MC). Después arranca sim en la pos correcta.
		SetActorLocation(GroundLocation, false, nullptr, ETeleportType::TeleportPhysics);
		EnterRagdollState();
	}
	else
	{
		ExitRagdollState();
	}
	// Notificar BP para que active el raptor, VFX, audio de muerte, etc.
	OnDeathVisualSet(bDead);
}

// ── R2: freeze/snap replicado del ragdoll de muerte ───────────────────────────

void ATortugaCharacter::ServerFreezeRagdoll()
{
	if (!HasAuthority() || !bIsDead || bRagdollFrozen)
	{
		return;
	}

	USkeletalMeshComponent* SkelMesh = GetMesh();
	if (!SkelMesh || !SkelMesh->IsSimulatingPhysics())
	{
		UE_LOG(LogTortunabo, Warning, TEXT("[Ragdoll][FREEZE] abortado en %s — mesh null o sin simular"), *GetName());
		return;
	}

	// Posición autoritativa del root bone. La loc se escribe ANTES que el flag:
	// ambas propiedades viajan en el mismo bunch y el OnRep del flag la lee.
	RagdollFrozenLoc = SkelMesh->GetBoneLocation(SkelMesh->GetBoneName(0), EBoneSpaces::WorldSpace);
	bRagdollFrozen = true;
	FlushNetDormancy();
	ForceNetUpdate();

	// Listen-host: OnRep no dispara en la máquina con autoridad.
	ApplyRagdollFreeze();
}

void ATortugaCharacter::OnRep_RagdollFrozen()
{
	if (bRagdollFrozen && bIsDead)
	{
		ApplyRagdollFreeze();
	}
	// bRagdollFrozen=false llega con el revive: ExitRagdollState (vía
	// OnRep_IsDead / MulticastSetDeadVisual) ya restaura el estado vivo.
}

void ATortugaCharacter::ApplyRagdollFreeze()
{
	USkeletalMeshComponent* SkelMesh = GetMesh();
	if (!bIsDead || !bRagdollFrozen || !SkelMesh)
	{
		return;
	}
	// Ya congelado en esta máquina (o revive en vuelo apagó la sim) → no-op.
	if (!SkelMesh->IsSimulatingPhysics())
	{
		return;
	}

	// JIP: bIsDead y bRagdollFrozen llegan en el mismo bunch inicial y el
	// ragdoll local acaba de arrancar. Petrificar la pose de spawn de pie se ve
	// roto — dejar ~0.5s de simulación para que Chaos pose el cuerpo primero.
	if (const UWorld* World = GetWorld())
	{
		constexpr float MinSimSeconds = 0.5f;
		const float SimTime = World->GetTimeSeconds() - LocalRagdollStartTime;
		if (LocalRagdollStartTime >= 0.f && SimTime < MinSimSeconds)
		{
			GetWorldTimerManager().SetTimer(RagdollFreezeTimerHandle,
				FTimerDelegate::CreateUObject(this, &ATortugaCharacter::ApplyRagdollFreeze),
				MinSimSeconds - SimTime, false);
			return;
		}
	}

	// Congelar: con bPauseAnims=true + bBlendPhysics, la última pose física está
	// cacheada en component-space → detener la simulación no produce salto.
	SkelMesh->PutAllRigidBodiesToSleep();
	SkelMesh->SetAllBodiesSimulatePhysics(false);

	// Snap por delta a la posición autoritativa: mover el ACTOR traslada el
	// cadáver congelado entero (la pose es component-space y el mesh sigue
	// attachado al capsule). En el server el delta es ~0; en clientes corrige
	// la divergencia acumulada de Chaos. Orden crítico: sim parada ANTES de
	// mover — los bodies simulados no siguen al actor, los kinematic sí.
	const FVector LocalRootLoc = SkelMesh->GetBoneLocation(SkelMesh->GetBoneName(0), EBoneSpaces::WorldSpace);
	const FVector Delta = FVector(RagdollFrozenLoc) - LocalRootLoc;
	AddActorWorldOffset(Delta, false, nullptr, ETeleportType::TeleportPhysics);

	UE_LOG(LogTortunabo, Log, TEXT("[Ragdoll][FREEZE] %s auth=%d delta=(%.0f,%.0f,%.0f) |delta|=%.0fcm"),
		*GetName(), HasAuthority() ? 1 : 0, Delta.X, Delta.Y, Delta.Z, Delta.Size());
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

