// PlayerController del Rally, sin el lobby ni el HUD de la tortuga a pie: crea el HUD del Rally solo en los jugadores
// locales y manda al servidor los cosméticos guardados (las tortugas sentadas en el buggy los pintan desde el
// PlayerState). Los controles (conducir, disparar, enderezar y pedir la reaparición) los pone el buggy o la artillera.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Player/TN_CosmeticsSync.h"
#include "TN_RallyPlayerController.generated.h"

class UTN_RallyHUDWidget;

UCLASS()
class TORTUNABO_API ATN_RallyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ATN_RallyPlayerController();

	/** HUD del Rally de este jugador (solo en el jugador local; nullptr en el resto). Lee calor, munición y tinta del buggy. */
	UFUNCTION(BlueprintPure, Category = "Rally")
	UTN_RallyHUDWidget* GetRallyHUD() const { return RallyHUD; }

	UPROPERTY(EditDefaultsOnly, Category = "Rally")
	TSubclassOf<UTN_RallyHUDWidget> HUDWidgetClass;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Jugador local: manda al servidor el casco, el color, el caparazón y los ojos de su save (TNCosmeticsSync). */
	void SyncCosmeticsToServer();

	/** Validado contra DT_Helmets y DT_Skins del servidor (lo mismo que AMP_GamePlayerController). */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSyncCosmetics(const FTNCosmeticLoadout& Loadout);

	UPROPERTY(Transient)
	TObjectPtr<UTN_RallyHUDWidget> RallyHUD;
};
