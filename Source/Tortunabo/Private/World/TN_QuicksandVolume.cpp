#include "World/TN_QuicksandVolume.h"
#include "Player/TortugaCharacter.h"
#include "Player/TN_StaminaComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ATN_QuicksandVolume::ATN_QuicksandVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false; // Lógica local en cada máquina — CMC predice igual en servidor y cliente

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	SetRootComponent(TriggerBox);

	SandMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SandMesh"));
	SandMesh->SetupAttachment(TriggerBox);
	SandMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	TriggerBox->SetBoxExtent(FVector(300.f, 300.f, 100.f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void ATN_QuicksandVolume::BeginPlay()
{
	Super::BeginPlay();

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ATN_QuicksandVolume::OnBoxBeginOverlap);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &ATN_QuicksandVolume::OnBoxEndOverlap);
}

void ATN_QuicksandVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ActiveChars.Empty();
	Super::EndPlay(EndPlayReason);
}

// ── Overlap ───────────────────────────────────────────────────────────────────

void ATN_QuicksandVolume::OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	ATortugaCharacter* Char = Cast<ATortugaCharacter>(OtherActor);
	if (!IsValid(Char)) { return; }

	FSandState State;

	if (UTN_StaminaComponent* Stamina = Char->FindComponentByClass<UTN_StaminaComponent>())
	{
		Stamina->SetSpeedCap(SlowSpeed);
	}

	// Gravedad + salto solo en la máquina autoritativa para este pawn (servidor o dueño).
	// Modificar el CMC de pawns remotos en clientes corrompería el estado si los
	// overlaps llegan desordenados o el actor se destruye antes del EndOverlap.
	const bool bShouldModifyCMC = HasAuthority() || Char->IsLocallyControlled();
	if (bShouldModifyCMC)
	{
		if (UCharacterMovementComponent* CMC = Char->GetCharacterMovement())
		{
			State.OrigGravityScale = CMC->GravityScale;
			State.OrigJumpZVel     = CMC->JumpZVelocity;

			CMC->GravityScale  = GravityScaleOverride;
			CMC->JumpZVelocity = JumpZVelocityOverride;
		}
	}

	Char->OnDestroyed.AddUniqueDynamic(this, &ATN_QuicksandVolume::OnCharacterDestroyed);
	ActiveChars.FindOrAdd(TWeakObjectPtr<ATortugaCharacter>(Char)) = State;
}

void ATN_QuicksandVolume::OnBoxEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ATortugaCharacter* Char = Cast<ATortugaCharacter>(OtherActor);
	if (!IsValid(Char)) { return; }

	if (UTN_StaminaComponent* Stamina = Char->FindComponentByClass<UTN_StaminaComponent>())
	{
		Stamina->ClearSpeedCap();
	}

	if (HasAuthority() || Char->IsLocallyControlled())
	{
		if (UCharacterMovementComponent* CMC = Char->GetCharacterMovement())
		{
			if (const FSandState* State = ActiveChars.Find(TWeakObjectPtr<ATortugaCharacter>(Char)))
			{
				CMC->GravityScale  = State->OrigGravityScale;
				CMC->JumpZVelocity = State->OrigJumpZVel;
			}
		}
	}

	Char->OnDestroyed.RemoveDynamic(this, &ATN_QuicksandVolume::OnCharacterDestroyed);
	ActiveChars.Remove(TWeakObjectPtr<ATortugaCharacter>(Char));
}

void ATN_QuicksandVolume::OnCharacterDestroyed(AActor* DestroyedActor)
{
	ATortugaCharacter* Char = Cast<ATortugaCharacter>(DestroyedActor);
	if (!Char) { return; }

	// CMC ya no existe — solo limpiamos la entrada
	ActiveChars.Remove(TWeakObjectPtr<ATortugaCharacter>(Char));
}
