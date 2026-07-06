// ─────────────────────────────────────────────────────────────────────────────
// TortugaCharacter — Sistema de Dive (dash direccional).
//
// Definiciones extraídas de TortugaCharacter.cpp (que superaba las 4000 líneas)
// para mejorar la legibilidad. Es la MISMA clase ATortugaCharacter en otra unidad
// de traducción: sin cambios de lógica ni de replicación.
// ─────────────────────────────────────────────────────────────────────────────

#include "Player/TortugaCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

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

