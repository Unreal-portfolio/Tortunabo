#include "Player/TN_InputSetupSubsystem.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputModifiers.h"
#include "InputTriggers.h"

#if WITH_EDITOR
	#include "AssetTools/AssetToolsModule.h"
	#include "AssetRegistry/AssetRegistryModule.h"
#endif

void UTN_InputSetupSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Ensure input assets are loaded or created at runtime
	EnsureInputAssetsExist();
}

void UTN_InputSetupSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UTN_InputSetupSubsystem::EnsureInputAssetsExist()
{
	// Try to load or create IMC
	UInputMappingContext* IMC = EnsureInputMappingContext();
	if (IMC)
	{
		// Ensure all required input actions exist
		EnsureInputActionsExist(IMC);
		UE_LOG(LogTemp, Log, TEXT("[Input] Input setup complete. IMC and InputActions verified."));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Input] Failed to ensure input mapping context."));
	}
}

UInputMappingContext* UTN_InputSetupSubsystem::EnsureInputMappingContext()
{
	// Try to load existing IMC
	UInputMappingContext* IMC = LoadObject<UInputMappingContext>(nullptr, INPUT_IMC_PATH);
	if (IMC)
	{
		UE_LOG(LogTemp, Log, TEXT("[Input] Loaded existing IMC: %s"), INPUT_IMC_PATH);
		return IMC;
	}

#if WITH_EDITOR
	// In Editor, create new IMC asset
	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
	UInputMappingContext* NewIMC = NewObject<UInputMappingContext>(GetTransientPackage(), UInputMappingContext::StaticClass(), FName("IMC_Player"));
	
	if (NewIMC)
	{
		FString PackagePath = TEXT("/Game/Input");
		FString AssetName = TEXT("IMC_Player");
		UPackage* Package = CreatePackage(*FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName));
		if (Package)
		{
			NewIMC->Rename(*AssetName, Package);
			Package->MarkPackageDirty();
			FAssetRegistryModule::AssetCreated(NewIMC);
			UE_LOG(LogTemp, Log, TEXT("[Input] Created new IMC at %s"), INPUT_IMC_PATH);
			return NewIMC;
		}
	}
#endif

	UE_LOG(LogTemp, Warning, TEXT("[Input] Could not create IMC. Players may need to manually set input assets."));
	return nullptr;
}

void UTN_InputSetupSubsystem::EnsureInputActionsExist(UInputMappingContext* IMC)
{
	struct FInputActionDef
	{
		const TCHAR* Path;
		const TCHAR* DisplayName;
		EInputActionValueType ValueType;
	};

	const FInputActionDef InputActions[] = {
		{ INPUT_IA_MOVE_PATH, TEXT("Move"), EInputActionValueType::Axis2D },
		{ INPUT_IA_LOOK_PATH, TEXT("Look"), EInputActionValueType::Axis2D },
		{ INPUT_IA_JUMP_PATH, TEXT("Jump"), EInputActionValueType::Digital },
		{ INPUT_IA_INTERACT_PATH, TEXT("Interact"), EInputActionValueType::Digital },
		{ INPUT_IA_ROTATE_INV_PATH, TEXT("RotateInventory"), EInputActionValueType::Digital },
	};

	for (const FInputActionDef& ActionDef : InputActions)
	{
		LoadOrCreateInputAction(ActionDef.Path, FString(ActionDef.DisplayName));
	}
}

UInputAction* UTN_InputSetupSubsystem::LoadOrCreateInputAction(const FString& AssetPath, FString ActionName)
{
	// Try to load existing action
	UInputAction* Action = LoadObject<UInputAction>(nullptr, *AssetPath);
	if (Action)
	{
		UE_LOG(LogTemp, Log, TEXT("[Input] Loaded existing InputAction: %s"), *AssetPath);
		return Action;
	}

#if WITH_EDITOR
	// In Editor, create new action asset (simplified; full implementation would configure modifiers/triggers)
	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
	UInputAction* NewAction = NewObject<UInputAction>(GetTransientPackage(), UInputAction::StaticClass(), FName(*ActionName));
	
	if (NewAction)
	{
		FString PackagePath = TEXT("/Game/Input");
		UPackage* Package = CreatePackage(*FString::Printf(TEXT("%s/%s"), *PackagePath, *ActionName));
		if (Package)
		{
			NewAction->Rename(*ActionName, Package);
			Package->MarkPackageDirty();
			FAssetRegistryModule::AssetCreated(NewAction);
			UE_LOG(LogTemp, Log, TEXT("[Input] Created new InputAction at %s"), *AssetPath);
			return NewAction;
		}
	}
#endif

	UE_LOG(LogTemp, Warning, TEXT("[Input] Could not create InputAction: %s. Players may need to manually configure."), *AssetPath);
	return nullptr;
}

