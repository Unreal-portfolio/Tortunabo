#include "Vehicles/TN_BuggyInput.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

namespace
{
	UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, FName(Name), RF_Transient);
		Action->ValueType = Type;
		return Action;
	}

	void MapNegated(UInputMappingContext* Context, const UInputAction* Action, const FKey& Key)
	{
		FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
		Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
	}

	void MapDeadZone(UInputMappingContext* Context, const UInputAction* Action, const FKey& Key)
	{
		FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
		Mapping.Modifiers.Add(NewObject<UInputModifierDeadZone>(Context));
	}

	UEnhancedInputLocalPlayerSubsystem* SubsystemOf(const APlayerController* PC)
	{
		if (!PC || !PC->IsLocalController())
		{
			return nullptr;
		}
		return ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	}
}

UTN_BuggyInputSet* UTN_BuggyInputSet::Create(UObject* Outer)
{
	UTN_BuggyInputSet* Set = NewObject<UTN_BuggyInputSet>(Outer);
	Set->Throttle = MakeAction(Set, TEXT("IA_BuggyThrottle"), EInputActionValueType::Axis1D);
	Set->Brake = MakeAction(Set, TEXT("IA_BuggyBrake"), EInputActionValueType::Axis1D);
	Set->Steer = MakeAction(Set, TEXT("IA_BuggySteer"), EInputActionValueType::Axis1D);
	Set->Handbrake = MakeAction(Set, TEXT("IA_BuggyHandbrake"), EInputActionValueType::Boolean);
	Set->FireBack = MakeAction(Set, TEXT("IA_BuggyFireBack"), EInputActionValueType::Boolean);
	Set->SelfRight = MakeAction(Set, TEXT("IA_BuggySelfRight"), EInputActionValueType::Boolean);
	Set->FireCoco = MakeAction(Set, TEXT("IA_BuggyFireCoco"), EInputActionValueType::Boolean);
	Set->FireSpecial = MakeAction(Set, TEXT("IA_BuggyFireSpecial"), EInputActionValueType::Boolean);
	Set->AimMouse = MakeAction(Set, TEXT("IA_BuggyAimMouse"), EInputActionValueType::Axis2D);
	Set->AimStick = MakeAction(Set, TEXT("IA_BuggyAimStick"), EInputActionValueType::Axis2D);

	UInputMappingContext* Driver = NewObject<UInputMappingContext>(Set, TEXT("IMC_BuggyDriver"), RF_Transient);
	Driver->MapKey(Set->Throttle, EKeys::W);
	Driver->MapKey(Set->Throttle, EKeys::Gamepad_RightTriggerAxis);
	Driver->MapKey(Set->Brake, EKeys::S);
	Driver->MapKey(Set->Brake, EKeys::Gamepad_LeftTriggerAxis);
	Driver->MapKey(Set->Steer, EKeys::D);
	MapNegated(Driver, Set->Steer, EKeys::A);
	MapDeadZone(Driver, Set->Steer, EKeys::Gamepad_LeftX);
	Driver->MapKey(Set->Handbrake, EKeys::SpaceBar);
	Driver->MapKey(Set->Handbrake, EKeys::Gamepad_FaceButton_Right);
	Driver->MapKey(Set->SelfRight, EKeys::R);
	Driver->MapKey(Set->SelfRight, EKeys::Gamepad_FaceButton_Top);
	Driver->MapKey(Set->FireCoco, EKeys::LeftMouseButton);
	Driver->MapKey(Set->FireCoco, EKeys::Gamepad_RightShoulder);
	Driver->MapKey(Set->FireSpecial, EKeys::RightMouseButton);
	Driver->MapKey(Set->FireSpecial, EKeys::Gamepad_LeftShoulder);
	Driver->MapKey(Set->FireBack, EKeys::Q);
	Driver->MapKey(Set->FireBack, EKeys::Gamepad_FaceButton_Left);
	Set->DriverContext = Driver;

	UInputMappingContext* Gunner = NewObject<UInputMappingContext>(Set, TEXT("IMC_BuggyGunner"), RF_Transient);
	Gunner->MapKey(Set->AimMouse, EKeys::Mouse2D);
	MapDeadZone(Gunner, Set->AimStick, EKeys::Gamepad_Right2D);
	Gunner->MapKey(Set->FireCoco, EKeys::LeftMouseButton);
	Gunner->MapKey(Set->FireCoco, EKeys::Gamepad_RightTriggerAxis);
	Gunner->MapKey(Set->FireSpecial, EKeys::RightMouseButton);
	Gunner->MapKey(Set->FireSpecial, EKeys::Gamepad_LeftTriggerAxis);
	Gunner->MapKey(Set->SelfRight, EKeys::R);
	Gunner->MapKey(Set->SelfRight, EKeys::Gamepad_FaceButton_Top);
	Set->GunnerContext = Gunner;
	return Set;
}

void UTN_BuggyInputSet::AddContext(const APlayerController* PC, const UInputMappingContext* Context)
{
	UEnhancedInputLocalPlayerSubsystem* Subsystem = SubsystemOf(PC);
	if (Subsystem && Context)
	{
		Subsystem->AddMappingContext(Context, ContextPriority);
	}
}

void UTN_BuggyInputSet::RemoveContext(const APlayerController* PC, const UInputMappingContext* Context)
{
	UEnhancedInputLocalPlayerSubsystem* Subsystem = SubsystemOf(PC);
	if (Subsystem && Context)
	{
		Subsystem->RemoveMappingContext(Context);
	}
}
