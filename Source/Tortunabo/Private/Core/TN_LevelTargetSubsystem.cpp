#include "Core/TN_LevelTargetSubsystem.h"
#include "Core/TN_Log.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UTN_LevelTargetSubsystem* UTN_LevelTargetSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject) { return nullptr; }
	UWorld* World = WorldContextObject->GetWorld();
	return World ? World->GetSubsystem<UTN_LevelTargetSubsystem>() : nullptr;
}

void UTN_LevelTargetSubsystem::RegisterTarget(FName Tag, AActor* Actor)
{
	if (Tag.IsNone() || !Actor) { return; }

	// Si ya hay otro registrado, avisar — colisión de tags es un design smell.
	if (TWeakObjectPtr<AActor>* Existing = RegisteredTargets.Find(Tag))
	{
		if (Existing->IsValid() && Existing->Get() != Actor)
		{
			UE_LOG(LogTortunabo, Warning,
				TEXT("[LevelTargetSubsystem] Tag '%s' already registered by '%s' — overwriting with '%s'."),
				*Tag.ToString(),
				*GetNameSafe(Existing->Get()),
				*GetNameSafe(Actor));
		}
	}

	RegisteredTargets.Add(Tag, TWeakObjectPtr<AActor>(Actor));

	UE_LOG(LogTortunabo, Log, TEXT("[LevelTargetSubsystem] Registered '%s' → '%s' (total=%d)"),
		*Tag.ToString(), *GetNameSafe(Actor), RegisteredTargets.Num());
}

void UTN_LevelTargetSubsystem::UnregisterTarget(FName Tag)
{
	if (Tag.IsNone()) { return; }
	if (RegisteredTargets.Remove(Tag) > 0)
	{
		UE_LOG(LogTortunabo, Log, TEXT("[LevelTargetSubsystem] Unregistered '%s' (remaining=%d)"),
			*Tag.ToString(), RegisteredTargets.Num());
	}
}

AActor* UTN_LevelTargetSubsystem::FindTarget(FName Tag) const
{
	if (Tag.IsNone()) { return nullptr; }
	const TWeakObjectPtr<AActor>* Found = RegisteredTargets.Find(Tag);
	return (Found && Found->IsValid()) ? Found->Get() : nullptr;
}

void UTN_LevelTargetSubsystem::Deinitialize()
{
	RegisteredTargets.Empty();
	Super::Deinitialize();
}
