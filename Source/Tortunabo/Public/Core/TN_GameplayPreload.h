#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TN_GameplayPreload.generated.h"

class UDataTable;
class UMaterialInterface;

/**
 * Lo que se usa en pleno juego ya cargado (Docs/Calidad-Codigo-2026-09-29.md M12): antes, el primer cofre, la primera
 * búsqueda, el premio del lagarto, la caja de objetos, el botín suelto y las tarjetas de la tienda hacían un LoadObject
 * síncrono y daban un tirón. Ahora se piden aquí, que solo resuelve lo que UTN_GameplayPreloadSubsystem precargó al
 * arrancar; si algo no estuviera cargado, se carga igual (para no quedarse sin objeto) y queda un aviso en el registro.
 */
namespace TNPreload
{
	/** Catálogo de objetos (DT_Items): cofres, búsquedas, botín suelto, lagarto y cajas de la carrera. */
	TORTUNABO_API const UDataTable* ItemCatalog();

	/** Material de las vistas previas en la interfaz (tienda, probador, campeona de la carrera). */
	TORTUNABO_API UMaterialInterface* PreviewMaterial();
}

/**
 * @brief Precarga y retiene al crearse la GameInstance (en el arranque, no en partida) los recursos de TNPreload.
 */
UCLASS()
class TORTUNABO_API UTN_GameplayPreloadSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

private:
	/** Retenidos mientras viva la GameInstance: el recolector no los descarga entre mapas. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> Retained;
};
