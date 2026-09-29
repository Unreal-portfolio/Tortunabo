#include "World/Beach/TN_RaceItemBox.h"
#include "World/Beach/TN_RaceItemComponent.h"
#include "World/Beach/TN_RaceItems.h"
#include "Core/TN_Log.h"
#include "GameFramework/Character.h"

ATN_RaceItemBox::ATN_RaceItemBox()
{
	// La fila del pickup es la de la caja; al cogerla se cambia por el objeto sorteado (Interact).
	PickupItem = TNRaceItems::MakeItem(ETNRaceItem::Box);
	PromptText = NSLOCTEXT("TNRace", "ItemBoxPrompt", "Coger caja");
}

void ATN_RaceItemBox::Interact(APawn* Interactor)
{
	if (!HasAuthority() || !Interactor || !CanInteract(Interactor))
	{
		return;
	}
	// El objeto depende del puesto de quien la coge: los de atrás, lo que hace remontar; los de delante, lo defensivo.
	FTN_InventoryItem Rolled;
	if (!TNRaceItems::RollLoot(Interactor, ETNRaceLootSource::Box, TNRaceItems::LoadCatalog(), Rolled))
	{
		UE_LOG(LogTortunabo, Warning, TEXT("[Carrera] La caja %s no ha podido sortear ningún objeto."), *GetName());
		return;
	}
	PickupItem = Rolled;
	Super::Interact(Interactor);
	if (!bTaken)
	{
		// No se ha podido coger: vuelve a ser una caja (si no, la siguiente tortuga vería y comprobaría lo sorteado).
		PickupItem = TNRaceItems::MakeItem(ETNRaceItem::Box);
		TNRaceItems::ResolveVisuals(PickupItem);
		return;
	}
	UE_LOG(LogTortunabo, Log, TEXT("[Carrera] %s coge una caja de objetos: %s."), *GetNameSafe(Interactor), *Rolled.ItemId.ToString());

	if (ACharacter* Turtle = Cast<ACharacter>(Interactor))
	{
		if (UTN_RaceItemComponent* Comp = UTN_RaceItemComponent::FindOrAddOn(Turtle))
		{
			Comp->MulticastCue(ETNRaceSound::BoxOpen, 1.f);
		}
	}
}
