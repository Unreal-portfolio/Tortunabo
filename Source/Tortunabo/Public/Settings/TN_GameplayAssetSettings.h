#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Templates/SubclassOf.h"
#include "TN_GameplayAssetSettings.generated.h"

class ATN_ScorePickup;
struct FStreamableHandle;

/**
 * @brief Recursos de juego que el código necesita sin que cuelguen de un actor del mapa: una sola fuente, editable en
 * Config/DefaultGame.ini ([/Script/Tortunabo.TN_GameplayAssetSettings]) o en Ajustes del proyecto > Tortunavy.
 *
 * La concha de puntos la sueltan cofres, fortalezas, lagartos, el botín de ronda y el mapa procedural. Se precarga de
 * forma asíncrona al arrancar el juego (UMP_GameInstance::Init) y queda retenida; si el Blueprint no existe, el log lo
 * dice con un Error y se usa la clase nativa. El test Tortunabo.Assets.ScorePickupClass falla si la ruta se rompe.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Tortunavy - Recursos de juego"))
class TORTUNABO_API UTN_GameplayAssetSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Blueprint de la concha de puntos (BP_ScorePickup). */
	UPROPERTY(Config, EditAnywhere, Category = "Puntuación")
	TSoftClassPtr<ATN_ScorePickup> ScorePickupClass;

	/** @brief Empieza a cargar en segundo plano los recursos de esta lista y los retiene. Idempotente. */
	static void PreloadAsync();

	/**
	 * @brief Clase de la concha de puntos, nunca nula: la del Blueprint (ya precargada o, si aún no, cargada ahora) o,
	 * si no existe, la nativa ATN_ScorePickup con un Error en el log (una vez por sesión).
	 */
	static UClass* GetScorePickupClass();

private:
	/** Clase resuelta y retenida (el CDO de los ajustes vive toda la sesión). */
	UPROPERTY(Transient)
	TSubclassOf<ATN_ScorePickup> ResolvedScorePickupClass;

	TSharedPtr<FStreamableHandle> PreloadHandle;

	bool bReportedMissingScorePickup = false;
};
