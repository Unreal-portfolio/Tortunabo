#include "Player/TN_InventoryComponent.h"
#include "Player/TN_InventoryDecisions.h"
#include "Player/TortugaCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

UTN_InventoryComponent::UTN_InventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

float UTN_InventoryComponent::GetTotalCarriedWeight() const
{
	float Total = 0.f;
	if (bHasEquippedItem) { Total += EquippedItem.ItemWeight; }
	if (bHasStoredItem)   { Total += StoredItem.ItemWeight; }
	return Total;
}

void UTN_InventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!GetOwner())
	{
		return;
	}

	EquippedVisualMesh = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("EquippedItemVisual"));
	if (!EquippedVisualMesh)
	{
		return;
	}

	EquippedVisualMesh->SetIsReplicated(false);
	EquippedVisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EquippedVisualMesh->SetCastShadow(false);
	EquippedVisualMesh->SetVisibility(false);
	EquippedVisualMesh->RegisterComponent();

	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		USceneComponent* Parent = Character->GetMesh() ? static_cast<USceneComponent*>(Character->GetMesh()) : static_cast<USceneComponent*>(Character->GetRootComponent());
		if (Parent)
		{
			if (EquippedAttachSocket != NAME_None && Character->GetMesh())
			{
				EquippedVisualMesh->AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, EquippedAttachSocket);
			}
			else
			{
				EquippedVisualMesh->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform);
				EquippedVisualMesh->SetRelativeLocationAndRotation(EquippedRelativeLocation, EquippedRelativeRotation);
			}
			VisualMeshParent = Parent;
		}
	}

	RefreshEquippedVisual();
}

void UTN_InventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UTN_InventoryComponent, EquippedItem);
	DOREPLIFETIME(UTN_InventoryComponent, bHasEquippedItem);
	DOREPLIFETIME(UTN_InventoryComponent, StoredItem);
	DOREPLIFETIME(UTN_InventoryComponent, bHasStoredItem);
}

bool UTN_InventoryComponent::TryAddItem(const FTN_InventoryItem& NewItem)
{
	if (!NewItem.IsValid())
	{
		return false;
	}

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	return AddItemInternal(NewItem);
}

bool UTN_InventoryComponent::TryAddOrReplaceEquipped(const FTN_InventoryItem& NewItem, bool bReplaceIfFull)
{
	if (!NewItem.IsValid())
	{
		return false;
	}

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	return AddOrReplaceEquippedInternal(NewItem, bReplaceIfFull);
}

bool UTN_InventoryComponent::CanReceiveItem(const FTN_InventoryItem& NewItem, bool bAllowReplaceIfFull) const
{
	if (!NewItem.IsValid())
	{
		return false;
	}

	return TNInventoryLogic::CanReceiveItem(bHasEquippedItem, bHasStoredItem, bAllowReplaceIfFull);
}

void UTN_InventoryComponent::RotateItems()
{
	if (!GetOwner())
	{
		return;
	}

	if (GetOwner()->HasAuthority())
	{
		SwapSlotsInternal();
		RefreshEquippedVisual();
	}
	else
	{
		ServerRotateItems();
	}
}

bool UTN_InventoryComponent::TryConsumeEquippedItem(FTN_InventoryItem& OutConsumedItem)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	return ConsumeEquippedInternal(OutConsumedItem);
}

bool UTN_InventoryComponent::TryExtractEquippedItem(FTN_InventoryItem& OutExtractedItem)
{
	return TryConsumeEquippedItem(OutExtractedItem);
}

bool UTN_InventoryComponent::TryConsumeItemByUseType(ETN_ItemUseType InUseType, FTN_InventoryItem& OutConsumedItem)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	using namespace TNInventoryLogic;

	const bool bEquippedMatches = bHasEquippedItem && EquippedItem.UseType == InUseType;
	const bool bStoredMatches   = bHasStoredItem && StoredItem.UseType == InUseType;

	switch (DecideConsumeByUseType(bEquippedMatches, bStoredMatches))
	{
		case EUseTypeSource::Equipped:
			if (!ConsumeEquippedInternal(OutConsumedItem))
			{
				return false;
			}
			break;

		case EUseTypeSource::Stored:
			OutConsumedItem = StoredItem;
			StoredItem      = FTN_InventoryItem();
			bHasStoredItem  = false;
			// RefreshEquippedVisual no cambia aquí (slot equipado intacto)
			break;

		default:
			return false;
	}

	if (ATortugaCharacter* Char = Cast<ATortugaCharacter>(GetOwner()))
	{
		if (Char->ConsumeSound) { Char->MulticastPlaySfx(Char->ConsumeSound); }
	}
	return true;
}

void UTN_InventoryComponent::ServerRotateItems_Implementation()
{
	SwapSlotsInternal();
	RefreshEquippedVisual();
}

void UTN_InventoryComponent::OnRep_EquippedItem()
{
	RefreshEquippedVisual();
}

void UTN_InventoryComponent::OnRep_StoredItem()
{
	// Stored slot has no world visual in this first implementation.
}

void UTN_InventoryComponent::RefreshEquippedVisual()
{
	if (!EquippedVisualMesh)
	{
		return;
	}

	if (!bHasEquippedItem || !EquippedItem.EquippedMesh)
	{
		EquippedVisualMesh->SetStaticMesh(nullptr);
		EquippedVisualMesh->SetVisibility(false);
		return;
	}

	EquippedVisualMesh->SetStaticMesh(EquippedItem.EquippedMesh);
	EquippedVisualMesh->SetVisibility(true);

	// ── Compensar escala del padre para obtener tamaño mundo correcto ─────────
	// Si el SkeletalMesh del personaje tiene escala no unitaria (ej. en el BP),
	// la bola heredaría esa escala y se deformaría. Calculamos la escala relativa
	// necesaria para que el tamaño mundo sea exactamente EquippedMeshScale.
	const FVector DesiredWorldScale = EquippedItem.EquippedMeshScale;
	if (VisualMeshParent)
	{
		const FVector ParentWorldScale = VisualMeshParent->GetComponentScale();
		const FVector RelativeScale = FVector(
			ParentWorldScale.X > KINDA_SMALL_NUMBER ? DesiredWorldScale.X / ParentWorldScale.X : DesiredWorldScale.X,
			ParentWorldScale.Y > KINDA_SMALL_NUMBER ? DesiredWorldScale.Y / ParentWorldScale.Y : DesiredWorldScale.Y,
			ParentWorldScale.Z > KINDA_SMALL_NUMBER ? DesiredWorldScale.Z / ParentWorldScale.Z : DesiredWorldScale.Z
		);
		EquippedVisualMesh->SetRelativeScale3D(RelativeScale);
	}
	else
	{
		EquippedVisualMesh->SetRelativeScale3D(DesiredWorldScale);
	}

	// Aplicar rotación del ítem (corrige orientación del asset en mano)
	EquippedVisualMesh->SetRelativeRotation(EquippedItem.EquippedMeshRotation);
}

bool UTN_InventoryComponent::AddItemInternal(const FTN_InventoryItem& NewItem)
{
	using namespace TNInventoryLogic;

	switch (DecideAddSlot(bHasEquippedItem, bHasStoredItem))
	{
		case EAddDecision::ToEquipped:
			EquippedItem = NewItem;
			bHasEquippedItem = true;
			RefreshEquippedVisual();
			break;
		case EAddDecision::ToStored:
			StoredItem = NewItem;
			bHasStoredItem = true;
			break;
		default:
			return false;
	}

	if (ATortugaCharacter* Char = Cast<ATortugaCharacter>(GetOwner()))
	{
		if (Char->PickupSound) { Char->MulticastPlaySfx(Char->PickupSound); }
	}
	return true;
}

bool UTN_InventoryComponent::AddOrReplaceEquippedInternal(const FTN_InventoryItem& NewItem, bool bReplaceIfFull)
{
	using namespace TNInventoryLogic;

	const EAddDecision Decision = DecideAddOrReplace(bHasEquippedItem, bHasStoredItem, bReplaceIfFull);
	if (Decision == EAddDecision::ToEquipped || Decision == EAddDecision::ToStored)
	{
		// La ocupación no cambia entre las dos decisiones → AddItemInternal elige el mismo slot.
		return AddItemInternal(NewItem);
	}

	if (Decision != EAddDecision::ReplaceEquipped)
	{
		return false;
	}

	EquippedItem = NewItem;
	bHasEquippedItem = true;
	RefreshEquippedVisual();
	if (ATortugaCharacter* Char = Cast<ATortugaCharacter>(GetOwner()))
	{
		if (Char->PickupSound) { Char->MulticastPlaySfx(Char->PickupSound); }
	}
	return true;
}

bool UTN_InventoryComponent::ConsumeEquippedInternal(FTN_InventoryItem& OutItem)
{
	using namespace TNInventoryLogic;

	const EConsumeDecision Decision = DecideConsumeEquipped(bHasEquippedItem, bHasStoredItem);
	if (Decision == EConsumeDecision::Rejected)
	{
		return false;
	}

	OutItem = EquippedItem;

	if (Decision == EConsumeDecision::ConsumeAndPromoteStored)
	{
		EquippedItem = StoredItem;
		bHasEquippedItem = true;
		StoredItem = FTN_InventoryItem();
		bHasStoredItem = false;
	}
	else
	{
		EquippedItem = FTN_InventoryItem();
		bHasEquippedItem = false;
	}

	RefreshEquippedVisual();
	return true;
}

void UTN_InventoryComponent::SwapSlotsInternal()
{
	if (!TNInventoryLogic::ShouldSwapSlots(bHasEquippedItem, bHasStoredItem))
	{
		return;
	}

	Swap(EquippedItem, StoredItem);
	Swap(bHasEquippedItem, bHasStoredItem);
}

