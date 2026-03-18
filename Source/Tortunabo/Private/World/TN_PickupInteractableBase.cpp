#include "World/TN_PickupInteractableBase.h"
#include "Player/TN_InventoryComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

ATN_PickupInteractableBase::ATN_PickupInteractableBase()
{
	PromptText = FText::FromString(TEXT("Recoger"));
}

bool ATN_PickupInteractableBase::CanInteract(APawn* Interactor) const
{
	if (!Super::CanInteract(Interactor) || bTaken || !Interactor)
	{
		return false;
	}

	const UTN_InventoryComponent* InventoryComponent = Interactor->FindComponentByClass<UTN_InventoryComponent>();
	if (!InventoryComponent)
	{
		return false;
	}

	return InventoryComponent->CanReceiveItem(PickupItem, true);
}

void ATN_PickupInteractableBase::Interact(APawn* Interactor)
{
	if (!HasAuthority() || !CanInteract(Interactor) || !Interactor)
	{
		return;
	}

	UTN_InventoryComponent* InventoryComponent = Interactor->FindComponentByClass<UTN_InventoryComponent>();
	if (!InventoryComponent || !InventoryComponent->TryAddOrReplaceEquipped(PickupItem, true))
	{
		return;
	}

	bTaken = true;
	SetInteractionEnabled(false);
	ApplyTakenState();
	OnPickedUp(Interactor);
	Super::Interact(Interactor);
}

void ATN_PickupInteractableBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_PickupInteractableBase, bTaken);
}

void ATN_PickupInteractableBase::OnRep_Taken()
{
	ApplyTakenState();
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
	if (Mesh)
	{
		Mesh->SetStaticMesh(PickupItem.EquippedMesh);
	}
}

