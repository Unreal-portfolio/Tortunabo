#include "World/TN_SlowZoneVolume.h"
#include "Player/TortugaCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

ATN_SlowZoneVolume::ATN_SlowZoneVolume()
{
	OnActorBeginOverlap.AddDynamic(this, &ATN_SlowZoneVolume::OnZoneBeginOverlap);
	OnActorEndOverlap.AddDynamic(this, &ATN_SlowZoneVolume::OnZoneEndOverlap);
}

void ATN_SlowZoneVolume::OnZoneBeginOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	if (!HasAuthority() || !OtherActor)
	{
		return;
	}

	ATortugaCharacter* Char = Cast<ATortugaCharacter>(OtherActor);
	if (!Char)
	{
		return;
	}

	UCharacterMovementComponent* CMC = Char->GetCharacterMovement();
	if (!CMC)
	{
		return;
	}

	// Solo aplicar si aún no está registrado (evitar doble-slow)
	if (OriginalSpeeds.Contains(Char))
	{
		return;
	}

	const float Original = CMC->MaxWalkSpeed;
	OriginalSpeeds.Add(Char, Original);
	CMC->MaxWalkSpeed = Original * SlowMultiplier;
}

void ATN_SlowZoneVolume::OnZoneEndOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	if (!HasAuthority() || !OtherActor)
	{
		return;
	}

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
