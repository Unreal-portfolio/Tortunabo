#include "World/TN_SlowZoneVolume.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/TortugaCharacter.h"
#include "Player/TN_StaminaComponent.h"

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
	if (!Char || CharactersInZone.Contains(Char))
	{
		return;
	}

	UTN_StaminaComponent* StaminaComp = Char->FindComponentByClass<UTN_StaminaComponent>();
	if (!StaminaComp)
	{
		return;
	}

	CharactersInZone.Add(Char);
	StaminaComp->SetSpeedCap(MaxSlowSpeed);

	// Registrar callback de destrucción para limpiar si el personaje muere dentro
	Char->OnDestroyed.AddUniqueDynamic(this, &ATN_SlowZoneVolume::OnCharacterDestroyed);

	if (bSlowFall)
	{
		if (UCharacterMovementComponent* CMC = Char->GetCharacterMovement())
		{
			OriginalGravityScales.Add(Char, CMC->GravityScale);
			CMC->GravityScale = GravityScaleInZone;
		}
	}
}

void ATN_SlowZoneVolume::OnBoxEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ATortugaCharacter* Char = Cast<ATortugaCharacter>(OtherActor);
	if (!Char || !CharactersInZone.Contains(Char))
	{
		return;
	}

	CharactersInZone.Remove(Char);

	UTN_StaminaComponent* StaminaComp = Char->FindComponentByClass<UTN_StaminaComponent>();
	if (StaminaComp)
	{
		StaminaComp->ClearSpeedCap();
	}

	if (bSlowFall)
	{
		if (UCharacterMovementComponent* CMC = Char->GetCharacterMovement())
		{
			if (const float* Original = OriginalGravityScales.Find(Char))
			{
				CMC->GravityScale = *Original;
			}
		}
		OriginalGravityScales.Remove(Char);
	}

	Char->OnDestroyed.RemoveDynamic(this, &ATN_SlowZoneVolume::OnCharacterDestroyed);
}

void ATN_SlowZoneVolume::OnCharacterDestroyed(AActor* DestroyedActor)
{
	ATortugaCharacter* Char = Cast<ATortugaCharacter>(DestroyedActor);
	if (!Char)
	{
		return;
	}

	// El personaje se destruyó dentro de la zona — limpiar sin restaurar GravityScale
	// (el CMC ya no existe; restaurar aquí causaría crash)
	CharactersInZone.Remove(Char);
	OriginalGravityScales.Remove(Char);
}
