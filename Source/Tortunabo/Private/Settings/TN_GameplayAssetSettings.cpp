#include "Settings/TN_GameplayAssetSettings.h"

#include "Core/TN_Log.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "World/TN_ScorePickup.h"

void UTN_GameplayAssetSettings::PreloadAsync()
{
	UTN_GameplayAssetSettings* Settings = GetMutableDefault<UTN_GameplayAssetSettings>();
	if (!Settings || Settings->PreloadHandle.IsValid() || Settings->ScorePickupClass.IsNull())
	{
		return;
	}
	if (!UAssetManager::IsInitialized())
	{
		return;
	}
	TWeakObjectPtr<UTN_GameplayAssetSettings> WeakSettings(Settings);
	Settings->PreloadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Settings->ScorePickupClass.ToSoftObjectPath(),
		FStreamableDelegate::CreateLambda([WeakSettings]()
		{
			if (UTN_GameplayAssetSettings* Loaded = WeakSettings.Get())
			{
				if (UClass* Class = Loaded->ScorePickupClass.Get())
				{
					Loaded->ResolvedScorePickupClass = Class;
				}
			}
		}),
		FStreamableManager::AsyncLoadHighPriority);
}

UClass* UTN_GameplayAssetSettings::GetScorePickupClass()
{
	UTN_GameplayAssetSettings* Settings = GetMutableDefault<UTN_GameplayAssetSettings>();
	if (!Settings)
	{
		return ATN_ScorePickup::StaticClass();
	}
	if (UClass* Resolved = Settings->ResolvedScorePickupClass.Get())
	{
		return Resolved;
	}
	if (Settings->bReportedMissingScorePickup)
	{
		return ATN_ScorePickup::StaticClass();
	}
	// Sin precarga (tests, editor) o aún en vuelo: se carga ahora y el handle la retiene.
	if (!Settings->ScorePickupClass.IsNull() && UAssetManager::IsInitialized())
	{
		Settings->PreloadHandle = UAssetManager::GetStreamableManager().RequestSyncLoad(
			Settings->ScorePickupClass.ToSoftObjectPath());
	}
	if (UClass* Class = Settings->ScorePickupClass.LoadSynchronous())
	{
		Settings->ResolvedScorePickupClass = Class;
		return Class;
	}
	if (!Settings->bReportedMissingScorePickup)
	{
		Settings->bReportedMissingScorePickup = true;
		UE_LOG(LogTortunabo, Error,
			TEXT("[Assets] No existe la concha de puntos '%s' (ScorePickupClass en [/Script/Tortunabo.TN_GameplayAssetSettings]). "
				"Se usa la clase nativa, sin el aspecto del Blueprint."),
			*Settings->ScorePickupClass.ToString());
	}
	return ATN_ScorePickup::StaticClass();
}

bool UTN_GameplayAssetSettings::IsScorePickupClassRetained()
{
	const UTN_GameplayAssetSettings* Settings = GetDefault<UTN_GameplayAssetSettings>();
	if (!Settings || !Settings->PreloadHandle.IsValid())
	{
		return false;
	}
	const UClass* Resolved = Settings->ResolvedScorePickupClass.Get();
	return Resolved && Settings->PreloadHandle->GetLoadedAsset() == Resolved;
}
