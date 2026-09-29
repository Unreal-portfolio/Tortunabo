#include "Multiplayer/TN_SaveGameIO.h"

#include "Core/TN_Log.h"
#include "GameFramework/SaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Multiplayer/TN_SaveGameDecisions.h"
#include "PlatformFeatures.h"
#include "SaveGameSystem.h"

namespace
{
	ISaveGameSystem* TNGetSaveSystem()
	{
		return IPlatformFeaturesModule::Get().GetSaveGameSystem();
	}

	/** Copia los bytes crudos a la ranura de cuarentena y borra el original. Devuelve si el original ya no corre peligro. */
	bool TNQuarantine(ISaveGameSystem& SaveSystem, const FString& Slot, int32 UserIndex, const TArray<uint8>& RawBytes,
		const TCHAR* What)
	{
		if (RawBytes.Num() == 0)
		{
			// Una copia vacía no protege nada: mejor no borrar el original.
			UE_LOG(LogTortunabo, Error, TEXT("[SaveGame] %s: '%s' vacío o ilegible; no se aparta ni se escribirá encima en esta sesión."),
				What, *Slot);
			return false;
		}
		const FString Quarantine = TNSaveLogic::BuildQuarantineSlotName(Slot, FDateTime::Now());
		if (!SaveSystem.SaveGame(false, *Quarantine, UserIndex, RawBytes))
		{
			UE_LOG(LogTortunabo, Error,
				TEXT("[SaveGame] %s: '%s' no se puede leer ni apartar a '%s'. No se escribirá encima en esta sesión."),
				What, *Slot, *Quarantine);
			return false;
		}
		if (!SaveSystem.DeleteGame(false, *Slot, UserIndex))
		{
			// La copia está a salvo: sobrescribir el original ya no pierde nada.
			UE_LOG(LogTortunabo, Warning, TEXT("[SaveGame] %s: copiado a '%s', pero '%s' no se ha podido borrar."),
				What, *Quarantine, *Slot);
		}
		UE_LOG(LogTortunabo, Warning,
			TEXT("[SaveGame] %s: '%s' estaba dañado (%d bytes). Apartado como '%s'; se empieza de cero."),
			What, *Slot, RawBytes.Num(), *Quarantine);
		return true;
	}
}

namespace TNSaveGameIO
{
	FLoadResult LoadOrQuarantine(const FString& Slot, int32 UserIndex, const UClass* ExpectedClass,
		TFunctionRef<bool(const USaveGame&)> IsIntact, const TCHAR* What)
	{
		FLoadResult Result;
		ISaveGameSystem* SaveSystem = TNGetSaveSystem();
		if (!SaveSystem)
		{
			UE_LOG(LogTortunabo, Error, TEXT("[SaveGame] %s: no hay sistema de guardado en esta plataforma."), What);
			Result.bSaveBlocked = true;
			return Result;
		}

		const bool bExists = SaveSystem->DoesSaveGameExist(*Slot, UserIndex);
		TArray<uint8> RawBytes;
		USaveGame* Loaded = nullptr;
		const bool bBytesRead = bExists && SaveSystem->LoadGame(false, *Slot, UserIndex, RawBytes);
		if (bBytesRead)
		{
			Loaded = UGameplayStatics::LoadGameFromMemory(RawBytes);
		}
		const bool bLoadedOk = Loaded && ExpectedClass && Loaded->IsA(ExpectedClass) && IsIntact(*Loaded);

		switch (TNSaveLogic::DecideLoadAction(bExists, bBytesRead, bLoadedOk))
		{
		case TNSaveLogic::ELoadAction::KeepAndBlockSaves:
			UE_LOG(LogTortunabo, Error,
				TEXT("[SaveGame] %s: '%s' existe pero no se ha podido leer; no se toca en esta sesión."), What, *Slot);
			Result.bSaveBlocked = true;
			break;
		case TNSaveLogic::ELoadAction::UseLoaded:
			Result.Loaded = Loaded;
			break;
		case TNSaveLogic::ELoadAction::QuarantineAndCreateFresh:
			Result.bQuarantined = TNQuarantine(*SaveSystem, Slot, UserIndex, RawBytes, What);
			Result.bSaveBlocked = !Result.bQuarantined;
			break;
		case TNSaveLogic::ELoadAction::CreateFresh:
			break;
		}
		return Result;
	}

	bool SaveChecked(USaveGame* Save, const FString& Slot, int32 UserIndex, const TCHAR* What)
	{
		if (!Save)
		{
			return false;
		}
		if (UGameplayStatics::SaveGameToSlot(Save, Slot, UserIndex))
		{
			return true;
		}
		UE_LOG(LogTortunabo, Warning, TEXT("[SaveGame] %s: SaveGameToSlot('%s') ha fallado; reintentando."), What, *Slot);
		if (UGameplayStatics::SaveGameToSlot(Save, Slot, UserIndex))
		{
			return true;
		}
		UE_LOG(LogTortunabo, Error, TEXT("[SaveGame] %s: no se ha podido guardar '%s' (disco lleno o sin permisos)."),
			What, *Slot);
		return false;
	}
}
