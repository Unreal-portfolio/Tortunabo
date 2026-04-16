#include "World/TN_CollectionZone.h"
#include "Player/TortugaCharacter.h"
#include "Player/TN_InventoryComponent.h"
#include "Core/TN_InventoryTypes.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Pawn.h"

ATN_CollectionZone::ATN_CollectionZone()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;

	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	SetRootComponent(BaseMesh);
	BaseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	DepositBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DepositBox"));
	DepositBox->SetupAttachment(BaseMesh);
	DepositBox->SetBoxExtent(FVector(100.f, 100.f, 100.f));
	DepositBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DepositBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	DepositBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void ATN_CollectionZone::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		DepositBox->OnComponentBeginOverlap.AddDynamic(this, &ATN_CollectionZone::OnBoxBeginOverlap);
	}
}

void ATN_CollectionZone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_CollectionZone, DepositedCount);
}

// ── Overlap ────────────────────────────────────────────────────────────────────

void ATN_CollectionZone::OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || bGoalReached) { return; }

	ATortugaCharacter* Char = Cast<ATortugaCharacter>(OtherActor);
	if (!Char || !Char->GetController()) { return; }

	if (!CheckPlayerHasItem(Char)) { return; }

	ConsumeItemFromPlayer(Char);
	++DepositedCount;

	UE_LOG(LogTemp, Log, TEXT("[CollectionZone] Deposited by %s → %d/%d"),
		*GetNameSafe(Char), DepositedCount, RequiredCount);

	MulticastOnDeposit(Char, DepositedCount, RequiredCount);

	if (DepositedCount >= RequiredCount)
	{
		bGoalReached = true;
		ApplyTargetTransforms();
		MulticastOnGoalReached();
	}
}

// ── Helpers ────────────────────────────────────────────────────────────────────

bool ATN_CollectionZone::CheckPlayerHasItem(ATortugaCharacter* Char) const
{
	if (!Char) { return false; }

	UTN_InventoryComponent* Inv = Char->FindComponentByClass<UTN_InventoryComponent>();
	if (!Inv) { return false; }

	const FTN_InventoryItem Equipped = Inv->GetEquippedItem();
	if (Equipped.ItemId == NAME_None) { return false; }

	// NAME_None = acepta cualquier ítem
	if (CollectibleTag == NAME_None) { return true; }

	return Equipped.ItemId == CollectibleTag;
}

void ATN_CollectionZone::ConsumeItemFromPlayer(ATortugaCharacter* Char)
{
	if (!Char) { return; }

	UTN_InventoryComponent* Inv = Char->FindComponentByClass<UTN_InventoryComponent>();
	if (!Inv) { return; }

	FTN_InventoryItem Consumed;
	Inv->TryConsumeEquippedItem(Consumed);
}

void ATN_CollectionZone::ApplyTargetTransforms()
{
	for (AActor* Target : TargetActors)
	{
		if (!Target) { continue; }
		Target->SetActorLocation(Target->GetActorLocation() + ActivationLocationOffset);
		Target->SetActorRotation(Target->GetActorRotation() + ActivationRotationOffset);
	}
}

// ── OnRep ──────────────────────────────────────────────────────────────────────

void ATN_CollectionZone::OnRep_DepositedCount()
{
	// El visual de progreso (HUD) se actualiza via Blueprint binding.
}

// ── Multicast ──────────────────────────────────────────────────────────────────

void ATN_CollectionZone::MulticastOnDeposit_Implementation(APawn* Depositor, int32 NewCount, int32 Needed)
{
	OnItemDeposited(Depositor, NewCount, Needed);
}

void ATN_CollectionZone::MulticastOnGoalReached_Implementation()
{
	OnGoalReached();
}
