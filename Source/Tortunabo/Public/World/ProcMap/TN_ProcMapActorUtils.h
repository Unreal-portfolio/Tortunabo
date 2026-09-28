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

	/** true si el material expone alguno de los parámetros de color que usa Tint. */
	inline bool HasTintParameter(const UMaterialInterface* Material)
	{
		FLinearColor Value;
		return Material && (Material->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("Color")), Value)
			|| Material->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("BaseColor")), Value));
	}

	/**
	 * Tinte greybox: instancia dinámica del material 0 con los parámetros de color habituales.
	 * Las formas básicas del motor traen DefaultMaterial o WorldGridMaterial, sin parámetros:
	 * si el material no se puede teñir y bAllowFallback, se cambia por BasicShapeMaterial.
	 */
	inline void Tint(UStaticMeshComponent* Comp, const FLinearColor& Color, bool bAllowFallback = true)
	{
		if (!Comp) { return; }
		if (bAllowFallback && !HasTintParameter(Comp->GetMaterial(0)))
		{
			if (UMaterialInterface* Tintable = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
			{
				Comp->SetMaterial(0, Tintable);
			}
		}
		if (!Comp->GetMaterial(0)) { return; }
		if (UMaterialInstanceDynamic* MID = Comp->CreateAndSetMaterialInstanceDynamic(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
			MID->SetVectorParameterValue(TEXT("BaseColor"), Color);
		}
	}
}
