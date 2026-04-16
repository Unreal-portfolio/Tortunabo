#include "World/TN_UmbrellaInteractable.h"
#include "Player/TortugaCharacter.h"
#include "TimerManager.h"

ATN_UmbrellaInteractable::ATN_UmbrellaInteractable()
{
	bReplicates = true;
	PromptText = FText::FromString(TEXT("Abrir sombrilla"));
	CooldownSeconds = ReuseDelaySecs;
}

void ATN_UmbrellaInteractable::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(UmbrellaActiveTimerHandle);

	// Si destruido mientras la protección está activa, limpiar
	if (ATortugaCharacter* Char = ProtectedCharacter.Get())
	{
		Char->SetUmbrellaProtection(false);
	}

	Super::EndPlay(EndPlayReason);
}

void ATN_UmbrellaInteractable::Interact(APawn* Interactor)
{
	if (!HasAuthority() || !Interactor) { return; }

	ATortugaCharacter* Char = Cast<ATortugaCharacter>(Interactor);
	if (!Char) { return; }

	// Actualizar cooldown de la base
	LastInteractionServerTime = GetWorld()->GetTimeSeconds();
	CooldownSeconds = ReuseDelaySecs;

	// Activar protección
	Char->SetUmbrellaProtection(true);
	ProtectedCharacter = Char;

	UE_LOG(LogTemp, Log, TEXT("[Umbrella] %s abrió sombrilla (%.1fs)"), *GetNameSafe(Char), UmbrellaDurationSeconds);

	MulticastOnUmbrellaOpened(Interactor);

	// Timer para cerrar la sombrilla — CreateUObject, no lambda
	FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &ATN_UmbrellaInteractable::HandleUmbrellaExpired);
	GetWorldTimerManager().SetTimer(UmbrellaActiveTimerHandle, Del, UmbrellaDurationSeconds, false);
}

void ATN_UmbrellaInteractable::HandleUmbrellaExpired()
{
	if (ATortugaCharacter* Char = ProtectedCharacter.Get())
	{
		Char->SetUmbrellaProtection(false);
	}
	ProtectedCharacter.Reset();
	MulticastOnUmbrellaClosed();
}

void ATN_UmbrellaInteractable::MulticastOnUmbrellaOpened_Implementation(APawn* User)
{
	OnUmbrellaOpened(User);
}

void ATN_UmbrellaInteractable::MulticastOnUmbrellaClosed_Implementation()
{
	OnUmbrellaClosed();
}
