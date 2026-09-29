#pragma once

#include "CoreMinimal.h"
#include "World/TN_PickupInteractableBase.h"
#include "TN_RaceItemBox.generated.h"

/**
 * Caja de objetos de la carrera de la playa (el «?» de las carreras de karts): un cubo de juguete de colores que flota y
 * gira en el suelo con la marca dorada de siempre de lo que se coge. Se recoge como cualquier objeto (E), pero no da un
 * objeto fijo: al cogerla se sortea uno según el puesto de quien la coge (TNRaceItems::RollLoot con la fuente Box), así que
 * a las de atrás les tocan la bala y el protector solar, y a las de delante lo que se lanza y lo defensivo
 * (Docs/Modo_Carrera.md, «Pesos por posición»). Si la mochila está llena no se puede coger y se queda donde está.
 *
 * La fila del pickup (Race_Box) viene en el valor por defecto de la clase: no se replica, y cada máquina le pone la malla en
 * BeginPlay (ATN_PickupInteractableBase). El sorteo lo hace el servidor al interactuar. Las reparte
 * UTN_BeachLootSubsystem como objetos sueltos (tres filas de lado a lado de la playa y unos veinte más por el recorrido).
 */
UCLASS()
class TORTUNABO_API ATN_RaceItemBox : public ATN_PickupInteractableBase
{
	GENERATED_BODY()

public:
	ATN_RaceItemBox();

	virtual void Interact(APawn* Interactor) override;
};
