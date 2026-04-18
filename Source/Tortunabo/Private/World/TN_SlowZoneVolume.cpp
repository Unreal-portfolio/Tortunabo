#include "World/TN_SlowZoneVolume.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/TortugaCharacter.h"
#include "Player/TN_StaminaComponent.h"

ATN_SlowZoneVolume::ATN_SlowZoneVolume()
{
	PrimaryActorTick.bCanEverTick = true;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	SetRootComponent(TriggerBox);

	TriggerBox->SetBoxExtent(FVector(200.f, 200.f, 100.f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void ATN_SlowZoneVolume::BeginPlay()
{
	Super::BeginPlay();

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ATN_SlowZoneVolume::OnBoxBeginOverlap);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &ATN_SlowZoneVolume::OnBoxEndOverlap);
}

void ATN_SlowZoneVolume::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	for (const TWeakObjectPtr<ATortugaCharacter>& WeakChar : CharactersInZone)
	{
		ATortugaCharacter* Char = WeakChar.Get();
		if (!Char) { continue; }

		// Solo modificar CMC en servidor o pawn local (evita corromper clientes remotos).
		if (!HasAuthority() && !Char->IsLocallyControlled()) { continue; }

		UCharacterMovementComponent* CMC = Char->GetCharacterMovement();
		if (!CMC) { continue; }

		// ── Clamp horizontal (en vuelo) ──────────────────────────────────────────
		if (CMC->IsFalling())
		{
			FVector HorizVel = FVector(CMC->Velocity.X, CMC->Velocity.Y, 0.f);
			if (HorizVel.Size() > MaxSlowSpeed)
			{
				HorizVel = HorizVel.GetSafeNormal() * MaxSlowSpeed;
				CMC->Velocity.X = HorizVel.X;
				CMC->Velocity.Y = HorizVel.Y;
			}
		}

		// ── Clamp vertical (efecto sirope): apenas sube, baja lento ─────────────
		if (CMC->Velocity.Z > MaxUpwardVelocity)
		{
			CMC->Velocity.Z = MaxUpwardVelocity;
		}
		else if (CMC->Velocity.Z < -MaxFallVelocity)
		{
			CMC->Velocity.Z = -MaxFallVelocity;
		}
	}
}

void ATN_SlowZoneVolume::OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ATortugaCharacter* Char = Cast<ATortugaCharacter>(OtherActor);
	if (!Char || CharactersInZone.Contains(Char)) { return; }

	UTN_StaminaComponent* StaminaComp = Char->FindComponentByClass<UTN_StaminaComponent>();
	if (!StaminaComp) { return; }

	CharactersInZone.Add(Char);
	StaminaComp->SetSpeedCap(MaxSlowSpeed);
	Char->OnDestroyed.AddUniqueDynamic(this, &ATN_SlowZoneVolume::OnCharacterDestroyed);

	// Guardar y reducir GravityScale + JumpZVelocity (sirope)
	if (HasAuthority() || Char->IsLocallyControlled())
	{
		if (UCharacterMovementComponent* CMC = Char->GetCharacterMovement())
		{
			FSyrupState State;
			State.OrigGravityScale = CMC->GravityScale;
			State.OrigJumpZVel     = CMC->JumpZVelocity;
			OriginalCMCState.Add(Char, State);

			CMC->GravityScale  = GravityScaleInZone;
			CMC->JumpZVelocity = JumpVelocityInZone;
		}
	}
}

void ATN_SlowZoneVolume::OnBoxEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ATortugaCharacter* Char = Cast<ATortugaCharacter>(OtherActor);
	if (!Char || !CharactersInZone.Contains(Char)) { return; }

	CharactersInZone.Remove(Char);
	Char->OnDestroyed.RemoveDynamic(this, &ATN_SlowZoneVolume::OnCharacterDestroyed);

	if (UTN_StaminaComponent* Stamina = Char->FindComponentByClass<UTN_StaminaComponent>())
	{
		Stamina->ClearSpeedCap();
	}

	// Restaurar CMC solo si no está en otra SlowZone
	bool bStillInSlowZone = false;
	TArray<AActor*> Overlapping;
	Char->GetOverlappingActors(Overlapping, ATN_SlowZoneVolume::StaticClass());
	for (AActor* OA : Overlapping)
	{
		if (OA && OA != this) { bStillInSlowZone = true; break; }
	}

	if (!bStillInSlowZone && (HasAuthority() || Char->IsLocallyControlled()))
	{
		if (UCharacterMovementComponent* CMC = Char->GetCharacterMovement())
		{
			if (const FSyrupState* State = OriginalCMCState.Find(Char))
			{
				CMC->GravityScale  = State->OrigGravityScale;
				CMC->JumpZVelocity = State->OrigJumpZVel;
			}
		}
	}
	OriginalCMCState.Remove(Char);
}

void ATN_SlowZoneVolume::OnCharacterDestroyed(AActor* DestroyedActor)
{
	ATortugaCharacter* Char = Cast<ATortugaCharacter>(DestroyedActor);
	if (!Char) { return; }

	CharactersInZone.Remove(Char);
	OriginalCMCState.Remove(Char);
}
