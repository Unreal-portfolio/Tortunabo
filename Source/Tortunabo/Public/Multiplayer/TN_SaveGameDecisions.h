#pragma once

#include "CoreMinimal.h"
#include "Misc/DateTime.h"

/**
 * Lógica pura de los guardados locales (cosméticos y tutorial): qué hacer al
 * cargar, cómo se llama la copia de un guardado corrupto y qué migración toca.
 * Sin UGameplayStatics ni disco, para que Tortunabo.SaveGame.* cubra las reglas
 * que usa TNSaveGameIO en producción.
 */
namespace TNSaveLogic
{
	/** Versión actual del perfil cosmético. 0 = guardado anterior al campo SaveVersion. */
	constexpr int32 COSMETIC_SAVE_VERSION = 1;

	/** Versión actual del estado del tutorial. 0 = guardado anterior al campo SaveVersion. */
	constexpr int32 TUTORIAL_SAVE_VERSION = 1;

	/** Qué hacer con una ranura al cargarla. */
	enum class ELoadAction : uint8
	{
		/** No hay fichero: perfil nuevo. */
		CreateFresh,
		/** El fichero se ha leído bien: usarlo. */
		UseLoaded,
		/** Hay fichero pero no se puede leer: apartarlo con otro nombre y empezar de cero. Nunca sobrescribirlo. */
		QuarantineAndCreateFresh
	};

	/** Qué migración aplicar a un guardado leído. */
	enum class EMigration : uint8
	{
		UpToDate,
		/** Versión antigua: se migra en memoria y se sella con la actual. */
		Upgrade,
		/** Guardado por una build más nueva: se usa tal cual y se avisa en el log. */
		FromNewerBuild
	};

	/** @brief Decide la acción de carga a partir de si el fichero existe y si se ha leído con la clase esperada. */
	inline ELoadAction DecideLoadAction(bool bFileExists, bool bLoadedOk)
	{
		if (!bFileExists)
		{
			return ELoadAction::CreateFresh;
		}
		return bLoadedOk ? ELoadAction::UseLoaded : ELoadAction::QuarantineAndCreateFresh;
	}

	/**
	 * @brief Un guardado con versión (>= 1) lleva SaveVersion como primera propiedad y bWriteComplete = true como
	 * última: si se ha leído la versión pero no la marca de fin, el fichero está truncado. Los de versión 0
	 * (anteriores al campo) no llevan marca y no se pueden comprobar.
	 */
	inline bool IsTruncated(int32 SavedVersion, bool bWriteComplete)
	{
		return SavedVersion >= 1 && !bWriteComplete;
	}

	/** @brief Decide la migración de un guardado con versión SavedVersion frente a la actual. */
	inline EMigration DecideMigration(int32 SavedVersion, int32 CurrentVersion)
	{
		if (SavedVersion == CurrentVersion)
		{
			return EMigration::UpToDate;
		}
		return SavedVersion < CurrentVersion ? EMigration::Upgrade : EMigration::FromNewerBuild;
	}

	/** @brief Nombre de la ranura a la que se aparta un guardado ilegible: <Slot>_corrupto_AAAAMMDD-HHMMSS. */
	inline FString BuildQuarantineSlotName(const FString& Slot, const FDateTime& When)
	{
		return FString::Printf(TEXT("%s_corrupto_%s"), *Slot, *When.ToString(TEXT("%Y%m%d-%H%M%S")));
	}

	/** @brief Copia de una lista de IDs sin NAME_None ni duplicados, en el orden original (migración v0 → v1). */
	inline TArray<FName> SanitizeIds(const TArray<FName>& Ids)
	{
		TArray<FName> Result;
		Result.Reserve(Ids.Num());
		for (const FName Id : Ids)
		{
			if (Id != NAME_None)
			{
				Result.AddUnique(Id);
			}
		}
		return Result;
	}
}
