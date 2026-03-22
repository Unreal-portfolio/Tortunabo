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

	const bool bCanReceive = InventoryComponent->CanReceiveItem(PickupItem, true);
	if (!bCanReceive)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[Pickup:CanInteract] Inventory FULL for '%s'"), *Interactor->GetName());
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

	if (!InventoryComponent->TryAddOrReplaceEquipped(PickupItem, true))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Pickup:Interact] TryAddOrReplaceEquipped FAILED for '%s'"), *GetName());
		return;
	}

	bTaken = true;
	SetInteractionEnabled(false);
	ApplyTakenState();
	OnPickedUp(Interactor);

	// Despertar al actor dormido para que bTaken se replique inmediatamente
	FlushNetDormancy();

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
	if (!Mesh || !PickupItem.EquippedMesh)
	{
		return;
	}

	Mesh->SetStaticMesh(PickupItem.EquippedMesh);

	// Zero-check: rows del DataTable guardados antes de añadir EquippedMeshScale
	// se zero-inicializan. Fallback a OneVector para evitar colapsar el mesh.
	const FVector SafeScale = PickupItem.EquippedMeshScale.IsNearlyZero()
		? FVector::OneVector
		: PickupItem.EquippedMeshScale;
	Mesh->SetRelativeScale3D(SafeScale);

	// Auto-offset de suelo: el mesh del pickup tiene el pivote en el centro → la mitad
	// clipa en el suelo. Calculamos el semiancho Z desde los bounds del mesh.
	const FBoxSphereBounds LocalBounds = Mesh->CalcLocalBounds();
	const float HalfHeight = LocalBounds.BoxExtent.Z * SafeScale.Z;
	if (HalfHeight > KINDA_SMALL_NUMBER)
	{
		MeshFloorOffset = HalfHeight;
		Mesh->SetRelativeLocation(FVector(0.f, 0.f, HalfHeight));
	}

	// El PromptWidget está adjunto al Mesh (root). Si el mesh es pequeño (ej. 0.25),
	// el prompt heredaría esa escala y sería ilegible. Escala inversa → tamaño mundo fijo.
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

void ATN_PickupInteractableBase::InitializeFromInventoryItem(const FTN_InventoryItem& NewPickupItem)
{
	if (!HasAuthority() || bTaken || !NewPickupItem.IsValid())
	{
		return;
	}

	PickupItem = NewPickupItem;

	// Forzar al actor a estar completamente despierto para que TODAS las
	// propiedades se repliquen a los clientes (incluyendo PickupItem y el mesh).
	// DormantAll en actores dinámicos puede impedir la replicación inicial.
	SetNetDormancy(DORM_Awake);
	FlushNetDormancy();

	// Volver a DormantAll tras 3 segundos para no generar tráfico innecesario
	FTimerHandle DormancyTimerHandle;
	GetWorldTimerManager().SetTimer(DormancyTimerHandle, [WeakThis = TWeakObjectPtr<ATN_PickupInteractableBase>(this)]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->SetNetDormancy(DORM_DormantAll);
		}
	}, 3.0f, false);

	if (Mesh && PickupItem.EquippedMesh)
	{
		Mesh->SetStaticMesh(PickupItem.EquippedMesh);

		const FVector SafeScale = PickupItem.EquippedMeshScale.IsNearlyZero()
			? FVector::OneVector
			: PickupItem.EquippedMeshScale;
		Mesh->SetRelativeScale3D(SafeScale);

		// Auto-offset de suelo: pivote en centro del mesh → la mitad clipa en suelo.
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
