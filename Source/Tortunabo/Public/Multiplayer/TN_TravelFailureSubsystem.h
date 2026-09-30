#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TN_TravelFailureSubsystem.generated.h"

class UMP_GameInstance;
class UWorld;

/**
 * @brief Qué pasa cuando un viaje de mapa falla (UEngine::OnTravelFailure: mapa sin cocinar, paquete que falta, URL mala).
 *
 * Sin esto, un ServerTravel fallido dejaba al invitado colgado en la pantalla de carga y sin mensaje (F_gaps_steam N-E).
 * La regla es TNTravel::DecideTravelFailure (tests Tortunabo.Net.TravelFailure):
 *  - El anfitrión, en el primer fallo, vuelve al lobby del que salió con todos (ServerTravel); la pantalla de carga lo dice.
 *  - Un invitado, una partida sin red o un segundo fallo seguido: sesión cerrada y al menú, que enseña el aviso.
 *  - Ya en el menú: el aviso, sin viajar.
 * Si el anfitrión llega al lobby, el aviso del menú se retira (no saldría horas después). Vive aparte de UMP_GameInstance
 * y solo usa su API pública.
 */
UCLASS()
class TORTUNABO_API UTN_TravelFailureSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Solo con UMP_GameInstance (la del juego). */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

	/** Mapa cargado: se reinicia la cuenta de fallos y, si no es el menú, se retira el aviso del fallo. */
	void HandlePostLoadMap(UWorld* LoadedWorld);

	UMP_GameInstance* GetTNGameInstance() const;

	FDelegateHandle TravelFailureHandle;
	FDelegateHandle PostLoadMapHandle;

	/** Fallos de viaje seguidos desde el último mapa cargado. */
	int32 FailureCount = 0;

	/** Hay un aviso de fallo esperando al menú. */
	bool bNoticePending = false;
};
