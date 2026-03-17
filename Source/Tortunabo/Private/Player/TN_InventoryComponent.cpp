#include "Player/TN_InventoryComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

UTN_InventoryComponent::UTN_InventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UTN_InventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!GetOwner())
	{
		return;
	}

	// Use proper factory for NewObject with valid naming context in UE 5.6
	EquippedVisualMesh = NewObject<UStaticMeshComponent>(GetOwner(), UStaticMeshComponent::StaticClass(), FName(TEXT("EquippedItemVisual")));
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
}

bool UTN_InventoryComponent::AddItemInternal(const FTN_InventoryItem& NewItem)
{
	if (!bHasEquippedItem)
	{
		EquippedItem = NewItem;
		bHasEquippedItem = true;
		RefreshEquippedVisual();
		return true;
	}

	if (!bHasStoredItem)
	{
		StoredItem = NewItem;
		bHasStoredItem = true;
		return true;
	}

	return false;
}

void UTN_InventoryComponent::SwapSlotsInternal()
{
	if (!bHasEquippedItem && !bHasStoredItem)
	{
		return;
	}

	Swap(EquippedItem, StoredItem);
	Swap(bHasEquippedItem, bHasStoredItem);
}

