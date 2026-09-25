#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

/**
 * Utilidades compartidas por los actores del mapa procedural.
 *
 * Muchos de estos actores se crean LOCALMENTE en todas las máquinas (no replican)
 * para que el movimiento predicho del cliente vea lo mismo que el servidor
 * (géiseres, toboganes, corrientes...). En un actor local HasAuthority() es true
 * también en los clientes, así que las decisiones "solo servidor" (matar, puntuar)
 * usan IsServerWorld() en su lugar.
 */
namespace TNProcActors
{
	inline bool IsServerWorld(const UObject* Context)
	{
		const UWorld* World = Context ? Context->GetWorld() : nullptr;
		return World && World->GetNetMode() != NM_Client;
	}

	/**
	 * true en las máquinas que simulan el movimiento de este personaje: el servidor
	 * y el cliente que lo controla. Los proxies simulados no aplican impulsos.
	 */
	inline bool SimulatesMovement(const APawn* Pawn)
	{
		return Pawn && (Pawn->IsLocallyControlled() || Pawn->HasAuthority());
	}

	/** Tinte greybox: instancia dinámica del material 0 con los parámetros de color habituales. */
	inline void Tint(UStaticMeshComponent* Comp, const FLinearColor& Color)
	{
		if (!Comp || !Comp->GetMaterial(0)) { return; }
		if (UMaterialInstanceDynamic* MID = Comp->CreateAndSetMaterialInstanceDynamic(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
			MID->SetVectorParameterValue(TEXT("BaseColor"), Color);
		}
	}
}
