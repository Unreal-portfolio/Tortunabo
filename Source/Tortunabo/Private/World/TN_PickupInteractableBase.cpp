#include "World/TN_PickupInteractableBase.h"
#include "Player/TN_InventoryComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/DataTable.h"
#include "TimerManager.h"

ATN_PickupInteractableBase::ATN_PickupInteractableBase()
{
	PromptText = FText::FromString(TEXT("Recoger"));
}

void ATN_PickupInteractableBase::BeginPlay()
{
	Super::BeginPlay();

	// ── Aplicar estado "taken" desde la replicación inicial ──────────────────
	// Si un cliente se une tarde y el pickup ya fue recogido, bTaken=true
	// llegará en la replicación inicial. OnRep_Taken dispara al recibir el valor
	// pero como seguridad extra también lo aplicamos aquí.
	if (bTaken)
	{
		ApplyTakenState();
	}

	// ── Auto-configuración desde DataTable ────────────────────────────────────
	// Solo el servidor inicializa; PickupItem está replicado y llegará a clientes.
	if (HasAuthority() && ItemDataTable && !ItemRowName.IsNone())
	{
		const FTN_InventoryItem* Row = ItemDataTable->FindRow<FTN_InventoryItem>(
			ItemRowName,
			TEXT("ATN_PickupInteractableBase::BeginPlay"));

		if (Row)
		{
			InitializeFromInventoryItem(*Row);
			UE_LOG(LogTemp, Log, TEXT("[Pickup] ✓ '%s' auto-configurado desde DataTable row '%s' — ItemId=%s  UseType=%s"),
				*GetName(),
				*ItemRowName.ToString(),
				*PickupItem.ItemId.ToString(),
				*UEnum::GetValueAsString(PickupItem.UseType));
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Pickup] ✗ Fila '%s' no encontrada en DataTable '%s'. "
				     "Verifica el nombre en DT_Items."),
				*ItemRowName.ToString(),
				*ItemDataTable->GetName());
		}
	}
	else if (HasAuthority() && PickupItem.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("[Pickup] '%s' — usando PickupItem pre-configurado (sin DataTable): ItemId=%s"),
			*GetName(), *PickupItem.ItemId.ToString());
	}
	else if (HasAuthority() && !ItemDataTable)
	{
		// Solo advertir si NO hay DataTable en absoluto: actor colocado en nivel sin configurar.
		// Si DataTable está asignado pero RowName=None, es un spawn dinámico válido:
		// InitializeFromInventoryItem() será llamado justo después de SpawnActor().
		UE_LOG(LogTemp, Warning,
			TEXT("[Pickup] ✗ '%s' — Sin DataTable ni PickupItem. "
			     "Asigna ItemDataTable+ItemRowName en el BP (actor de nivel), o "
			     "llama InitializeFromInventoryItem() tras SpawnActor (spawn dinámico)."),
			*GetName());
	}
}

bool ATN_PickupInteractableBase::CanInteract(APawn* Interactor) const
{
	if (!Super::CanInteract(Interactor))
	{
		UE_LOG(LogTemp, Verbose, TEXT("[Pickup:CanInteract] Base CanInteract=FALSE (disabled or no interactor)"));
		return false;
	}

	if (bTaken)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[Pickup:CanInteract] '%s' already taken"), *GetName());
		return false;
	}

	if (!Interactor)
	{
		return false;
	}

	if (!PickupItem.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Pickup:CanInteract] '%s' — PickupItem is INVALID (ItemId=None). "
			"Assign ItemDataTable + ItemRowName, or call InitializeFromInventoryItem."), *GetName());
		return false;
	}

	const UTN_InventoryComponent* InventoryComponent = Interactor->FindComponentByClass<UTN_InventoryComponent>();
	if (!InventoryComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Pickup:CanInteract] Interactor '%s' has NO InventoryComponent"), *Interactor->GetName());
		return false;
	}

	// Si el inventario está lleno (2 slots ocupados) NO permitir recoger:
	// el ítem se queda en el suelo. bAllowReplaceIfFull=false evita borrar
	// silenciosamente el ítem equipado actual al coger uno nuevo.
	const bool bCanReceive = InventoryComponent->CanReceiveItem(PickupItem, false);
	if (!bCanReceive)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[Pickup:CanInteract] Inventario lleno — '%s' no se puede recoger"), *GetName());
	}
	return bCanReceive;
}

void ATN_PickupInteractableBase::Interact(APawn* Interactor)
{
	if (!HasAuthority() || !CanInteract(Interactor) || !Interactor)
	{
		return;
	}

	UTN_InventoryComponent* InventoryComponent = Interactor->FindComponentByClass<UTN_InventoryComponent>();
	if (!InventoryComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Pickup:Interact] No InventoryComponent on '%s'"), *Interactor->GetName());
		return;
	}

	// bReplaceIfFull=false: nunca sobreescribir silenciosamente el ítem equipado.
	// CanInteract ya garantizó que hay espacio, así que esto solo falla
	// en condición de carrera (raro en escenarios co-op).
	if (!InventoryComponent->TryAddOrReplaceEquipped(PickupItem, false))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Pickup:Interact] TryAddOrReplaceEquipped FAILED for '%s'"), *GetName());
		return;
	}

	// Despertar el actor ANTES de cambiar bTaken para que la replicación funcione.
	// Con DORM_DormantAll, FlushNetDormancy() sola no despierta el canal de replicación.
	SetNetDormancy(DORM_Awake);

	bTaken = true;
	SetInteractionEnabled(false);
	ApplyTakenState();
	OnPickedUp(Interactor);

	// Forzar net update inmediato para que el cliente vea la desaparición sin delay.
	FlushNetDormancy();
	ForceNetUpdate();

	// Destruir el actor en el siguiente frame — la destrucción replicada es más
	// fiable que la dormancy para garantizar que los clientes eliminen el mesh.
	// Un actor destruido siempre se propaga; un actor dormante puede quedar desync.
	GetWorldTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &ATN_PickupInteractableBase::HandleDeferredDestroy));

	// ── Log de confirmación de recogida ───────────────────────────────────────
	UE_LOG(LogTemp, Log,
		TEXT("[Pickup] ✓ '%s' recogido por '%s'  |  ItemId: %s  |  UseType: %s"),
		*GetName(),
		*Interactor->GetName(),
		*PickupItem.ItemId.ToString(),
		*UEnum::GetValueAsString(PickupItem.UseType));

	Super::Interact(Interactor);
}

void ATN_PickupInteractableBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_PickupInteractableBase, bTaken);
	// PickupItem replicado para que clientes obtengan el mesh
	// en pickups spawneados dinámicamente (ball landing, drop, etc.)
	DOREPLIFETIME(ATN_PickupInteractableBase, PickupItem);
}

void ATN_PickupInteractableBase::OnRep_Taken()
{
	ApplyTakenState();
}

void ATN_PickupInteractableBase::OnRep_PickupItem()
{
	if (!Mesh || !PickupItem.EquippedMesh) { return; }

	Mesh->SetStaticMesh(PickupItem.EquippedMesh);

	// FMath::Max por componente: evita que cualquier eje sea 0
	// (ej. default antiguo era (0,1,1) → colapsaba el mesh en X)
	const FVector RS1 = PickupItem.EquippedMeshScale;
	const FVector SafeScale(FMath::Max(RS1.X, 0.01f), FMath::Max(RS1.Y, 0.01f), FMath::Max(RS1.Z, 0.01f));
	Mesh->SetRelativeScale3D(SafeScale);

	const FBoxSphereBounds LocalBounds = Mesh->CalcLocalBounds();
	const float HalfHeight = LocalBounds.BoxExtent.Z * SafeScale.Z;
	if (HalfHeight > KINDA_SMALL_NUMBER)
	{
		MeshFloorOffset = HalfHeight;
		Mesh->SetRelativeLocation(FVector(0.f, 0.f, HalfHeight));
	}

	if (PromptWidgetComponent)
	{
		const FVector InvScale(
			SafeScale.X > KINDA_SMALL_NUMBER ? 1.f / SafeScale.X : 1.f,
			SafeScale.Y > KINDA_SMALL_NUMBER ? 1.f / SafeScale.Y : 1.f,
			SafeScale.Z > KINDA_SMALL_NUMBER ? 1.f / SafeScale.Z : 1.f
		);
		PromptWidgetComponent->SetRelativeScale3D(InvScale);
	}
}

void ATN_PickupInteractableBase::ApplyTakenState()
{
	SetActorHiddenInGame(bTaken);
	SetActorEnableCollision(!bTaken);
}

void ATN_PickupInteractableBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	Super::EndPlay(EndPlayReason);
}

void ATN_PickupInteractableBase::HandleDeferredDestroy()
{
	Destroy();
}

void ATN_PickupInteractableBase::HandleRestoreDormancy()
{
	SetNetDormancy(DORM_DormantAll);
}

void ATN_PickupInteractableBase::InitializeFromInventoryItem(const FTN_InventoryItem& NewPickupItem)
{
	if (!HasAuthority() || bTaken || !NewPickupItem.IsValid()) { return; }

	PickupItem = NewPickupItem;
	SetNetDormancy(DORM_Awake);
	FlushNetDormancy();

	GetWorldTimerManager().SetTimer(DormancyTimerHandle,
		FTimerDelegate::CreateUObject(this, &ATN_PickupInteractableBase::HandleRestoreDormancy),
		3.0f, false);

	if (Mesh && PickupItem.EquippedMesh)
	{
		Mesh->SetStaticMesh(PickupItem.EquippedMesh);

		// FMath::Max por componente: evita meshes colapsados
		const FVector RS2 = PickupItem.EquippedMeshScale;
		const FVector SafeScale(FMath::Max(RS2.X, 0.01f), FMath::Max(RS2.Y, 0.01f), FMath::Max(RS2.Z, 0.01f));
		Mesh->SetRelativeScale3D(SafeScale);

		const FBoxSphereBounds LocalBounds = Mesh->CalcLocalBounds();
		const float HalfHeight = LocalBounds.BoxExtent.Z * SafeScale.Z;
		if (HalfHeight > KINDA_SMALL_NUMBER)
		{
			MeshFloorOffset = HalfHeight;
			Mesh->SetRelativeLocation(FVector(0.f, 0.f, HalfHeight));
		}

		if (PromptWidgetComponent)
		{
			const FVector InvScale(
				SafeScale.X > KINDA_SMALL_NUMBER ? 1.f / SafeScale.X : 1.f,
				SafeScale.Y > KINDA_SMALL_NUMBER ? 1.f / SafeScale.Y : 1.f,
				SafeScale.Z > KINDA_SMALL_NUMBER ? 1.f / SafeScale.Z : 1.f
			);
			PromptWidgetComponent->SetRelativeScale3D(InvScale);
		}
	}
}
