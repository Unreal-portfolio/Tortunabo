// ─────────────────────────────────────────────────────────────────────────────
// TortugaCharacter — Sistema de Dive (dash direccional).
//
// Definiciones extraídas de TortugaCharacter.cpp (que superaba las 4000 líneas)
// para mejorar la legibilidad. Es la MISMA clase ATortugaCharacter en otra unidad
// de traducción: sin cambios de lógica ni de replicación.
// ─────────────────────────────────────────────────────────────────────────────

#include "Player/TortugaCharacter.h"
#include "Core/TN_Log.h"
#include "Player/TN_TurtleMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

// ── Dive System ───────────────────────────────────────────────────────────────
//
// Flow:
//   Input (client)  → TryDive()
//   → Server_StartDive(DiveDir)      — validates, applies physics, sets bIsDiving (+ DiveSerial)
//   → Multicast_OnDiveVisual(true)   — all clients apply tilt + capsule resize
//   Al caer de tripa, UTN_TurtleMovementComponent arrastra a la tortuga dentro de la simulación del movimiento
//   (predicha en el cliente dueño): inercia, rozamiento por superficie, pendientes y rebotes. Casi parada, se levanta
//   (cápsula de pie sin atravesar nada) y, con eso, TickDive (servidor) llama a EndDive().
//   → bIsDiving = false              — OnRep_IsDiving fires on clients → restore
//
// Visual pattern mirrors knockdown: same KnockdownVisualComp, same NetworkSmoothingMode
// disable, but pitch is FORWARD (-85°) instead of backward.
// Capsule HalfHeight is reduced to simulate a horizontal hitbox.
// ─────────────────────────────────────────────────────────────────────────────

UTN_TurtleMovementComponent* ATortugaCharacter::GetTurtleMovement() const
{
	return Cast<UTN_TurtleMovementComponent>(GetCharacterMovement());
}

float ATortugaCharacter::GetStandingCapsuleHalfHeight() const
{
	// La cápsula de la clase (el objeto por defecto del Blueprint): nunca está encogida por un panzazo.
	const ATortugaCharacter* DefaultTurtle = GetClass() ? GetClass()->GetDefaultObject<ATortugaCharacter>() : nullptr;
	const UCapsuleComponent* DefaultCapsule = DefaultTurtle ? DefaultTurtle->GetCapsuleComponent() : nullptr;
	if (DefaultCapsule && DefaultCapsule->GetUnscaledCapsuleHalfHeight() > 1.f)
	{
		return DefaultCapsule->GetUnscaledCapsuleHalfHeight();
	}
	return DiveCapsuleOrigHalfHeight;
}

bool ATortugaCharacter::IsBellyPoseActive() const
{
	if (!bIsDiving)
	{
		return false;
	}
	// El dueño y el servidor simulan el movimiento: saben al momento que ya se ha levantado. En el resto de máquinas el
	// componente no lleva fase y manda el panzazo replicado.
	const UTN_TurtleMovementComponent* TurtleMove = GetTurtleMovement();
	return !(TurtleMove && TurtleMove->HasStoodUpFromDive(DiveSerial));
}

bool ATortugaCharacter::IsBellyOnGround() const
{
	const UCharacterMovementComponent* CMC = GetCharacterMovement();
	return IsBellyPoseActive() && CMC && CMC->IsMovingOnGround();
}

void ATortugaCharacter::TryDive()
{
	// Only locally controlled, not already diving, not knocked down
	if (!IsLocallyControlled() || bIsDiving || bIsKnockedDown || bIsDead || IsInShell())
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

	UE_LOG(LogTortunabo, Log,
		TEXT("[Dive] DASH-03 · DiveDir(camera)=(%.2f,%.2f,%.2f) ControlYaw=%.1f"),
		DiveDir.X, DiveDir.Y, DiveDir.Z, ControlRot.Yaw);

	Server_StartDive(DiveDir);
}

bool ATortugaCharacter::Server_StartDive_Validate(FVector DiveDir)
{
	// Red de seguridad de engine: el cliente legítimo manda una dirección (unit
	// vector o casi). NaN o magnitudes absurdas = paquete manipulado → kick.
	// _Implementation sigue saneando (Z=0, normalize con fallback a forward).
	return !DiveDir.ContainsNaN() && DiveDir.SizeSquared() <= 100.f;
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

		UE_LOG(LogTortunabo, Log,
			TEXT("[Dive] Momentum · JumpStartSpeed=%.0f CamVsJumpAlign=%.2f Factor=%.2f Bonus=%.0f → Total=%.0f"),
			JumpStartSpeed, Alignment, Factor, MomentumBonus, DiveForwardSpeed + MomentumBonus);
	}
	else
	{
		UE_LOG(LogTortunabo, Log,
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

	// Activate dive state — triggers OnRep on clients. El número nuevo le dice al movimiento que este panzazo aún no se
	// ha arrastrado (al dar la vuelta se salta el 0, que significa «ninguno»).
	DiveSerial    = DiveSerial >= 255 ? static_cast<uint8>(1) : static_cast<uint8>(DiveSerial + 1);
	bIsDiving     = true;
	DiveLockTimer = 0.f;

	// Apply visual on all machines (server runs Multicast locally too)
	Multicast_OnDiveVisual(true);

	UE_LOG(LogTortunabo, Log, TEXT("[Dive] %s — dive started, dir=%s"), *GetNameSafe(this), *DiveDir.ToString());
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

	UE_LOG(LogTortunabo, Log, TEXT("[Dive] %s — dive ended."), *GetNameSafe(this));
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
			// Cancelar interpolación pendiente sin tocar el giro del mesh — ya lo
			// gestiona ApplyKnockdownVisual / SetDeadVisual. La subida del panzazo sí se deshace.
			if (DiveTiltAlpha > 0.f && SkelMeshGuard)
			{
				SkelMeshGuard->SetRelativeLocation(DiveMeshDefaultLoc);
				SkelMeshGuard->SetRelativeScale3D(DiveMeshDefaultScale);
			}
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
	// Tumbada mientras dure la pose de panzazo: en el dueño y el servidor acaba en cuanto el movimiento la levanta (sin
	// esperar al fin del panzazo replicado); en el resto, con el fin del panzazo.
	const bool bBellyPose = IsBellyPoseActive();
	if (bBellyPose || DiveTiltAlpha > 0.f)
	{
		const float TargetAlpha = bBellyPose ? 1.f : 0.f;
		// Al levantarse del suelo, algo más despacio: se ve el empujón de brazos de UTN_TurtleAnimInstance.
		const UCharacterMovementComponent* TiltMove = GetCharacterMovement();
		const bool bGettingUpFromGround = !bBellyPose && TiltMove && !TiltMove->IsFalling();
		DiveTiltAlpha = FMath::FInterpTo(DiveTiltAlpha, TargetAlpha, DeltaTime, bGettingUpFromGround ? DiveGetUpTiltSpeed : DiveTiltSpeed);
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

			// La malla gira sobre sus pies y la cápsula encoge a DiveCapsuleHalfHeight: se sube para que los pies
			// queden a DiveBellyPivotHeight del suelo y el cuerpo tumbado se apoye en la tripa en vez de hundirse; y
			// se aplasta un poco contra el suelo (ejes locales de la malla: X ancho, Y tripa-espalda, Z largo). Con la
			// cápsula que haya ahora: al levantarse ya está de pie y la subida baja con el giro, sin dar un salto.
			const double CurrentCapsuleHalf = GetCapsuleComponent() ? GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() : DiveCapsuleHalfHeight;
			const double Lift = (DiveBellyPivotHeight - CurrentCapsuleHalf - DiveMeshDefaultLoc.Z) * env;
			SkelMesh->SetRelativeLocation(DiveMeshDefaultLoc + FVector(0.0, 0.0, FMath::Max(0.0, Lift)));
			SkelMesh->SetRelativeScale3D(DiveMeshDefaultScale * FMath::Lerp(FVector::OneVector, DiveSquash, static_cast<double>(env)));
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

		// Snap everything back to exact rest once fade-out completes (la cápsula solo cuando el panzazo ha acabado: si ya
		// se levantó, el movimiento la puso de pie).
		if (!bBellyPose && DiveTiltAlpha == 0.f)
		{
			if (!bIsDiving)
			{
				GetCapsuleComponent()->SetCapsuleHalfHeight(DiveCapsuleOrigHalfHeight);
			}
			if (USkeletalMeshComponent* SkelMesh = GetMesh())
			{
				SkelMesh->SetRelativeRotation(DiveMeshDefaultRot);
				SkelMesh->SetRelativeLocation(DiveMeshDefaultLoc);
				SkelMesh->SetRelativeScale3D(DiveMeshDefaultScale);
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

	// 1) Ya se ha levantado del arrastre (lo decide el movimiento, predicho igual en el cliente dueño): fin del panzazo.
	const UTN_TurtleMovementComponent* TurtleMove = GetTurtleMovement();
	if (TurtleMove && TurtleMove->HasStoodUpFromDive(DiveSerial))
	{
		EndDive();
		return;
	}
	// 2) Al agua: se acaba y nada. Metida en el caparazón (caída larga) o sin movimiento (ragdoll, caja física): el
	// movimiento no corre, así que tampoco se levantaría; se acaba aquí.
	const UCharacterMovementComponent* RecoveryMove = GetCharacterMovement();
	if (RecoveryMove && (RecoveryMove->IsSwimming() || RecoveryMove->MovementMode == MOVE_None))
	{
		EndDive();
		return;
	}
	if (IsInShell())
	{
		EndDive();
		return;
	}
	// 3) Sobre la tripa manda el movimiento (se levantará él); tope de seguridad por si algo lo deja colgado.
	if (TurtleMove && TurtleMove->IsOnBelly())
	{
		if (DiveLockTimer >= DiveMaxSeconds)
		{
			UE_LOG(LogTortunabo, Warning, TEXT("[Dive] %s — panzazo colgado %.1f s sobre la tripa: se acaba."), *GetNameSafe(this), DiveLockTimer);
			EndDive();
		}
		return;
	}

	// 4) En el aire (o sin arrastre, TN.Dive.Slide 0): como siempre, tras el bloqueo mínimo, si se ha parado (contra una
	// pared en pleno vuelo, por ejemplo) se acaba.
	if (DiveLockTimer < DiveMinLockDuration) { return; }

	const float Speed2D = GetVelocity().Size2D();
	if (Speed2D <= DiveStopSpeedThreshold || DiveLockTimer >= DiveMaxSeconds)
	{
		EndDive();
	}
}

