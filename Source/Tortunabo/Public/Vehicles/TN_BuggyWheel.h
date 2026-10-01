// Ruedas del buggy sobre el esqueleto SKM_Offroad (port de UHYBuggyWheelFront/Rear de HellYeah): radio 51 cm y ancho
// 35 cm, suspensión blanda y con rebote, tracción trasera; el derrape largo es del freno de mano.
#pragma once

#include "CoreMinimal.h"
#include "ChaosVehicleWheel.h"
#include "TN_BuggyWheel.generated.h"

UCLASS()
class TORTUNABO_API UTN_BuggyWheelFront : public UChaosVehicleWheel
{
	GENERATED_BODY()

public:
	UTN_BuggyWheelFront();
};

/** Trasera: motriz y con freno de mano. */
UCLASS()
class TORTUNABO_API UTN_BuggyWheelRear : public UChaosVehicleWheel
{
	GENERATED_BODY()

public:
	UTN_BuggyWheelRear();
};
