#pragma once

#include "CoreMinimal.h"

/**
 * Lógica pura del cocinado: si un asset que el código carga por ruta (LoadObject, LoadClass, FObjectFinder,
 * FSoftObjectPath) entra en el .pak según la configuración de empaquetado de Config/DefaultGame.ini.
 * La usa el test Tortunabo.Cook.StringPathsAreCooked, que recorre las rutas literales de Source/.
 */
namespace TNCookLogic
{
	/** @brief Paquete de una ruta de objeto: «/Game/A/B.B_C» → «/Game/A/B». */
	inline FString ToPackageName(const FString& ObjectPath)
	{
		FString Package = ObjectPath;
		int32 Dot = INDEX_NONE;
		if (Package.FindChar(TEXT('.'), Dot))
		{
			Package.LeftInline(Dot);
		}
		return Package;
	}

	/** @brief true si PackageName está dentro de la carpeta Dir («/Game/UI» cubre «/Game/UI/HUD/X», no «/Game/UIX»). */
	inline bool IsUnderDirectory(const FString& PackageName, const FString& Dir)
	{
		FString Normalized = Dir;
		Normalized.RemoveFromEnd(TEXT("/"));
		return PackageName.StartsWith(Normalized + TEXT("/"), ESearchCase::IgnoreCase);
	}

	/**
	 * @brief Decide si la ruta se cocina: está en una carpeta de DirectoriesToAlwaysCook o es uno de MapsToCook, y no está
	 * en una carpeta de DirectoriesToNeverCook.
	 */
	inline bool IsCooked(const FString& ObjectPath, const TArray<FString>& AlwaysCookDirs, const TArray<FString>& MapsToCook,
		const TArray<FString>& NeverCookDirs)
	{
		const FString Package = ToPackageName(ObjectPath);
		for (const FString& Dir : NeverCookDirs)
		{
			if (IsUnderDirectory(Package, Dir))
			{
				return false;
			}
		}
		for (const FString& Map : MapsToCook)
		{
			if (Package.Equals(ToPackageName(Map), ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		for (const FString& Dir : AlwaysCookDirs)
		{
			if (IsUnderDirectory(Package, Dir))
			{
				return true;
			}
		}
		return false;
	}
}
