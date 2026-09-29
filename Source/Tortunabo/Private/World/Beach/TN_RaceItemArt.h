#pragma once

#include "CoreMinimal.h"
#include "World/Beach/TN_RaceItems.h"

class UStaticMesh;
class UTexture2D;

/**
 * Arte de los objetos de carrera dibujado en código (sin assets): la malla con la que se ven en el suelo y en las aletas y
 * el icono de la mochila. Todo se construye una vez por ejecución y queda en caché (fuera del recolector). Implementado en
 * TN_RaceItemArt.cpp.
 *
 * Las mallas están hechas al tamaño con que se ven (cm): entre 20 y 40 cm de lado, con el pivote en el CENTRO de la malla
 * (ATN_PickupInteractableBase la apoya en el suelo suponiendo que el origen está en su centro; en la mano se recentra sola)
 * y mirando a +X. Los actores del objeto (la mina lanzada, el disco...) pueden
 * usar la misma malla, escalada, para que en vuelo se vea lo que se cogió.
 */
namespace TNRaceItemArt
{
	/** Cómo se ve un objeto en el suelo y en la mano de la tortuga. */
	struct FHeldLook
	{
		UStaticMesh* Mesh = nullptr;
		/** Escala y giro con los que se pone (EquippedMeshScale y EquippedMeshRotation de la fila). */
		FVector Scale = FVector::OneVector;
		FRotator Rotation = FRotator::ZeroRotator;
	};

	/** La malla del objeto (None y Count no tienen). false si no se ha podido construir (servidor dedicado, sin material...). */
	bool GetHeldLook(ETNRaceItem Kind, FHeldLook& OutLook);

	/** El icono de la mochila (128x128, estilo pegatina del HUD), o null en servidor dedicado o sin motor gráfico. */
	UTexture2D* GetIcon(ETNRaceItem Kind);
}
