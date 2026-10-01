// ATN_Buggy: carrocería de tortuga y pintura (#297, #114). El aspecto sale del PlayerState de la conductora
// (EquippedBuggyLook, validado y replicado por el servidor), así que cada máquina pinta lo mismo sin RPC propias.

#include "Vehicles/TN_Buggy.h"
#include "Vehicles/TN_BuggyCosmetics.h"
#include "Vehicles/TN_BuggyLookComponent.h"
#include "Core/TN_CoopPlayerState.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerState.h"

void ATN_Buggy::RefreshBuggyLook(bool bForce)
{
	if (GetNetMode() == NM_DedicatedServer || !BuggyLook)
	{
		return;
	}
	if (!BuggyLook->HasBuiltLook())
	{
		// Primera vez: las ruedas de los huesos y el cañón de la torreta pasan a ser los del modelo, y el buggy escucha los
		// cambios de aspecto de los PlayerState (el de su conductora lo repinta al momento).
		TArray<UStaticMeshComponent*> WheelParts;
		for (UStaticMeshComponent* Tire : Tires) { WheelParts.Add(Tire); }
		BuggyLook->UseExternalWheels(WheelParts);
		BuggyLook->UseExternalCannon(TurretBarrel);
		ATN_CoopPlayerState::OnAnyBuggyLookChanged.AddWeakLambda(this, [this](const ATN_CoopPlayerState* Changed) { NotifyDriverLookChanged(Changed); });
	}
	// Los pilotos IA también tienen PlayerState (para la tabla de puestos): llevan el buggy de su equipo.
	const ATN_CoopPlayerState* Driver = bDriverSeated ? Cast<ATN_CoopPlayerState>(DriverPlayerState) : nullptr;
	const FTN_BuggyLook Look = Driver && !Driver->IsABot() ? Driver->EquippedBuggyLook : TNBuggyCosmetics::LookForTeam(TeamIndex);
	BuggyLook->SetGunnerSeated(bGunnerSeated);
	BuggyLook->ApplyLook(Look, TeamIndex, bForce);
	if (BuggyLook->HasBuiltLook() && Body && Body->IsVisible())
	{
		// La carrocería prestada de HellYeah se queda solo como respaldo.
		Body->SetVisibility(false);
	}
}

void ATN_Buggy::NotifyDriverLookChanged(const APlayerState* ChangedPlayerState)
{
	if (ChangedPlayerState && ChangedPlayerState == DriverPlayerState)
	{
		RefreshBuggyLook(false);
	}
}
