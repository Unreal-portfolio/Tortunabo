#include "World/TN_UmbrellaInteractable.h"
#include "Player/TortugaCharacter.h"
#include "TimerManager.h"

ATN_UmbrellaInteractable::ATN_UmbrellaInteractable()
{
	bReplicates = true;
	PromptText = FText::FromString(TEXT("Abrir sombrilla"));
	CooldownSeconds = ReuseDelaySecs;
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

	UE_LOG(LogTemp, Log, TEXT("[Umbrella] %s abrió sombrilla (%.1fs)"), *GetNameSafe(Char), UmbrellaDurationSeconds);

	MulticastOnUmbrellaOpened(Interactor);

	// Timer para cerrar la sombrilla
	TWeakObjectPtr<ATortugaCharacter> WeakChar(Char);
	TWeakObjectPtr<ATN_UmbrellaInteractable> WeakSelf(this);

	GetWorldTimerManager().SetTimer(UmbrellaActiveTimerHandle,
		[WeakChar, WeakSelf]()
		{
			if (WeakChar.IsValid())
			{
				WeakChar->SetUmbrellaProtection(false);
			}
			if (WeakSelf.IsValid())
			{
				WeakSelf->MulticastOnUmbrellaClosed();
			}
		}, UmbrellaDurationSeconds, false);
}

void ATN_UmbrellaInteractable::MulticastOnUmbrellaOpened_Implementation(APawn* User)
{
	OnUmbrellaOpened(User);
}

void ATN_UmbrellaInteractable::MulticastOnUmbrellaClosed_Implementation()
{
	OnUmbrellaClosed();
}
