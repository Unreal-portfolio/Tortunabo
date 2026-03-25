#include "World/TN_SlowZoneVolume.h"
#include "Components/BoxComponent.h"
#include "Player/TortugaCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

ATN_SlowZoneVolume::ATN_SlowZoneVolume()
{
	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	SetRootComponent(TriggerBox);

	TriggerBox->SetBoxExtent(FVector(200.f, 200.f, 100.f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ATN_SlowZoneVolume::OnBoxBeginOverlap);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &ATN_SlowZoneVolume::OnBoxEndOverlap);
}

void ATN_SlowZoneVolume::OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ATortugaCharacter* Char = Cast<ATortugaCharacter>(OtherActor);
	if (!Char || OriginalSpeeds.Contains(Char))
	{
		return;
	}

	UCharacterMovementComponent* CMC = Char->GetCharacterMovement();
	if (!CMC)
	{
		return;
	}

	OriginalSpeeds.Add(Char, CMC->MaxWalkSpeed);
	CMC->MaxWalkSpeed = MaxSlowSpeed;
}

void ATN_SlowZoneVolume::OnBoxEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ATortugaCharacter* Char = Cast<ATortugaCharacter>(OtherActor);
	if (!Char)
	{
		return;
	}

	float* Original = OriginalSpeeds.Find(Char);
	if (!Original)
	{
		return;
	}

	if (UCharacterMovementComponent* CMC = Char->GetCharacterMovement())
	{
		CMC->MaxWalkSpeed = *Original;
	}

	OriginalSpeeds.Remove(Char);
}
