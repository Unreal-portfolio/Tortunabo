#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "TN_InputSetupSubsystem.generated.h"

class UInputMappingContext;
class UInputAction;

/**
 * Subsystem to ensure Enhanced Input assets (IMC and InputActions) exist at runtime.
 * If assets don't exist in Content/Input/, this subsystem will create them programmatically.
 */
UCLASS()
class TORTUNABO_API UTN_InputSetupSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	// Paths for input assets
	static constexpr const TCHAR* INPUT_IMC_PATH = TEXT("/Game/Input/IMC_Player");
	static constexpr const TCHAR* INPUT_IA_MOVE_PATH = TEXT("/Game/Input/IA_Move");
	static constexpr const TCHAR* INPUT_IA_LOOK_PATH = TEXT("/Game/Input/IA_Look");
	static constexpr const TCHAR* INPUT_IA_JUMP_PATH = TEXT("/Game/Input/IA_Jump");
	static constexpr const TCHAR* INPUT_IA_INTERACT_PATH = TEXT("/Game/Input/IA_Interact");
	static constexpr const TCHAR* INPUT_IA_ROTATE_INV_PATH = TEXT("/Game/Input/IA_RotateInventory");

	void EnsureInputAssetsExist();
	UInputMappingContext* EnsureInputMappingContext();
	void EnsureInputActionsExist(UInputMappingContext* IMC);

	UInputAction* LoadOrCreateInputAction(const FString& AssetPath, FString ActionName);
};

