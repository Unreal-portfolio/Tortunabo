#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TN_ArtSettings.generated.h"

class UTN_ArtCatalog;

/**
 * Catálogos de arte que usa el juego (Ajustes del proyecto > Tortunavy > Arte; Config/DefaultGame.ini,
 * [/Script/Tortunabo.TN_ArtSettings]). Cada pieza generada desde C++ busca su sustituto en todos ellos (Docs/Arte_Assets.md).
 * /Game/Art se cocina entero (DirectoriesToAlwaysCook): todas las máquinas resuelven lo mismo.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Arte"))
class TORTUNABO_API UTN_ArtSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UTN_ArtSettings();

	/**
	 * Catálogos por zona: DA_Arte_Lobby (Lobby.*) y DA_Arte_ProcMap (ProcMap.* y Beach.*, los dos mapas procedurales). Si una
	 * pieza sale en dos, gana el primero.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Catálogos")
	TArray<TSoftObjectPtr<UTN_ArtCatalog>> Catalogs;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
