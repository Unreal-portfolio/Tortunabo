#include "Vehicles/TN_BuggyWheel.h"

namespace
{
	/**
	 * Geometría y suspensión comunes: parten del Offroad de TP_VehicleAdv con ajuste arcade (amortiguación 0,25,
	 * recorrido 25 cm y freno de 6000 N·m), como en HellYeah.
	 */
	void ApplySharedWheelSetup(UChaosVehicleWheel& Wheel)
	{
		Wheel.WheelRadius = 51.f;
		Wheel.WheelWidth = 35.f;
		Wheel.CorneringStiffness = 750.f;
		Wheel.SuspensionMaxRaise = 25.f;
		Wheel.SuspensionMaxDrop = 25.f;
		Wheel.SuspensionDampingRatio = 0.25f;
		Wheel.WheelLoadRatio = 1.f;
		Wheel.SpringRate = 100.f;
		Wheel.SpringPreload = 100.f;
		Wheel.SweepShape = ESweepShape::Shapecast;
		Wheel.MaxBrakeTorque = 6000.f;
	}
}

UTN_BuggyWheelFront::UTN_BuggyWheelFront()
{
	ApplySharedWheelSetup(*this);
	AxleType = EAxleType::Front;
	bAffectedBySteering = true;
	MaxSteerAngle = 40.f;
	FrictionForceMultiplier = 3.f;
}

UTN_BuggyWheelRear::UTN_BuggyWheelRear()
{
	ApplySharedWheelSetup(*this);
	AxleType = EAxleType::Rear;
	bAffectedByHandbrake = true;
	bAffectedByEngine = true;
	MaxHandBrakeTorque = 6000.f;
	FrictionForceMultiplier = 2.f;
}
