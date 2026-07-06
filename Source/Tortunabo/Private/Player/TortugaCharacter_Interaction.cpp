// ─────────────────────────────────────────────────────────────────────────────
// TortugaCharacter — Interacción y uso de ítems.
//
// Definiciones extraídas de TortugaCharacter.cpp para mejorar la legibilidad:
// escaneo de interactuables (UpdateFocusedInteractable), Server RPCs de
// interacción/uso/drop de ítems y helpers de spawn. Misma clase ATortugaCharacter
// en otra unidad de traducción: sin cambios de lógica ni de replicación.
// ─────────────────────────────────────────────────────────────────────────────

#include "Player/TortugaCharacter.h"
#include "Player/TN_InventoryComponent.h"
#include "Player/TN_StaminaComponent.h"
#include "World/TN_InteractableBase.h"
#include "World/TN_PickupInteractableBase.h"
#include "World/TN_ThrowableItemActor.h"
#include "World/TN_ConchPickup.h"
#include "World/TN_InkProjectile.h"
#include "Core/TN_CoopPlayerState.h"
#include "Game/TN_RunGameMode.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "Net/UnrealNetwork.h"
#include "Engine/OverlapResult.h"

// El CVar de debug se define (con linkage externo) en TortugaCharacter.cpp
extern TAutoConsoleVariable<int32> CVarDebugInteraction;

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

