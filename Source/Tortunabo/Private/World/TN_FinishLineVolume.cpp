#include "World/TN_FinishLineVolume.h"
#include "Game/TN_RunGameMode.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

ATN_FinishLineVolume::ATN_FinishLineVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	SetRootComponent(TriggerBox);
	TriggerBox->SetBoxExtent(FVector(200.f, 200.f, 200.f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);
	TriggerBox->SetHiddenInGame(true);

#if WITH_EDITORONLY_DATA
	TriggerBox->ShapeColor = FColor::Green;
	TriggerBox->SetLineThickness(2.f);
#endif

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ATN_FinishLineVolume::OnBoxBeginOverlap);
}

void ATN_FinishLineVolume::OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || !OtherActor)
	{
		return;
	}

	if (const APawn* Pawn = Cast<APawn>(OtherActor))
	{
		if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
		{
			if (ATN_RunGameMode* RunGM = ResolveRunGameMode())
			{
				RunGM->MarkPlayerFinished(PC);
			}
		}
	}
}

ATN_RunGameMode* ATN_FinishLineVolume::ResolveRunGameMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<ATN_RunGameMode>() : nullptr;
}

